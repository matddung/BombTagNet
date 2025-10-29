#include "MenuGameMode.h"
#include "BombTagPlayerController.h"
#include "BombTagGameInstance.h"
#include "GameModeTravelUtils.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

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
        RequestingController->ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 6"));
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