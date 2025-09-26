#include "BombTagPlayerController.h"
#include "BombTagCharacter.h"
#include "BombTagGameInstance.h"
#include "BombTagStateBase.h"
#include "ResultEntryWidget.h"

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

    if (IsLocalPlayerController() && SVirtualJoystick::ShouldDisplayTouchInterface())
    {
        MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
        if (MobileControlsWidget) MobileControlsWidget->AddToPlayerScreen(0);
        else UE_LOG(LogTemp, Error, TEXT("Could not spawn mobile controls widget."));
    }

    if (IsLocalPlayerController() && CanDisplayPlayerUI())
    {
        if (bWantsHUDWidget && !HUDWidget) ShowHUDWidget();
        if (DeferredMenuClass)
        {
            const TSubclassOf<UUserWidget> MenuClassToShow = DeferredMenuClass;
            DeferredMenuClass = nullptr;
            ShowMainMenuInternal(MenuClassToShow);
        }
    }
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

    if (!CanDisplayPlayerUI())
    {
        bWantsHUDWidget = true;
        return;
    }

    bWantsHUDWidget = false;

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
        if (NicknameToApply.IsEmpty()) NicknameToApply = TEXT("Guest");
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

void ABombTagPlayerController::ClientShowMainMenu_Implementation(TSubclassOf<UUserWidget> InMenuClass)
{
#if !UE_SERVER
    if (!IsLocalPlayerController()) return;
    if (!InMenuClass) return;

    if (!CanDisplayPlayerUI())
    {
        DeferredMenuClass = InMenuClass;
        return;
    }

    DeferredMenuClass = nullptr;
    ShowMainMenuInternal(InMenuClass);
#endif
}

void ABombTagPlayerController::ShowMainMenuInternal(TSubclassOf<UUserWidget> InMenuClass)
{
#if !UE_SERVER
    if (!IsLocalPlayerController()) return;

    if (HUDWidget) HUDWidget->RemoveFromParent();
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

bool ABombTagPlayerController::CanDisplayPlayerUI() const
{
#if !UE_SERVER
    return IsLocalPlayerController() || GetNetMode() == NM_Client;
#endif
}