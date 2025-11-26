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
        return;
    }

    if (RoomId.IsEmpty())
    {
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 4"));
        return;
    }

    if (UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance()))
    {
        const FString ExpectedRoomId = GameInstance->GetCurrentRoomId();
        if (!ExpectedRoomId.IsEmpty() && !RoomId.Equals(ExpectedRoomId, ESearchCase::CaseSensitive))
        {
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
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
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
        RequestingController->ClientDebugVerifyStartResult(TEXT("verifyStart precheck gameInstanceMissing"), false, FString(), FString(), FString());
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 8"));
        return;
    }

    UApiClient* ApiClient = GameInstance->GetApiClient();
    if (!ApiClient)
    {
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
    }
#endif

    if (!RequiredRoomId.IsEmpty() && !RoomId.Equals(RequiredRoomId, ESearchCase::CaseSensitive))
    {
        RequestingController->ClientDebugVerifyStartResult(TEXT("verifyStart precheck roomMismatch"), false, RoomId, FString(), FString());
        RequestingController->ClientNotifyMatchStartDenied(TEXT("ROOM_MISMATCH"));
        return;
    }

    if (!ExpectedToken.IsEmpty() && !StartToken.Equals(ExpectedToken, ESearchCase::CaseSensitive))
    {
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
        Controller->ClientDebugVerifyStartResult(TEXT("verifyStart httpOk=false"), false, FString(), FString(), FString());
        Reject(TEXT("MATCH_START_DENIED 6"));
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
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
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(DeniedCode);
        return;
    }

#if !UE_BUILD_SHIPPING
    {
        const FString PayloadExpirationLabel = BombTag::Logging::DescribeOptionalForLog(ResponseExpiresAt, TEXT("<invalid>"));
    }
#endif

    const FString RequiredRoomId = GameInstance->GetCurrentRoomId();
    const FString ExpectedToken = GameInstance->GetPendingMatchStartToken();
    const FString PendingMatchId = GameInstance->GetPendingMatchId();

    if (!RequiredRoomId.IsEmpty() && !RoomId.Equals(RequiredRoomId, ESearchCase::CaseSensitive))
    {
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("ROOM_MISMATCH"));
        return;
    }

    if (!ExpectedToken.IsEmpty() && !StartToken.Equals(ExpectedToken, ESearchCase::CaseSensitive))
    {
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("TOKEN_MISMATCH"));
        return;
    }

    if (!RequiredRoomId.IsEmpty() && !ResponseRoomId.Equals(RequiredRoomId, ESearchCase::CaseSensitive))
    {
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("MATCH_START_DENIED 10"));
        return;
    }

    if (!PendingMatchId.IsEmpty() && !ResponseMatchId.IsEmpty() && !ResponseMatchId.Equals(PendingMatchId, ESearchCase::CaseSensitive))
    {
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
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("MATCH_START_DENIED 7"));
        return;
    }

    const int32 ExpectedPortForLocalCheck = (ConfiguredPort > 0) ? ConfiguredPort : ProvidedPort;

    if (ExpectedPortForLocalCheck > 0 && LocalBoundPort > 0 && ExpectedPortForLocalCheck != LocalBoundPort)
    {
        Controller->ClientDebugVerifyStartResult(VerificationSummary, false, ResponseRoomId, ResponseMatchId, ResponseDedicatedServerId);
        Reject(TEXT("MATCH_START_DENIED 15"));
        return;
    }

    const FString EndpointLabel = BuildEndpointLabel(ProvidedHost, ProvidedPort);
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