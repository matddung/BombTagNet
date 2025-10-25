#include "BombTagPlayerController.h"
#include "BombTagCharacter.h"
#include "BombTagGameInstance.h"
#include "BombTagStateBase.h"
#include "ResultEntryWidget.h"
#include "BombTagGameMode.h"
#include "MenuGameMode.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"

ABombTagPlayerController::ABombTagPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;

#if !UE_SERVER
    static ConstructorHelpers::FClassFinder<UUserWidget> HUDBPClass(TEXT("/Game/UI/WBP_HUDWidget"));
    if (HUDBPClass.Succeeded())
    {
        HUDWidgetClass = HUDBPClass.Class;
    }
#endif
}

void ABombTagPlayerController::BeginPlay()
{
    Super::BeginPlay();

    const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(this, true);
    // 매치 흐름 디버깅을 위해 접속 시 현재 맵 정보를 남긴다.
    UE_LOG(LogTemp, Log, TEXT("[Match] CurrentMap=%s"), *CurrentMap);

#if !UE_SERVER
    if (IsLocalPlayerController())
    {
        ShowHUDWidget();

        FString Nickname;
        if (UBombTagGameInstance* GI = Cast<UBombTagGameInstance>(GetGameInstance()))
        {
            Nickname = GI->GetPlayerNickname();
            Nickname.TrimStartAndEndInline();
        }
        ServerSetPlayerNickname(Nickname);
    }

    ShowMobileControlsIfNeeded();
#endif
}

void ABombTagPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

#if !UE_SERVER
    ApplyDefaultGameInputMode();
#endif
}

void ABombTagPlayerController::OnRep_Pawn()
{
    Super::OnRep_Pawn();

#if !UE_SERVER
    ApplyDefaultGameInputMode();
#endif
}

void ABombTagPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

#if !UE_SERVER
    if (IsLocalPlayerController())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            for (UInputMappingContext* C : DefaultMappingContexts) Subsystem->AddMappingContext(C, 0);

            if (!SVirtualJoystick::ShouldDisplayTouchInterface())
            {
                for (UInputMappingContext* C : MobileExcludedMappingContexts)
                    Subsystem->AddMappingContext(C, 0);
            }
        }
    }
#endif
}

void ABombTagPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

#if !UE_SERVER
    if (IsLocalPlayerController())
    {
        if (TimerText)
        {
            if (ABombTagStateBase* GS = GetWorld()->GetGameState<ABombTagStateBase>())
            {
                const float Remaining = FMath::Max(0.f, GS->GetRemainingGameTime());
                TimerText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Remaining)));
            }
        }

        if (BorderFlash)
        {
            ABombTagCharacter* Ch = Cast<ABombTagCharacter>(GetPawn());
            const bool bShouldFlash = bBorderFlashEnabled && Ch && Ch->HasBomb() && !GetWorld()->IsPaused();
            if (bShouldFlash)
            {
                BorderFlashElapsed += DeltaSeconds;
                const float Alpha = 0.5f * (1.f - FMath::Cos(2.f * PI * BorderFlashElapsed));
                BorderFlash->SetRenderOpacity(Alpha);
            }
            else
            {
                BorderFlashElapsed = 0.f;
                BorderFlash->SetRenderOpacity(0.f);
            }
        }
    }
#endif
}

void ABombTagPlayerController::SetBorderFlashEnabled(bool bEnabled)
{
#if !UE_SERVER
    bBorderFlashEnabled = bEnabled;
    if (!bBorderFlashEnabled && BorderFlash)
    {
        BorderFlashElapsed = 0.f;
        BorderFlash->SetRenderOpacity(0.f);
    }
#endif
}

void ABombTagPlayerController::ShowHUDWidget()
{
#if !UE_SERVER
    if (!IsLocalPlayerController()) return;
    if (!HUDWidgetClass) return;

    if (!HUDWidget)
    {
        HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
    }
    if (HUDWidget && !HUDWidget->IsInViewport())
    {
        HUDWidget->AddToPlayerScreen();
        TimerText = Cast<UTextBlock>(HUDWidget->GetWidgetFromName(TEXT("TimerText")));
        BorderFlash = Cast<UBorder>(HUDWidget->GetWidgetFromName(TEXT("BorderFlash")));
        if (BorderFlash) BorderFlash->SetRenderOpacity(0.f);
        ShowMobileControlsIfNeeded();
    }
#endif
}

void ABombTagPlayerController::ServerSetPlayerNickname_Implementation(const FString& Nickname)
{
    if (!HasAuthority()) return;
    if (PlayerState)
    {
        FString NicknameToApply = Nickname;
        NicknameToApply.TrimStartAndEndInline();
        if (NicknameToApply.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("ServerSetPlayerNickname called with empty nickname; ignoring."));
            return;
        }
        PlayerState->SetPlayerName(NicknameToApply);
    }
}

void ABombTagPlayerController::ClientShowResultScreen_Implementation(TSubclassOf<UResultEntryWidget> ResultWidgetClass, bool bWinner)
{
#if !UE_SERVER
    if (!IsLocalPlayerController()) return;

    if (ResultWidgetClass)
    {
        if (UResultEntryWidget* ResultWidget = CreateWidget<UResultEntryWidget>(this, ResultWidgetClass))
        {
            ResultWidget->AddToPlayerScreen();
        }
    }
#endif
}

void ABombTagPlayerController::ClientRequestMatchResultSubmission_Implementation(const FBombTagMatchResultSnapshot& Snapshot)
{
#if !UE_SERVER
    if (!IsLocalPlayerController())
    {
        return;
    }

    const bool bIsValid = ValidateMatchSnapshot(Snapshot);
    const FString ResultHash = Snapshot.BuildCanonicalSignature();

    ServerSubmitMatchResultHash(ResultHash, bIsValid);

    if (!bIsValid)
    {
        UE_LOG(LogTemp, Warning, TEXT("Client %s rejected match snapshot and reported hash %s"), *GetName(), *ResultHash);
    }
#endif
}

void ABombTagPlayerController::ClientLogServerEndpoint_Implementation(const FString& ExpectedAddress, int32 ExpectedPort)
{
#if !UE_SERVER
    if (!IsLocalPlayerController())
    {
        return;
    }

    FString RemoteAddress(TEXT("UNKNOWN"));
    if (const UWorld* World = GetWorld())
    {
        if (UNetDriver* NetDriver = World->GetNetDriver())
        {
            if (UNetConnection* Connection = NetDriver->ServerConnection)
            {
                RemoteAddress = Connection->LowLevelGetRemoteAddress(true);
            }
        }
    }

    FString ExpectedEndpoint(TEXT("UNSPECIFIED"));
    if (!ExpectedAddress.IsEmpty() && ExpectedPort > 0)
    {
        ExpectedEndpoint = FString::Printf(TEXT("%s:%d"), *ExpectedAddress, ExpectedPort);
    }
    else if (!ExpectedAddress.IsEmpty())
    {
        ExpectedEndpoint = ExpectedAddress;
    }

    UE_LOG(LogTemp, Log, TEXT("[Match] ServerEndpointAudit remote=%s expected=%s"), *RemoteAddress, *ExpectedEndpoint);
#endif
}

void ABombTagPlayerController::ServerSubmitMatchResultHash_Implementation(const FString& ResultHash, bool bClientAccepted)
{
    if (ABombTagGameMode* GameMode = GetWorld()->GetAuthGameMode<ABombTagGameMode>())
    {
        GameMode->RegisterMatchResultSubmission(this, ResultHash, bClientAccepted);
    }
}

void ABombTagPlayerController::ClientFinalizeMatchResult_Implementation(const FBombTagMatchResultSnapshot& FinalSnapshot, bool bIsWinner)
{
#if !UE_SERVER
    if (!IsLocalPlayerController())
    {
        return;
    }

    if (UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance()))
    {
        const EBombTagMatchResult MatchResult = bIsWinner ? EBombTagMatchResult::Win : EBombTagMatchResult::Lose;
        GameInstance->RecordMatchResult(MatchResult);
    }
#endif
}

void ABombTagPlayerController::ServerRequestStartMatch_Implementation(const FString& RoomId, const FString& StartToken)
{
    if (!HasAuthority())
    {
        return;
    }

    if (RoomId.IsEmpty())
    {
        // 방 정보 없이 호출되면 즉시 거부한다.
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] ServerRequestStartMatch received without a room id."));
        ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 1"));
        return;
    }

    if (AMenuGameMode* MenuGameMode = GetWorld()->GetAuthGameMode<AMenuGameMode>())
    {
        // 메뉴 게임모드가 호스트 권한과 백엔드 토큰을 검증한 뒤 트래블을 수행한다.
        MenuGameMode->HandleStartMatchRequest(this, RoomId, StartToken);
    }
    else
    {
        // 서버에서 메뉴 게임모드가 아닐 경우 안전하게 거부한다.
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] ServerRequestStartMatch called but menu game mode not available."));
        ClientNotifyMatchStartDenied(TEXT("MATCH_START_DENIED 2"));
    }
}

void ABombTagPlayerController::ClientNotifyMatchStartDenied_Implementation(const FString& ErrorCode)
{
    const FString& CodeToReport = ErrorCode.IsEmpty() ? FString(TEXT("MATCH_START_DENIED 3")) : ErrorCode;
    // 거부 사유를 표준화된 로그 형식으로 남겨 분석 가능하게 한다.
    UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Match start denied: %s"), *CodeToReport);

#if !UE_SERVER
    if (UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance()))
    {
        GameInstance->OnRoomStarted.Broadcast(false, CodeToReport);
    }
#endif
}

bool ABombTagPlayerController::ValidateMatchSnapshot(const FBombTagMatchResultSnapshot& Snapshot) const
{
#if !UE_SERVER
    if (Snapshot.IsEmpty())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
    if (!GameState)
    {
        return false;
    }

    TSet<FString> KnownPlayers;
    for (APlayerState* PlayerStates : GameState->PlayerArray)
    {
        if (!PlayerStates)
        {
            continue;
        }

        KnownPlayers.Add(PlayerStates->GetPlayerName());
    }

    for (const FBombTagPlayerMatchResult& Entry : Snapshot.PlayerResults)
    {
        if (!KnownPlayers.Contains(Entry.PlayerName))
        {
            UE_LOG(LogTemp, Warning, TEXT("Unknown player '%s' in submitted match snapshot"), *Entry.PlayerName);
            return false;
        }
    }

    return true;
#else
    return false;
#endif
}

void ABombTagPlayerController::ClientShowMainMenu_Implementation(TSubclassOf<UUserWidget> InMenuClass)
{
#if !UE_SERVER
    if (!IsLocalPlayerController()) return;
    if (!InMenuClass) return;

    ShowMainMenuInternal(InMenuClass);
#endif
}

void ABombTagPlayerController::ShowMainMenuInternal(TSubclassOf<UUserWidget> InMenuClass)
{
#if !UE_SERVER
    if (!IsLocalPlayerController()) return;

    if (HUDWidget) HUDWidget->RemoveFromParent();
    HideMobileControls();
    if (!MenuWidget) MenuWidget = CreateWidget<UUserWidget>(this, InMenuClass);

    if (MenuWidget && !MenuWidget->IsInViewport())
    {
        MenuWidget->AddToPlayerScreen(10);
        FInputModeUIOnly InputMode; InputMode.SetWidgetToFocus(MenuWidget->GetCachedWidget());
        SetInputMode(InputMode);
        bShowMouseCursor = true;
    }
#endif
}

void ABombTagPlayerController::ApplyDefaultGameInputMode()
{
#if !UE_SERVER
    if (!IsLocalPlayerController())
    {
        return;
    }

    FInputModeGameOnly InputMode;
    InputMode.SetConsumeCaptureMouseDown(true);
    SetInputMode(InputMode);
    bShowMouseCursor = false;
#endif
}

void ABombTagPlayerController::ShowMobileControlsIfNeeded()
{
#if !UE_SERVER
    if (!IsLocalPlayerController())
    {
        return;
    }

    if (!SVirtualJoystick::ShouldDisplayTouchInterface())
    {
        return;
    }

    if (!MobileControlsWidget && MobileControlsWidgetClass)
    {
        MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
        if (!MobileControlsWidget)
        {
            UE_LOG(LogTemp, Error, TEXT("Could not create mobile controls widget."));
            return;
        }
    }

    if (MobileControlsWidget && !MobileControlsWidget->IsInViewport())
    {
        MobileControlsWidget->AddToPlayerScreen(0);
    }
#endif
}

void ABombTagPlayerController::HideMobileControls()
{
#if !UE_SERVER
    if (MobileControlsWidget)
    {
        MobileControlsWidget->RemoveFromParent();
    }
#endif
}