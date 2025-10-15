#include "BombTagPlayerController.h"
#include "BombTagCharacter.h"
#include "BombTagGameInstance.h"
#include "BombTagStateBase.h"
#include "ResultEntryWidget.h"
#include "BombTagGameMode.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"

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