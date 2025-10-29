#include "MenuGameMode.h"
#include "BombTagPlayerController.h"
#include "BombTagGameInstance.h"
#include "GameModeTravelUtils.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "Containers/StringConv.h"
#include "Misc/DateTime.h"

namespace
{
    FString BuildEndpointLabel(const FString& Host, int32 Port)
    {
        if (Host.IsEmpty())
        {
            return FString(TEXT("UNSPECIFIED"));
        }

        return Port > 0 ? FString::Printf(TEXT("%s:%d"), *Host, Port) : Host;
    }

    struct FBombTagMatchStartTokenPayload
    {
        FString Version;
        FString DedicatedServerId;
        FString RoomId;
        FString MatchId;
        FDateTime ExpiresAt;
    };

    bool ParseMatchStartToken(const FString& Token, const FString& Secret, FBombTagMatchStartTokenPayload& OutPayload, FString& OutError)
    {
        if (Token.IsEmpty())
        {
            OutError = TEXT("TOKEN_EMPTY");
            return false;
        }

        if (Secret.IsEmpty())
        {
            OutError = TEXT("TOKEN_SECRET_UNAVAILABLE");
            return false;
        }

        TArray<FString> Segments;
        Token.ParseIntoArray(Segments, TEXT("."), true);
        if (Segments.Num() != 3)
        {
            OutError = TEXT("TOKEN_FORMAT");
            return false;
        }

        const FString& Header = Segments[0];
        const FString& PayloadSegment = Segments[1];
        const FString& SignatureSegment = Segments[2];

        if (!Header.Equals(TEXT("v1"), ESearchCase::IgnoreCase))
        {
            OutError = TEXT("TOKEN_VERSION");
            return false;
        }

        const FString Material = FString::Printf(TEXT("%s.%s.%s"), *Header, *PayloadSegment, *Secret);
        const FString ExpectedSignature = FMD5::HashAnsiString(*Material);
        if (!SignatureSegment.Equals(ExpectedSignature, ESearchCase::IgnoreCase))
        {
            OutError = TEXT("TOKEN_SIGNATURE");
            return false;
        }

        TArray<uint8> PayloadBytes;
        if (!FBase64::Decode(PayloadSegment, PayloadBytes))
        {
            OutError = TEXT("TOKEN_DECODE");
            return false;
        }

        FUTF8ToTCHAR Converter(reinterpret_cast<const char*>(PayloadBytes.GetData()), PayloadBytes.Num());
        FString PayloadString(Converter.Get(), Converter.Length());

        TSharedPtr<FJsonObject> PayloadObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadString);
        if (!FJsonSerializer::Deserialize(Reader, PayloadObject) || !PayloadObject.IsValid())
        {
            OutError = TEXT("TOKEN_JSON");
            return false;
        }

        FString DsId;
        FString RoomId;
        FString MatchId;
        FString ExpirationText;
        if (!PayloadObject->TryGetStringField(TEXT("dsId"), DsId) ||
            !PayloadObject->TryGetStringField(TEXT("roomId"), RoomId) ||
            !PayloadObject->TryGetStringField(TEXT("matchId"), MatchId) ||
            !PayloadObject->TryGetStringField(TEXT("exp"), ExpirationText))
        {
            OutError = TEXT("TOKEN_FIELDS");
            return false;
        }

        DsId.TrimStartAndEndInline();
        RoomId.TrimStartAndEndInline();
        MatchId.TrimStartAndEndInline();
        ExpirationText.TrimStartAndEndInline();
        if (DsId.IsEmpty() || RoomId.IsEmpty() || MatchId.IsEmpty() || ExpirationText.IsEmpty())
        {
            OutError = TEXT("TOKEN_FIELDS");
            return false;
        }

        FDateTime ExpirationValue;
        if (!FDateTime::ParseIso8601(*ExpirationText, ExpirationValue))
        {
            OutError = TEXT("TOKEN_EXP_PARSE");
            return false;
        }

        OutPayload.Version = Header;
        OutPayload.DedicatedServerId = DsId;
        OutPayload.RoomId = RoomId;
        OutPayload.MatchId = MatchId;
        OutPayload.ExpiresAt = ExpirationValue;
        return true;
    }

    int32 ResolveLocalServerPort(const UWorld* World)
    {
        if (!World)
        {
            return 0;
        }

        const int32 WorldPort = World->URL.Port;
        if (WorldPort > 0)
        {
            return WorldPort;
        }

        return 0;
    }
}

AMenuGameMode::AMenuGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ABombTagPlayerController::StaticClass();

#if !UE_SERVER
    static ConstructorHelpers::FClassFinder<UUserWidget> MenuBPClass(TEXT("/Game/UI/WBP_MainMenu"));
    if (MenuBPClass.Succeeded())
    {
        MenuClass = MenuBPClass.Class;
    }
#endif
}

void AMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(this, true);
    const FString OwnerId = BombTag::GameMode::ResolveHostId(Cast<UBombTagGameInstance>(GetGameInstance()));
    UE_LOG(LogTemp, Log, TEXT("[Match] CurrentMap=%s GameMode=%s Seamless=%s Owner=%s"), *CurrentMap, *GetClass()->GetName(), bUseSeamlessTravel ? TEXT("true") : TEXT("false"), *OwnerId);
}

void AMenuGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

#if !UE_SERVER
    if (MenuClass)
    {
        if (ABombTagPlayerController* BTPC = Cast<ABombTagPlayerController>(NewPlayer))
        {
            BTPC->ClientShowMainMenu(MenuClass);
        }
    }
#endif
}

void AMenuGameMode::HandleStartMatchRequest(ABombTagPlayerController* RequestingController, const FString& RoomId, const FString& StartToken)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!RequestingController)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Null controller attempted to request match start."));
        return;
    }

    if (RoomId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start request missing room id."));
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 4"));
        return;
    }

    if (!HasHostAuthority(RequestingController))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Controller %s lacks host permissions for match start."), *GetNameSafe(RequestingController));
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 5"));
        return;
    }

    FString VerificationError;
    if (!VerifyWithBackend(RoomId, StartToken, VerificationError))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Backend verification failed: %s"), *VerificationError);
        const FString DeniedCode = VerificationError.IsEmpty() ? FString(TEXT("MATCH_START_DENIED 6")) : VerificationError;
        RequestingController->ClientNotifyMatchStartDenied(DeniedCode);
        return;
    }

    FString ExpectedHost;
    int32 ExpectedPort = 0;
    FString TravelURL;
    if (const UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance()))
    {
        ExpectedHost = GameInstance->GetPendingMatchHostAddress();
        ExpectedPort = GameInstance->GetPendingMatchHostPort();
        TravelURL = GameInstance->GetPendingMatchTravelURL();
    }

    if (ExpectedHost.IsEmpty() || ExpectedPort <= 0 || TravelURL.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: missing dedicated server endpoint data (host=%s port=%d url=%s)."), *ExpectedHost, ExpectedPort, *TravelURL);
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 7"));
        return;
    }

    const FString EndpointLabel = BuildEndpointLabel(ExpectedHost, ExpectedPort);
    UE_LOG(LogTemp, Log, TEXT("[Match] Host %s approved match start for room %s. Directing clients to %s."), *GetNameSafe(RequestingController), *RoomId, *EndpointLabel);

    SendClientsToMatch(TravelURL);
}

void AMenuGameMode::SendClientsToMatch(const FString& TravelURL)
{
    if (TravelURL.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] SendClientsToMatch skipped: travel url is empty."));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] SendClientsToMatch skipped: world not available."));
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            UE_LOG(LogTemp, Log, TEXT("[Match] Instructing %s to travel to %s."), *GetNameSafe(PC), *TravelURL);
            PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
        }
    }
}

bool AMenuGameMode::HasHostAuthority(const ABombTagPlayerController* RequestingController) const
{
    if (!RequestingController)
    {
        return false;
    }

    if (RequestingController->IsLocalController())
    {
        return true;
    }

    if (const UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance()))
    {
        const FString HostPlayerId = GameInstance->GetPendingMatchHostPlayerId();
        if (!HostPlayerId.IsEmpty())
        {
            if (const APlayerState* PlayerState = RequestingController->PlayerState)
            {
                if (PlayerState->GetPlayerName().Equals(HostPlayerId, ESearchCase::IgnoreCase))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool AMenuGameMode::VerifyWithBackend(const FString& RoomId, const FString& StartToken, FString& OutError) const
{
    const UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance());
    if (!GameInstance)
    {
        OutError = TEXT("MATCH_START_DENIED 8");
        return false;
    }

    const FString ExpectedRoomId = GameInstance->GetCurrentRoomId();
    const FString PendingRoomId = GameInstance->GetPendingMatchRoomId();
    const FString& RequiredId = !PendingRoomId.IsEmpty() ? PendingRoomId : ExpectedRoomId;
    if (!RequiredId.IsEmpty() && !RoomId.Equals(RequiredId, ESearchCase::CaseSensitive))
    {
        OutError = TEXT("ROOM_MISMATCH");
        return false;
    }

    const FString ExpectedToken = GameInstance->GetPendingMatchStartToken();
    if (!ExpectedToken.IsEmpty() && !StartToken.Equals(ExpectedToken, ESearchCase::CaseSensitive))
    {
        OutError = TEXT("TOKEN_MISMATCH");
        return false;
    }

    FBombTagMatchStartTokenPayload TokenPayload;
    FString TokenParseError;
    const FString TokenSecret = GameInstance->GetMatchStartTokenSecret();
    if (!ParseMatchStartToken(StartToken, TokenSecret, TokenPayload, TokenParseError))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Start token verification failed: %s"), *TokenParseError);
        OutError = TEXT("MATCH_START_DENIED 8");
        return false;
    }

    if (!RequiredId.IsEmpty() && !TokenPayload.RoomId.Equals(RequiredId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Token room mismatch: expected %s but token has %s"), *RequiredId, *TokenPayload.RoomId);
        OutError = TEXT("MATCH_START_DENIED 8");
        return false;
    }

    const FString ExpectedMatchId = !PendingRoomId.IsEmpty() ? PendingRoomId : RoomId;
    if (!ExpectedMatchId.IsEmpty() && !TokenPayload.MatchId.Equals(ExpectedMatchId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Token match mismatch: expected %s but token has %s"), *ExpectedMatchId, *TokenPayload.MatchId);
        OutError = TEXT("MATCH_START_DENIED 8");
        return false;
    }

    const FString PendingDedicatedServerId = GameInstance->GetPendingMatchDedicatedServerId();
    if (!PendingDedicatedServerId.IsEmpty() && !TokenPayload.DedicatedServerId.Equals(PendingDedicatedServerId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Token DS mismatch: pending=%s token=%s"), *PendingDedicatedServerId, *TokenPayload.DedicatedServerId);
        OutError = TEXT("MATCH_START_DENIED 8");
        return false;
    }

    const FString LocalDedicatedServerId = GameInstance->GetDedicatedServerId();
    if (!LocalDedicatedServerId.IsEmpty() && !TokenPayload.DedicatedServerId.Equals(LocalDedicatedServerId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Local DS id %s does not match token %s"), *LocalDedicatedServerId, *TokenPayload.DedicatedServerId);
        OutError = TEXT("MATCH_START_DENIED 8");
        return false;
    }

    const FDateTime NowUtc = FDateTime::UtcNow();
    if (TokenPayload.ExpiresAt <= NowUtc)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Start token expired at %s (now=%s)"), *TokenPayload.ExpiresAt.ToIso8601(), *NowUtc.ToIso8601());
        OutError = TEXT("MATCH_START_DENIED 8");
        return false;
    }

    const int32 LocalBoundPort = ResolveLocalServerPort(GetWorld());
    int32 ExpectedPort = GameInstance->GetDedicatedServerGamePort();
    if (ExpectedPort <= 0)
    {
        ExpectedPort = GameInstance->GetPendingMatchHostPort();
    }

    if (ExpectedPort > 0 && LocalBoundPort > 0 && ExpectedPort != LocalBoundPort)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Dedicated server port mismatch: expected %d actual %d"), ExpectedPort, LocalBoundPort);
        OutError = TEXT("MATCH_START_DENIED 8");
        return false;
    }

    OutError.Reset();
    return true;
}