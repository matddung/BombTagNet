#include "MainMenuWidget.h"
#include "BombTagGameInstance.h"

#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/Char.h"

#if !UE_SERVER

bool UMainMenuWidget::Initialize()
{
    const bool bOk = Super::Initialize();
    if (!bOk) return false;

    if (MatchButton)
    {
        MatchButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenMatchMenu);
    }

    if (HostButton)
    {
        HostButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenHostMenu);
    }

    if (JoinButton)
    {
        JoinButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenJoinMenu);
    }

    if (MyRecordButton)
    {
        MyRecordButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenMyRecordMenu);
    }

    if (MatchMenuBackButton)
    {
        MatchMenuBackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenMainMenu);
    }

    if (HostMenuBackButton)
    {
        HostMenuBackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenMainMenu);
    }

    if (JoinMenuBackButton)
    {
        JoinMenuBackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenMainMenu);
    }

    if (MyRecordMenuBackButton)
    {
        MyRecordMenuBackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenMainMenu);
    }

    if (WaitingRoomMenuBackButton)
    {
        WaitingRoomMenuBackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenMainMenu);
    }

    if (HostMenuPasswordCheckBox)
    {
        HostMenuPasswordCheckBox->OnCheckStateChanged.AddDynamic(this, &UMainMenuWidget::OnHostMenuPasswordCheckBoxChanged);
    }

    if (HostMenuCreateButton)
    {
        HostMenuCreateButton->OnClicked.AddDynamic(this, &UMainMenuWidget::CreateHostMatch);
    }

    if (JoinMenuJoinButton)
    {
        JoinMenuJoinButton->OnClicked.AddDynamic(this, &UMainMenuWidget::JoinMatch);
    }

    if (WaitingRoomMenuStartButton)
    {
        WaitingRoomMenuStartButton->OnClicked.AddDynamic(this, &UMainMenuWidget::WaitingRoomStart);
    }

    if (NewNicknameMenuConfirmButton)
    {
        NewNicknameMenuConfirmButton->OnClicked.AddDynamic(this, &UMainMenuWidget::ConfirmNewNickname);
    }

    if (NewNicknameMenuNicknameTextBox)
    {
        NewNicknameMenuNicknameTextBox->OnTextChanged.AddDynamic(this, &UMainMenuWidget::OnNewNicknameTextChanged);
    }

    return true;
}

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    GetWorld()->GetTimerManager().SetTimer(MatchDotsTimerHandle, this, &UMainMenuWidget::UpdateMatchMenuDots, 0.4f, true);

    if (UWorld* W = GetWorld())
    {
        if (UBombTagGameInstance* GI = W->GetGameInstance<UBombTagGameInstance>())
        {
            GI->OnWaitingRoomJoinSucceeded().AddUObject(this, &UMainMenuWidget::HandleWaitingRoomJoinSucceeded);
        }
    }

    if (HostMenuPasswordCheckBox)
        OnHostMenuPasswordCheckBoxChanged(HostMenuPasswordCheckBox->IsChecked());

    bool bShowNewNickname = false;
    if (UWorld* W = GetWorld())
    {
        if (UBombTagGameInstance* GI = W->GetGameInstance<UBombTagGameInstance>())
        {
            FString Nick = GI->GetPlayerNickname();
            Nick.TrimStartAndEndInline();
            bShowNewNickname = Nick.IsEmpty();
        }
    }

    if (bShowNewNickname)
    {
        OpenNewNicknameMenu();
    }
    else
    {
        OpenMainMenu();
    }
}

void UMainMenuWidget::NativeDestruct()
{
    StopWaitingRoomSlotUpdates();

    Super::NativeDestruct();
}

void UMainMenuWidget::OpenMatchMenu()
{
    StopWaitingRoomSlotUpdates();
    if (MenuSwitcher && MatchMenu) MenuSwitcher->SetActiveWidget(MatchMenu);
}

void UMainMenuWidget::OpenHostMenu()
{
    StopWaitingRoomSlotUpdates();
    if (MenuSwitcher && HostMenu) MenuSwitcher->SetActiveWidget(HostMenu);
}

void UMainMenuWidget::OpenJoinMenu()
{
    StopWaitingRoomSlotUpdates();
    if (MenuSwitcher && JoinMenu) MenuSwitcher->SetActiveWidget(JoinMenu);
}

void UMainMenuWidget::OpenMyRecordMenu()
{
    StopWaitingRoomSlotUpdates();
    UpdateMyRecordMenu();
    if (MenuSwitcher && MyRecordMenu) MenuSwitcher->SetActiveWidget(MyRecordMenu);
}

void UMainMenuWidget::OpenMainMenu()
{
    StopWaitingRoomSlotUpdates();
    if (MenuSwitcher && MainMenu) MenuSwitcher->SetActiveWidget(MainMenu);
}

void UMainMenuWidget::OpenWaitingRoomMenu()
{
    if (MenuSwitcher && WaitingRoomMenu) MenuSwitcher->SetActiveWidget(WaitingRoomMenu);
    StartWaitingRoomSlotUpdates();
}

void UMainMenuWidget::CreateHostMatch()
{
    FString Name, Password;
    if (HostMenuTitleTextBox)
    {
        Name = HostMenuTitleTextBox->GetText().ToString();
        Name.TrimStartAndEndInline();
    }

    if (HostMenuPasswordCheckBox && HostMenuPasswordCheckBox->IsChecked() && HostMenuPasswordTextBox)
    {
        Password = HostMenuPasswordTextBox->GetText().ToString();
        Password.TrimStartAndEndInline();
    }

    if (UWorld* W = GetWorld())
    {
        if (UBombTagGameInstance* GI = W->GetGameInstance<UBombTagGameInstance>())
        {
            GI->HostOnlineSession(Name, Password, 4, false);
        }
    }
    EnterWaitingRoomForLocalPlayer();
}

void UMainMenuWidget::JoinMatch()
{
    FString Name, Password;

    if (JoinMenuTitleTextBox)
    {
        Name = JoinMenuTitleTextBox->GetText().ToString();
        Name.TrimStartAndEndInline();
    }

    if (JoinMenuPasswordTextBox)
    {
        Password = JoinMenuPasswordTextBox->GetText().ToString();
        Password.TrimStartAndEndInline();
    }

    if (UWorld* W = GetWorld())
    {
        if (UBombTagGameInstance* GI = W->GetGameInstance<UBombTagGameInstance>())
        {
            GI->FindAndJoinSession(Name, Password, false);
        }
    }

    EnterWaitingRoomForLocalPlayer();
}

void UMainMenuWidget::OnHostMenuPasswordCheckBoxChanged(bool bIsChecked)
{
    if (HostMenuPasswordTextBox)
    {
        HostMenuPasswordTextBox->SetIsEnabled(bIsChecked);
    }
}

void UMainMenuWidget::WaitingRoomStart()
{
    if (UWorld* World = GetWorld())
    {
        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            GI->StartHostedMatch();
        }
    }
}

void UMainMenuWidget::WaitingRoomPlayerMenu(int32 PlayerIndex)
{
    if (UWidgetSwitcher* Target = GetWaitingRoomSlotSwitcher(PlayerIndex))
    {
        Target->SetActiveWidgetIndex(1);
    }
}

void UMainMenuWidget::ResetWaitingRoomSlots()
{
    for (int32 i = 1; i <= 4; ++i) SetWaitingRoomSlotWaiting(i);
}

void UMainMenuWidget::SetWaitingRoomSlotWaiting(int32 PlayerIndex)
{
    if (UWidgetSwitcher* S = GetWaitingRoomSlotSwitcher(PlayerIndex))
        S->SetActiveWidgetIndex(0);
}

void UMainMenuWidget::SetWaitingRoomSlotPopulated(int32 PlayerIndex, const FString& PlayerId, int32 WinCount, int32 LoseCount)
{
    if (UWidgetSwitcher* S = GetWaitingRoomSlotSwitcher(PlayerIndex))
        S->SetActiveWidgetIndex(1);

    if (UTextBlock* IdText = GetWaitingRoomSlotIdText(PlayerIndex))
    {
        const FText DisplayId = PlayerId.IsEmpty()
            ? NSLOCTEXT("WaitingRoom", "GuestName", "Guest")
            : FText::FromString(PlayerId);
        IdText->SetText(DisplayId);
    }

    if (UTextBlock* RecText = GetWaitingRoomSlotRecordText(PlayerIndex))
    {
        RecText->SetText(FText::FromString(FString::Printf(TEXT("Win : %d / Lose : %d"), WinCount, LoseCount)));
    }
}

void UMainMenuWidget::EnterWaitingRoomForLocalPlayer()
{
    OpenWaitingRoomMenu();
}

void UMainMenuWidget::HandleWaitingRoomJoinSucceeded()
{
    EnterWaitingRoomForLocalPlayer();
}

UWidgetSwitcher* UMainMenuWidget::GetWaitingRoomSlotSwitcher(int32 PlayerIndex) const
{
    switch (PlayerIndex)
    {
    case 1: return WaitingRoomMenuPlayer1Switcher;
    case 2: return WaitingRoomMenuPlayer2Switcher;
    case 3: return WaitingRoomMenuPlayer3Switcher;
    case 4: return WaitingRoomMenuPlayer4Switcher;
    default: return nullptr;
    }
}

UTextBlock* UMainMenuWidget::GetWaitingRoomSlotIdText(int32 PlayerIndex) const
{
    switch (PlayerIndex)
    {
    case 1: return WaitingRoomMenuPlayer1IDText;
    case 2: return WaitingRoomMenuPlayer2IDText;
    case 3: return WaitingRoomMenuPlayer3IDText;
    case 4: return WaitingRoomMenuPlayer4IDText;
    default: return nullptr;
    }
}

UTextBlock* UMainMenuWidget::GetWaitingRoomSlotRecordText(int32 PlayerIndex) const
{
    switch (PlayerIndex)
    {
    case 1: return WaitingRoomMenuPlayer1RecordText;
    case 2: return WaitingRoomMenuPlayer2RecordText;
    case 3: return WaitingRoomMenuPlayer3RecordText;
    case 4: return WaitingRoomMenuPlayer4RecordText;
    default: return nullptr;
    }
}

void UMainMenuWidget::StartWaitingRoomSlotUpdates()
{
    if (UWorld* World = GetWorld())
    {
        UpdateWaitingRoomSlotsFromGameState();

        if (!World->GetTimerManager().IsTimerActive(WaitingRoomRefreshTimerHandle))
        {
            World->GetTimerManager().SetTimer(
                WaitingRoomRefreshTimerHandle,
                this,
                &UMainMenuWidget::UpdateWaitingRoomSlotsFromGameState,
                1.0f,
                true
            );
        }
    }
}

void UMainMenuWidget::StopWaitingRoomSlotUpdates()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(WaitingRoomRefreshTimerHandle);
    }
}

void UMainMenuWidget::UpdateWaitingRoomSlotsFromGameState()
{
    ResetWaitingRoomSlots();

    UWorld* World = GetWorld();
    if (!World) return;

    AGameStateBase* GS = World->GetGameState<AGameStateBase>();
    if (!GS) return;

    APlayerController* LPC = GetOwningPlayer();
    APlayerState* LPS = LPC ? LPC->PlayerState : nullptr;

    UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>();

    TArray<APlayerState*> PSs = GS->PlayerArray;
    PSs.Sort([](const APlayerState& A, const APlayerState& B)
        {
            return A.GetPlayerId() < B.GetPlayerId();
        });

    int32 PSlot = 1;
    for (APlayerState* PS : PSs)
    {
        if (!PS) continue;
        if (PSlot > 4) break;

        int32 Win = 0, Lose = 0;
        if (PS == LPS && GI)
        {
            int32 Total = 0;
            GI->GetPlayerRecord(Win, Lose, Total);
        }

        SetWaitingRoomSlotPopulated(PSlot, PS->GetPlayerName(), Win, Lose);
        ++PSlot;
    }

    if (PSlot == 1 && GI)
    {
        FString Nick = GI->GetPlayerNickname();
        int32 Win = 0, Lose = 0, Total = 0;
        GI->GetPlayerRecord(Win, Lose, Total);
        SetWaitingRoomSlotPopulated(1, Nick, Win, Lose);
    }
}

void UMainMenuWidget::UpdateMyRecordMenu()
{
    UBombTagGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UBombTagGameInstance>() : nullptr;
    if (!GI) return;

    const FString Nick = GI->GetPlayerNickname();
    if (MyRecordMenuNicknameText)
    {
        const FText NickTxt = Nick.IsEmpty()
            ? NSLOCTEXT("MainMenu", "DefaultNickname", "Guest")
            : FText::FromString(Nick);
        MyRecordMenuNicknameText->SetText(NickTxt);
    }

    int32 Win = 0, Lose = 0, Total = 0;
    GI->GetPlayerRecord(Win, Lose, Total);

    if (MyRecordMenuWinText)  MyRecordMenuWinText->SetText(FText::FromString(FString::Printf(TEXT("Win : %d"), Win)));
    if (MyRecordMenuLoseText) MyRecordMenuLoseText->SetText(FText::FromString(FString::Printf(TEXT("Lose : %d"), Lose)));
    if (MyRecordMenuRateText)
    {
        const float Rate = Total > 0 ? (float(Win) / float(Total)) * 100.f : 0.f;
        MyRecordMenuRateText->SetText(FText::FromString(FString::Printf(TEXT("Rate : %.1f%%"), Rate)));
    }
}

void UMainMenuWidget::OpenNewNicknameMenu()
{
    StopWaitingRoomSlotUpdates();

    if (NewNicknameMenuNicknameTextBox)
    {
        UBombTagGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UBombTagGameInstance>() : nullptr;
        const FString Current = GI ? GI->GetPlayerNickname() : FString();
        NewNicknameMenuNicknameTextBox->SetText(FText::FromString(Current));
        UpdateNewNicknameError(Current);
    }

    if (MenuSwitcher && NewNicknameMenu) MenuSwitcher->SetActiveWidget(NewNicknameMenu);
}

void UMainMenuWidget::ConfirmNewNickname()
{
    if (!NewNicknameMenuNicknameTextBox) return;

    FString Entered = NewNicknameMenuNicknameTextBox->GetText().ToString();
    Entered.TrimStartAndEndInline();

    UpdateNewNicknameError(Entered);
    if (!IsValidNickname(Entered)) return;

    if (UBombTagGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UBombTagGameInstance>() : nullptr)
    {
        GI->SetPlayerNickname(Entered);
    }

    OpenMainMenu();
}

void UMainMenuWidget::OnNewNicknameTextChanged(const FText& NewText)
{
    UpdateNewNicknameError(NewText.ToString());
}

bool UMainMenuWidget::IsValidNickname(const FString& Nickname) const
{
    return GetNicknameValidationErrorText(Nickname).IsEmpty();
}

FText UMainMenuWidget::GetNicknameValidationErrorText(const FString& Nickname) const
{
    FString Trimmed = Nickname;
    Trimmed.TrimStartAndEndInline();

    if (Trimmed.IsEmpty())
        return NSLOCTEXT("MainMenu", "NicknameRequired", "Please enter your nickname");

    const int32 Len = Trimmed.Len();
    if (Len < 4 || Len > 10)
        return NSLOCTEXT("MainMenu", "NicknameLength", "Nickname must be 4-10 characters");

    for (const TCHAR C : Trimmed)
    {
        if (!IsAsciiAlphanumeric(C))
            return NSLOCTEXT("MainMenu", "NicknameInvalidChar", "Only English letters and numbers are allowed");
    }

    return FText::GetEmpty();
}

void UMainMenuWidget::UpdateNewNicknameError(const FString& Nickname)
{
    if (!NewNicknameMenuErrorText) return;

    const FText Err = GetNicknameValidationErrorText(Nickname);
    NewNicknameMenuErrorText->SetText(Err);

    if (!Err.IsEmpty())
    {
        NewNicknameMenuErrorText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
    }
}

bool UMainMenuWidget::IsAsciiAlphanumeric(TCHAR Character) const
{
    return (Character >= TEXT('0') && Character <= TEXT('9')) ||
        (Character >= TEXT('A') && Character <= TEXT('Z')) ||
        (Character >= TEXT('a') && Character <= TEXT('z'));
}

void UMainMenuWidget::UpdateMatchMenuDots()
{
    MatchDotCount = (MatchDotCount % 3) + 1;
    const FString Dots = FString::ChrN(MatchDotCount, TEXT('.'));
    const FText Base = NSLOCTEXT("Match", "Searching", "Searching for Match");
    if (MatchMenuTextBlock)
        MatchMenuTextBlock->SetText(FText::FromString(Base.ToString() + Dots));
}

#endif