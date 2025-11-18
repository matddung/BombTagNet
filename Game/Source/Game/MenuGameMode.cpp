#include "MenuGameMode.h"
#include "BombTagPlayerController.h"
#include "BombTagGameInstance.h"
#include "GameModeTravelUtils.h"
#include "ApiClient.h"
#include "Game.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
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
    const FString DedicatedServerLabel = BombTag::GameMode::ResolveDedicatedServerLabel(Cast<UBombTagGameInstance>(GetGameInstance()));
    UE_LOG(LogTemp, Log, TEXT("[Match] CurrentMap=%s GameMode=%s Seamless=%s DedicatedServer=%s"), *CurrentMap, *GetClass()->GetName(), bUseSeamlessTravel ? TEXT("true") : TEXT("false"), *DedicatedServerLabel);
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

void AMenuGameMode::HandleStartMatchRequest(ABombTagPlayerController* RequestingController, const FString& RoomId, const FString& StartToken, const FString& DedicatedServerAddress, int32 DedicatedServerPort, const FString& TravelURL)
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

    if (UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance()))
    {
        const FString ExpectedRoomId = GameInstance->GetCurrentRoomId();
        if (!ExpectedRoomId.IsEmpty() && !RoomId.Equals(ExpectedRoomId, ESearchCase::CaseSensitive))
        {
            UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: room mismatch before verification (expected=%s provided=%s)."), *ExpectedRoomId, *RoomId);
            RequestingController->ClientNotifyMatchStartDenied(TEXT("ROOM_MISMATCH"));
            return;
        }
    }

    VerifyStartTokenWithBackend(RequestingController, RoomId, StartToken, DedicatedServerAddress, DedicatedServerPort, TravelURL);
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

void AMenuGameMode::VerifyStartTokenWithBackend(ABombTagPlayerController* RequestingController, const FString& RoomId, const FString& StartToken, const FString& DedicatedServerAddress, int32 DedicatedServerPort, const FString& TravelURL)
{
    if (!RequestingController)
    {
        return;
    }

    UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance());
    if (!GameInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: game instance unavailable."));
        RequestingController->ClientDebugVerifyStartResult(TEXT("verifyStart precheck gameInstanceMissing"), false, FString(), FString(), FString());
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 8"));
        return;
    }

    UApiClient* ApiClient = GameInstance->GetApiClient();
    if (!ApiClient)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: API client unavailable."));
        RequestingController->ClientDebugVerifyStartResult(TEXT("verifyStart precheck apiMissing"), false, FString(), FString(), FString());
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 9"));
        return;
    }

    const FString RequiredRoomId = GameInstance->GetCurrentRoomId();
    const FString ExpectedToken = GameInstance->GetPendingMatchStartToken();
    const FString PendingMatchId = GameInstance->GetPendingMatchId();

#if !UE_BUILD_SHIPPING
    {
        const bool bAnyRoomAccepted = RequiredRoomId.IsEmpty();
        const FString RequiredRoomLabel = BombTag::Logging::DescribeOptionalForLog(RequiredRoomId, bAnyRoomAccepted ? TEXT("<any>") : TEXT("<empty>"));
        const FString ProvidedRoomLabel = BombTag::Logging::DescribeOptionalForLog(RoomId, TEXT("<none>"));
        const FString PendingMatchLabel = BombTag::Logging::DescribeOptionalForLog(PendingMatchId, TEXT("<none>"));
        const FString IncomingTokenLabel = BombTag::Logging::DescribeTokenForLog(StartToken);
        const FString ExpectedTokenLabel = BombTag::Logging::DescribeTokenForLog(ExpectedToken);

        UE_LOG(LogTemp, Log, TEXT("[Match][Server] VerifyStartToken request room=%s pendingMatch=%s requiredRoom=%s startToken=%s expectedToken=%s"),
            *ProvidedRoomLabel,
            *PendingMatchLabel,
            *RequiredRoomLabel,
            *IncomingTokenLabel,
            *ExpectedTokenLabel);
    }
#endif

    if (!RequiredRoomId.IsEmpty() && !RoomId.Equals(RequiredRoomId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: room mismatch (expected=%s provided=%s)."), *RequiredRoomId, *RoomId);
        RequestingController->ClientDebugVerifyStartResult(TEXT("verifyStart precheck roomMismatch"), false, RoomId, FString(), FString());
        RequestingController->ClientNotifyMatchStartDenied(TEXT("ROOM_MISMATCH"));
        return;
    }

    if (!ExpectedToken.IsEmpty() && !StartToken.Equals(ExpectedToken, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: token mismatch."));
        RequestingController->ClientDebugVerifyStartResult(TEXT("verifyStart precheck tokenMismatch"), false, RoomId, FString(), FString());
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
        RequestingController->ClientDebugVerifyStartResult(TEXT("verifyStart precheck serializationFailed"), false, RoomId, FString(), FString());
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 9"));
        return;
    }

    TWeakObjectPtr<ABombTagPlayerController> WeakController(RequestingController);

    FOnApiResponse Response;
    Response.BindLambda([this, WeakController, RoomId, StartToken, DedicatedServerAddress, DedicatedServerPort, TravelURL](bool bOk, const FString& BodyOrError)
        {
            HandleVerifyStartTokenResponse(WeakController, RoomId, StartToken, DedicatedServerAddress, DedicatedServerPort, TravelURL, bOk, BodyOrError);
        });

    ApiClient->PostJson(TEXT("/ds/matches/verify-start"), Content, MoveTemp(Response));
}

void AMenuGameMode::HandleVerifyStartTokenResponse(TWeakObjectPtr<ABombTagPlayerController> RequestingController, const FString& RoomId, const FString& StartToken, const FString& DedicatedServerAddress, int32 DedicatedServerPort, const FString& TravelURL, bool bOk, const FString& BodyOrError)
{
    ABombTagPlayerController* Controller = RequestingController.Get();
    if (!Controller)
    {
        return;
    }

    UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance());
    if (!GameInstance)
    {
        Controller->ClientDebugVerifyStartResult(TEXT("verifyStart gameInstanceMissing"), false, FString(), FString(), FString());
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
        Controller->ClientDebugVerifyStartResult(TEXT("verifyStart httpOk=false"), false, FString(), FString(), FString());
        Reject(TEXT("MATCH_START_DENIED 6"));
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Backend verification returned invalid JSON."));
        Controller->ClientDebugVerifyStartResult(TEXT("verifyStart jsonParseError"), false, FString(), FString(), FString());
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

    const FString VerificationSummary = FString::Printf(TEXT("verifyStart ok=%s"), bResponseSuccess ? TEXT("true") : TEXT("false"));

    if (!bResponseSuccess)
    {
        const FString DeniedCode = ErrorCode.IsEmpty() ? FString(TEXT("MATCH_START_DENIED 9")) : ErrorCode;
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Backend rejected start token: %s"), *DeniedCode);
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(DeniedCode);
        return;
    }

#if !UE_BUILD_SHIPPING
    {
        const FString PayloadExpirationLabel = BombTag::Logging::DescribeOptionalForLog(ResponseExpiresAt, TEXT("<invalid>"));
        UE_LOG(LogTemp, Log, TEXT("[Match][Server] Backend verified token room=%s match=%s dsId=%s expiresAt=%s"),
            *BombTag::Logging::DescribeOptionalForLog(ResponseRoomId, TEXT("<none>")),
            *BombTag::Logging::DescribeOptionalForLog(ResponseMatchId, TEXT("<none>")),
            *BombTag::Logging::DescribeOptionalForLog(ResponseDedicatedServerId, TEXT("<none>")),
            *PayloadExpirationLabel);
    }
#endif

    const FString RequiredRoomId = GameInstance->GetCurrentRoomId();
    const FString ExpectedToken = GameInstance->GetPendingMatchStartToken();
    const FString PendingMatchId = GameInstance->GetPendingMatchId();

    if (!RequiredRoomId.IsEmpty() && !RoomId.Equals(RequiredRoomId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied after verify: room mismatch."));
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("ROOM_MISMATCH"));
        return;
    }

    if (!ExpectedToken.IsEmpty() && !StartToken.Equals(ExpectedToken, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied after verify: token changed."));
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("TOKEN_MISMATCH"));
        return;
    }

    if (!RequiredRoomId.IsEmpty() && !ResponseRoomId.Equals(RequiredRoomId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Verified token room mismatch: expected=%s response=%s"), *RequiredRoomId, *ResponseRoomId);
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("MATCH_START_DENIED 10"));
        return;
    }

    if (!PendingMatchId.IsEmpty() && !ResponseMatchId.IsEmpty() && !ResponseMatchId.Equals(PendingMatchId, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Verified token match mismatch: expected=%s response=%s"), *PendingMatchId, *ResponseMatchId);
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
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
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
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
                Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
                Reject(TEXT("MATCH_START_DENIED 14"));
                return;
            }
        }
    }

    FString ProvidedHost = DedicatedServerAddress;
    int32 ProvidedPort = DedicatedServerPort;
    FString ProvidedTravelURL = TravelURL;

    const FString StoredHost = GameInstance->GetPendingMatchServerAddress();
    const int32 StoredPort = GameInstance->GetPendingMatchServerPort();
    const FString StoredTravelURL = GameInstance->GetPendingMatchTravelURL();

    if (!DedicatedServerAddress.IsEmpty() && !StoredHost.IsEmpty() && !DedicatedServerAddress.Equals(StoredHost, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] ServerRequestStartMatch address mismatch: provided=%s stored=%s"), *DedicatedServerAddress, *StoredHost);
    }

    if (DedicatedServerPort > 0 && StoredPort > 0 && DedicatedServerPort != StoredPort)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] ServerRequestStartMatch port mismatch: provided=%d stored=%d"), DedicatedServerPort, StoredPort);
    }

    if (!TravelURL.IsEmpty() && !StoredTravelURL.IsEmpty() && !TravelURL.Equals(StoredTravelURL, ESearchCase::CaseSensitive))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] ServerRequestStartMatch travel mismatch: provided=%s stored=%s"), *TravelURL, *StoredTravelURL);
    }

    if (ProvidedHost.IsEmpty())
    {
        ProvidedHost = StoredHost;
    }

    if (ProvidedPort <= 0)
    {
        ProvidedPort = StoredPort;
    }

    if (ProvidedTravelURL.IsEmpty())
    {
        ProvidedTravelURL = StoredTravelURL;
    }

    const int32 ConfiguredPort = GameInstance->GetDedicatedServerGamePort();
    const int32 LocalBoundPort = ResolveLocalServerPort(GetWorld());

    if (ProvidedPort <= 0)
    {
        if (ConfiguredPort > 0)
        {
            ProvidedPort = ConfiguredPort;
        }
        else if (LocalBoundPort > 0)
        {
            ProvidedPort = LocalBoundPort;
        }
    }

    if (ProvidedHost.IsEmpty() || ProvidedPort <= 0 || ProvidedTravelURL.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: missing dedicated server endpoint data (address=%s port=%d url=%s)."), *ProvidedHost, ProvidedPort, *ProvidedTravelURL);
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("MATCH_START_DENIED 7"));
        return;
    }

    const int32 ExpectedPortForLocalCheck = (ConfiguredPort > 0) ? ConfiguredPort : ProvidedPort;

    if (ExpectedPortForLocalCheck > 0 && LocalBoundPort > 0 && ExpectedPortForLocalCheck != LocalBoundPort)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Dedicated server port mismatch: expected %d actual %d"), ExpectedPortForLocalCheck, LocalBoundPort);
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("MATCH_START_DENIED 15"));
        return;
    }

    const FString EndpointLabel = BuildEndpointLabel(ProvidedHost, ProvidedPort);
    UE_LOG(LogTemp, Log, TEXT("[Match] Match start approved for room %s. Directing clients to %s."), *GetNameSafe(Controller), *RoomId, *EndpointLabel);
    Controller->ClientDebugVerifyStartResult(VerificationSummary, true, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);

    SendClientsToMatch(ProvidedTravelURL);
}

FString AMenuGameMode::ResolveMatchIdentifierForVerification(const UBombTagGameInstance* GameInstance, const FString& RoomId) const
{
    (void)RoomId;

    if (!GameInstance)
    {
        return FString();
    }

    return GameInstance->GetPendingMatchId();
}