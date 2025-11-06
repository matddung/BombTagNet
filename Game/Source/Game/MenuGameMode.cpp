#include "MenuGameMode.h"
#include "BombTagPlayerController.h"
#include "BombTagGameInstance.h"
#include "GameModeTravelUtils.h"
#include "ApiClient.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/DateTime.h"
#include "UObject/WeakObjectPtrTemplates.h"
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

#if !UE_BUILD_SHIPPING
    FString DescribeOptionalForLog(const FString& Value, const TCHAR* EmptyLabel = TEXT("<empty>"))
    {
        FString Trimmed = Value;
        Trimmed.TrimStartAndEndInline();

        if (Trimmed.IsEmpty())
        {
            return FString(EmptyLabel);
        }

        return Trimmed;
    }

    FString DescribeTokenForLog(const FString& Token)
    {
        FString Trimmed = Token;
        Trimmed.TrimStartAndEndInline();

        if (Trimmed.IsEmpty())
        {
            return FString(TEXT("<empty>"));
        }

        const int32 Length = Trimmed.Len();
        if (Length <= 12)
        {
            return FString::Printf(TEXT("%s (len=%d)"), *Trimmed, Length);
        }

        return FString::Printf(TEXT("%s...%s (len=%d)"), *Trimmed.Left(6), *Trimmed.Right(4), Length);
    }
#endif

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

    VerifyStartTokenWithBackend(RequestingController, RoomId, StartToken);
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

void AMenuGameMode::VerifyStartTokenWithBackend(ABombTagPlayerController* RequestingController, const FString& RoomId, const FString& StartToken)
{
    if (!RequestingController)
    {
        return;
    }

    UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance());
    if (!GameInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: game instance unavailable."));
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 8"));
        return;
    }

    UApiClient* ApiClient = GameInstance->GetApiClient();
    if (!ApiClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: API client unavailable."));
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 9"));
        return;
    }

    const FString ExpectedRoomId = GameInstance->GetCurrentRoomId();
    const FString PendingRoomId = GameInstance->GetPendingMatchRoomId();
    const FString& RequiredRoomId = !PendingRoomId.IsEmpty() ? PendingRoomId : ExpectedRoomId;
    const FString ExpectedToken = GameInstance->GetPendingMatchStartToken();

#if !UE_BUILD_SHIPPING
    {
        const bool bAnyRoomAccepted = PendingRoomId.IsEmpty() && ExpectedRoomId.IsEmpty();
        const FString RequiredRoomLabel = DescribeOptionalForLog(RequiredRoomId, bAnyRoomAccepted ? TEXT("<any>") : TEXT("<empty>"));
        const FString ProvidedRoomLabel = DescribeOptionalForLog(RoomId, TEXT("<none>"));
        const FString PendingRoomLabel = DescribeOptionalForLog(PendingRoomId, TEXT("<none>"));
        const FString ExpectedRoomLabel = DescribeOptionalForLog(ExpectedRoomId, TEXT("<none>"));
        const FString IncomingTokenLabel = DescribeTokenForLog(StartToken);
        const FString ExpectedTokenLabel = DescribeTokenForLog(ExpectedToken);

        UE_LOG(LogTemp, Log, TEXT("[Match][Server] VerifyStartToken request room=%s pendingRoom=%s expectedRoom=%s requiredRoom=%s startToken=%s expectedToken=%s"),
            *ProvidedRoomLabel,
            *PendingRoomLabel,
            *ExpectedRoomLabel,
            *RequiredRoomLabel,
            *IncomingTokenLabel,
            *ExpectedTokenLabel);
    }
#endif

    if (!RequiredRoomId.IsEmpty() && !RoomId.Equals(RequiredRoomId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: room mismatch (expected=%s provided=%s)."), *RequiredRoomId, *RoomId);
        RequestingController->ClientNotifyMatchStartDenied(TEXT("ROOM_MISMATCH"));
        return;
    }

    if (!ExpectedToken.IsEmpty() && !StartToken.Equals(ExpectedToken, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: token mismatch."));
        RequestingController->ClientNotifyMatchStartDenied(TEXT("TOKEN_MISMATCH"));
        return;
    }

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    FString DedicatedServerId = GameInstance->GetDedicatedServerId();
    if (DedicatedServerId.IsEmpty())
    {
        DedicatedServerId = GameInstance->GetPendingMatchDedicatedServerId();
    }

    if (!DedicatedServerId.IsEmpty())
    {
        Payload->SetStringField(TEXT("dsId"), DedicatedServerId);
    }

    if (!RoomId.IsEmpty())
    {
        Payload->SetStringField(TEXT("roomId"), RoomId);
    }

    const FString MatchIdentifier = ResolveMatchIdentifierForVerification(GameInstance, RoomId);
    if (!MatchIdentifier.IsEmpty())
    {
        Payload->SetStringField(TEXT("matchId"), MatchIdentifier);
    }

    Payload->SetStringField(TEXT("startToken"), StartToken);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    if (!FJsonSerializer::Serialize(Payload, Writer))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: failed to serialize verification payload."));
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 9"));
        return;
    }

    TWeakObjectPtr<ABombTagPlayerController> WeakController(RequestingController);

    FOnApiResponse Response;
    Response.BindLambda([this, WeakController, RoomId, StartToken](bool bOk, const FString& BodyOrError)
        {
            HandleVerifyStartTokenResponse(WeakController, RoomId, StartToken, bOk, BodyOrError);
        });

    ApiClient->PostJson(TEXT("/ds/matches/verify-start"), Content, MoveTemp(Response));
}

void AMenuGameMode::HandleVerifyStartTokenResponse(TWeakObjectPtr<ABombTagPlayerController> RequestingController, const FString& RoomId, const FString& StartToken, bool bOk, const FString& BodyOrError)
{
    ABombTagPlayerController* Controller = RequestingController.Get();
    if (!Controller)
    {
        return;
    }

    UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance());
    if (!GameInstance)
    {
        Controller->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 8"));
        return;
    }

    auto Reject = [Controller](const FString& Code)
        {
            const FString& CodeToSend = Code.IsEmpty() ? FString(TEXT("MATCH_START_DENIED 6")) : Code;
            Controller->ClientNotifyMatchStartDenied(CodeToSend);
        };

    if (!bOk)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Backend verification HTTP error: %s"), *BodyOrError);
        Reject(TEXT("MATCH_START_DENIED 6"));
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Backend verification returned invalid JSON."));
        Reject(TEXT("MATCH_START_DENIED 9"));
        return;
    }

    bool bResponseSuccess = false;
    RootObject->TryGetBoolField(TEXT("success"), bResponseSuccess);

    FString ErrorCode;
    RootObject->TryGetStringField(TEXT("error"), ErrorCode);

    FString ResponseRoomId;
    RootObject->TryGetStringField(TEXT("roomId"), ResponseRoomId);
    FString ResponseMatchId;
    RootObject->TryGetStringField(TEXT("matchId"), ResponseMatchId);
    FString ResponseDedicatedServerId;
    RootObject->TryGetStringField(TEXT("dedicatedServerId"), ResponseDedicatedServerId);
    FString ResponseExpiresAt;
    RootObject->TryGetStringField(TEXT("expiresAt"), ResponseExpiresAt);

    if (!bResponseSuccess)
    {
        const FString DeniedCode = ErrorCode.IsEmpty() ? FString(TEXT("MATCH_START_DENIED 9")) : ErrorCode;
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Backend rejected start token: %s"), *DeniedCode);
        Reject(DeniedCode);
        return;
    }

#if !UE_BUILD_SHIPPING
    {
        const FString PayloadExpirationLabel = DescribeOptionalForLog(ResponseExpiresAt, TEXT("<invalid>"));
        UE_LOG(LogTemp, Log, TEXT("[Match][Server] Backend verified token room=%s match=%s dsId=%s expiresAt=%s"),
            *DescribeOptionalForLog(ResponseRoomId, TEXT("<none>")),
            *DescribeOptionalForLog(ResponseMatchId, TEXT("<none>")),
            *DescribeOptionalForLog(ResponseDedicatedServerId, TEXT("<none>")),
            *PayloadExpirationLabel);
    }
#endif

    const FString ExpectedRoomId = GameInstance->GetCurrentRoomId();
    const FString PendingRoomId = GameInstance->GetPendingMatchRoomId();
    const FString& RequiredRoomId = !PendingRoomId.IsEmpty() ? PendingRoomId : ExpectedRoomId;
    const FString ExpectedToken = GameInstance->GetPendingMatchStartToken();

    if (!RequiredRoomId.IsEmpty() && !RoomId.Equals(RequiredRoomId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied after verify: room mismatch."));
        Reject(TEXT("ROOM_MISMATCH"));
        return;
    }

    if (!ExpectedToken.IsEmpty() && !StartToken.Equals(ExpectedToken, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied after verify: token changed."));
        Reject(TEXT("TOKEN_MISMATCH"));
        return;
    }

    if (!RequiredRoomId.IsEmpty() && !ResponseRoomId.Equals(RequiredRoomId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Verified token room mismatch: expected=%s response=%s"), *RequiredRoomId, *ResponseRoomId);
        Reject(TEXT("MATCH_START_DENIED 10"));
        return;
    }

    const FString ExpectedMatchId = ResolveMatchIdentifierForVerification(GameInstance, RoomId);
    if (!ExpectedMatchId.IsEmpty() && !ResponseMatchId.Equals(ExpectedMatchId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Verified token match mismatch: expected=%s response=%s"), *ExpectedMatchId, *ResponseMatchId);
        Reject(TEXT("MATCH_START_DENIED 11"));
        return;
    }

    FString RequiredDedicatedServerId = GameInstance->GetPendingMatchDedicatedServerId();
    if (RequiredDedicatedServerId.IsEmpty())
    {
        RequiredDedicatedServerId = GameInstance->GetDedicatedServerId();
    }

    if (!RequiredDedicatedServerId.IsEmpty() && !ResponseDedicatedServerId.Equals(RequiredDedicatedServerId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Verified token DS mismatch: expected=%s response=%s"), *RequiredDedicatedServerId, *ResponseDedicatedServerId);
        Reject(TEXT("MATCH_START_DENIED 12"));
        return;
    }

    if (!ResponseExpiresAt.IsEmpty())
    {
        FDateTime ExpirationUtc;
        if (FDateTime::ParseIso8601(*ResponseExpiresAt, ExpirationUtc))
        {
            if (ExpirationUtc <= FDateTime::UtcNow())
            {
                UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Verified token already expired at %s"), *ResponseExpiresAt);
                Reject(TEXT("MATCH_START_DENIED 14"));
                return;
            }
        }
    }

    FString ExpectedHost = GameInstance->GetPendingMatchServerAddress();
    int32 ExpectedPort = GameInstance->GetPendingMatchServerPort();
    FString TravelURL = GameInstance->GetPendingMatchTravelURL();

    if (ExpectedHost.IsEmpty() || ExpectedPort <= 0 || TravelURL.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: missing dedicated server endpoint data (host=%s port=%d url=%s)."), *ExpectedHost, ExpectedPort, *TravelURL);
        Reject(TEXT("MATCH_START_DENIED 7"));
        return;
    }

    int32 ConfiguredPort = GameInstance->GetDedicatedServerGamePort();
    if (ConfiguredPort <= 0)
    {
        ConfiguredPort = ExpectedPort;
    }

    const int32 LocalBoundPort = ResolveLocalServerPort(GetWorld());
    if (ConfiguredPort > 0 && LocalBoundPort > 0 && ConfiguredPort != LocalBoundPort)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Dedicated server port mismatch: expected %d actual %d"), ConfiguredPort, LocalBoundPort);
        Reject(TEXT("MATCH_START_DENIED 15"));
        return;
    }

    const FString EndpointLabel = BuildEndpointLabel(ExpectedHost, ExpectedPort);
    UE_LOG(LogTemp, Log, TEXT("[Match] Host %s approved match start for room %s. Directing clients to %s."), *GetNameSafe(Controller), *RoomId, *EndpointLabel);

    SendClientsToMatch(TravelURL);
}

FString AMenuGameMode::ResolveMatchIdentifierForVerification(const UBombTagGameInstance* GameInstance, const FString& RoomId) const
{
    if (!GameInstance)
    {
        return FString();
    }

    const FString PendingRoomId = GameInstance->GetPendingMatchRoomId();
    if (!PendingRoomId.IsEmpty())
    {
        return PendingRoomId;
    }

    const FString CurrentRoomId = GameInstance->GetCurrentRoomId();
    if (!CurrentRoomId.IsEmpty())
    {
        return CurrentRoomId;
    }

    return RoomId;
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