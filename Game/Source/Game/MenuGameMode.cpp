#include "MenuGameMode.h"
#include "BombTagPlayerController.h"
#include "BombTagGameInstance.h"
#include "GameModeTravelUtils.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"

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
    // 로비 진입 시 현재 맵/게임모드/시즌리스 설정을 로그로 남긴다.
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
        // 컨트롤러가 유효하지 않다면 더 진행하지 않는다.
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Null controller attempted to request match start."));
        return;
    }

    if (RoomId.IsEmpty())
    {
        // 방 식별자가 없으면 요청을 거부한다.
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start request missing room id."));
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 4"));
        return;
    }

    if (!HasHostAuthority(RequestingController))
    {
        // 방장이 아닌 경우 서버에서 거부하고 경고 로그를 남긴다.
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Controller %s lacks host permissions for match start."), *GetNameSafe(RequestingController));
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 5"));
        return;
    }

    FString VerificationError;
    if (!VerifyWithBackend(RoomId, StartToken, VerificationError))
    {
        // 백엔드 검증이 실패하면 세부 코드와 함께 거부한다.
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Backend verification failed: %s"), *VerificationError);
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 6"));
        return;
    }

    FString ExpectedHost;
    int32 ExpectedPort = 0;
    if (const UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance()))
    {
        ExpectedHost = GameInstance->GetPendingMatchHostAddress();
        ExpectedPort = GameInstance->GetPendingMatchHostPort();
    }

    if (ExpectedHost.IsEmpty() || ExpectedPort <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: missing host endpoint data (host=%s port=%d)."), *ExpectedHost, ExpectedPort);
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 7"));
        return;
    }

    FString ActualEndpoint;
    if (!ValidateServerInstance(ExpectedHost, ExpectedPort, ActualEndpoint))
    {
        const FString ExpectedEndpoint = BuildEndpointLabel(ExpectedHost, ExpectedPort);
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: server endpoint mismatch expected=%s actual=%s."), *ExpectedEndpoint, *ActualEndpoint);
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 8"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Match] Host %s approved match start for room %s."), *GetNameSafe(RequestingController), *RoomId);
    BroadcastServerEndpointAudit(ExpectedHost, ExpectedPort);
    StartMatchTravel();
}

void AMenuGameMode::StartMatchTravel()
{
    if (MatchTravelURL.IsEmpty())
    {
        // 설정이 빠져 있으면 서버 트래블을 시도하지 않는다.
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] MatchTravelURL is empty; cannot travel to match map."));
        return;
    }

    FString MapName;
    FString GameModePath;
    BombTag::GameMode::ExtractTravelTargets(MatchTravelURL, MapName, GameModePath);
    bUseSeamlessTravel = true;

    const FString OwnerId = BombTag::GameMode::ResolveHostId(Cast<UBombTagGameInstance>(GetGameInstance()));
    // 서버 전용 트래블 실행과 함께 표준화된 로그를 남긴다.
    UE_LOG(LogTemp, Log, TEXT("[Match] ServerTravel to %s (Map=%s GameMode=%s Owner=%s Seamless=%s)"), *MatchTravelURL, *MapName, *GameModePath, *OwnerId, bUseSeamlessTravel ? TEXT("true") : TEXT("false"));
    GetWorld()->ServerTravel(MatchTravelURL, true);
}

bool AMenuGameMode::HasHostAuthority(const ABombTagPlayerController* RequestingController) const
{
    if (!RequestingController)
    {
        return false;
    }

    if (RequestingController->IsLocalController())
    {
        // 데디케이티드 서버 환경에서는 호스트가 로컬 컨트롤러로 접속하지 않지만, 에디터 테스트 시를 위해 허용한다.
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
                    // 백엔드가 지정한 호스트 ID와 일치하면 권한 부여.
                    return true;
                }
            }
        }
    }

    return false;
}

bool AMenuGameMode::VerifyWithBackend(const FString& RoomId, const FString& StartToken, FString& OutError) const
{
    if (const UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance()))
    {
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
    }

    return true;
}

bool AMenuGameMode::ValidateServerInstance(const FString& ExpectedAddress, int32 ExpectedPort, FString& OutActualEndpoint) const
{
    OutActualEndpoint = TEXT("NO_WORLD");

    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    OutActualEndpoint = TEXT("NO_NETDRIVER");
    UNetDriver* NetDriver = World->GetNetDriver();
    if (!NetDriver)
    {
        return false;
    }

    if (NetDriver->ServerConnection)
    {
        OutActualEndpoint = NetDriver->ServerConnection->LowLevelDescribe();
    }
    else
    {
        OutActualEndpoint = FString::Printf(TEXT("%s:%d"), *NetDriver->LocalAddr->ToString(false), NetDriver->LocalAddr->GetPort());
    }

    if (ExpectedAddress.IsEmpty() || ExpectedPort <= 0)
    {
        return false;
    }

    const bool bMatchesAddress = OutActualEndpoint.Contains(ExpectedAddress, ESearchCase::IgnoreCase);
    const FString ExpectedPortSuffix = FString::Printf(TEXT(":%d"), ExpectedPort);
    const bool bMatchesPort = OutActualEndpoint.EndsWith(ExpectedPortSuffix, ESearchCase::IgnoreCase);

    return bMatchesAddress && bMatchesPort;
}

void AMenuGameMode::BroadcastServerEndpointAudit(const FString& ExpectedAddress, int32 ExpectedPort) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FString ExpectedEndpoint = BuildEndpointLabel(ExpectedAddress, ExpectedPort);

    if (UNetDriver* NetDriver = World->GetNetDriver())
    {
        FString LocalEndpoint = TEXT("Unknown");

        if (NetDriver->ServerConnection)
        {
            LocalEndpoint = NetDriver->ServerConnection->LowLevelDescribe();
        }
        else if (NetDriver->LocalAddr.IsValid())
        {
            LocalEndpoint = FString::Printf(
                TEXT("%s:%d"),
                *NetDriver->LocalAddr->ToString(false),
                NetDriver->LocalAddr->GetPort()
            );
        }
        else
        {
            const FURL& URL = World->URL;
            LocalEndpoint = FString::Printf(TEXT("%s:%d"), *URL.Host, URL.Port);
        }

        UE_LOG(LogTemp, Log, TEXT("[Match] ServerEndpointAudit local=%s expected=%s"), *LocalEndpoint, *ExpectedEndpoint);
    }

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            FString RemoteEndpoint(TEXT("UNKNOWN"));
            if (UNetConnection* Connection = PC->NetConnection)
            {
                RemoteEndpoint = Connection->LowLevelGetRemoteAddress(true);
            }

            UE_LOG(LogTemp, Log, TEXT("[Match] ClientEndpointAudit remote=%s player=%s expectedServer=%s"), *RemoteEndpoint, *GetNameSafe(PC), *ExpectedEndpoint);

            if (ABombTagPlayerController* BombTagPC = Cast<ABombTagPlayerController>(PC))
            {
                BombTagPC->ClientLogServerEndpoint(ExpectedAddress, ExpectedPort);
            }
        }
    }
}