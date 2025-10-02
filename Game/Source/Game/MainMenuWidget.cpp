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
        WaitingRoomMenuBackButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OnWaitingRoomBackClicked);
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

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(MatchDotsTimerHandle, this, &UMainMenuWidget::UpdateMatchMenuDots, 0.4f, true);

        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            GI->OnBackendLogin.AddDynamic(this, &UMainMenuWidget::HandleBackendLogin);
            GI->OnRoomJoined.AddDynamic(this, &UMainMenuWidget::HandleRoomJoined);
            GI->OnRoomUpdated.AddDynamic(this, &UMainMenuWidget::HandleRoomUpdated);
            GI->OnRoomStarted.AddDynamic(this, &UMainMenuWidget::HandleRoomStarted);
            GI->OnRoomClosed.AddDynamic(this, &UMainMenuWidget::HandleRoomClosed);

            if (!bGuestLoginRequested)
            {
                GI->Backend_GuestLogin(GI->GetPlayerNickname());
                bGuestLoginRequested = true;
            }
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

    if (UWorld* World = GetWorld())
    {
        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            GI->OnBackendLogin.RemoveDynamic(this, &UMainMenuWidget::HandleBackendLogin);
            GI->OnRoomJoined.RemoveDynamic(this, &UMainMenuWidget::HandleRoomJoined);
            GI->OnRoomUpdated.RemoveDynamic(this, &UMainMenuWidget::HandleRoomUpdated);
            GI->OnRoomStarted.RemoveDynamic(this, &UMainMenuWidget::HandleRoomStarted);
        }
    }

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
    ShowErrorMessage(HostMenuErrorText, FString());
    if (MenuSwitcher && HostMenu) MenuSwitcher->SetActiveWidget(HostMenu);
}

void UMainMenuWidget::OpenJoinMenu()
{
    StopWaitingRoomSlotUpdates();
    ShowErrorMessage(JoinMenuErrorText, FString());
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

void UMainMenuWidget::OnWaitingRoomBackClicked()
{
    RequestLeaveCurrentRoom();
    OpenMainMenu();
}

void UMainMenuWidget::OpenWaitingRoomMenu()
{
    if (MenuSwitcher && WaitingRoomMenu) MenuSwitcher->SetActiveWidget(WaitingRoomMenu);
    ShowErrorMessage(WaitingRoomMenuStatusText, FString());
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

    if (Name.IsEmpty())
    {
        Name = TEXT("BombTag Session");
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
            PendingRoomRequest = ERoomRequestType::Host;
            ShowErrorMessage(HostMenuErrorText, FString());
            GI->Backend_CreateRoom(Name, 4, Password);
            return;
        }
    }
    PendingRoomRequest = ERoomRequestType::None;
    ShowErrorMessage(HostMenuErrorText, TEXT("Failed to contact server"));
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
            PendingRoomRequest = ERoomRequestType::Join;
            ShowErrorMessage(JoinMenuErrorText, FString());
            GI->Backend_JoinRoom(Name, Password);
            return;
        }
    }

    PendingRoomRequest = ERoomRequestType::None;
    ShowErrorMessage(JoinMenuErrorText, TEXT("Failed to contact server"));
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
            if (WaitingRoomMenuStatusText)
            {
                WaitingRoomMenuStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
                WaitingRoomMenuStatusText->SetText(NSLOCTEXT("WaitingRoom", "StartingMatch", "Starting match..."));
            }
            GI->Backend_StartRoom();
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
        bIsWaitingRoomVisible = true;
        UpdateWaitingRoomSlotsFromGameState();
        RequestRoomSummaryRefresh();

        if (!World->GetTimerManager().IsTimerActive(WaitingRoomRefreshTimerHandle))
        {
            World->GetTimerManager().SetTimer(
                WaitingRoomRefreshTimerHandle,
                this,
                &UMainMenuWidget::RequestRoomSummaryRefresh,
                2.0f,
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

    bIsWaitingRoomVisible = false;
    bHasCachedSummary = false;
    ResetWaitingRoomSlots();
    ShowErrorMessage(WaitingRoomMenuStatusText, FString());
}

void UMainMenuWidget::RequestLeaveCurrentRoom()
{
    if (UWorld* World = GetWorld())
    {
        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            GI->LeaveSession();
        }
    }
}

void UMainMenuWidget::UpdateWaitingRoomSlotsFromGameState()
{
    ResetWaitingRoomSlots();

    if (!bHasCachedSummary)
    {
        return;
    }

    UBombTagGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UBombTagGameInstance>() : nullptr;

    TArray<FRoomPlayer> PlayersToDisplay = CachedRoomSummary.Players;

    if (PlayersToDisplay.Num() == 0 && GI)
    {
        FRoomPlayer SelfPlayer;
        SelfPlayer.PlayerId = TEXT("LocalPlayer");
        SelfPlayer.Nickname = GI->GetPlayerNickname();
        PlayersToDisplay.Add(SelfPlayer);
    }

    int32 Slots = 1;
    for (const FRoomPlayer& Player : PlayersToDisplay)
    {
        if (Slots > 4)
        {
            break;
        }

        FString DisplayName = Player.Nickname;
        if (DisplayName.IsEmpty())
        {
            DisplayName = Player.PlayerId;
        }

        int32 Win = 0;
        int32 Lose = 0;
        if (GI && !DisplayName.IsEmpty() && DisplayName.Equals(GI->GetPlayerNickname(), ESearchCase::IgnoreCase))
        {
            int32 Total = 0;
            GI->GetPlayerRecord(Win, Lose, Total);
        }

        SetWaitingRoomSlotPopulated(Slots, DisplayName, Win, Lose);
        ++Slots;
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

void UMainMenuWidget::RequestRoomSummaryRefresh()
{
    if (!bIsWaitingRoomVisible)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            GI->Backend_GetRoom();
        }
    }
}

void UMainMenuWidget::ApplyRoomSummary(const FRoomSummary& RoomSummary)
{
    CachedRoomSummary = RoomSummary;
    bHasCachedSummary = true;

    if (bIsWaitingRoomVisible)
    {
        UpdateWaitingRoomSlotsFromGameState();
    }
}

void UMainMenuWidget::ShowErrorMessage(UTextBlock* Target, const FString& Message)
{
    if (!Target)
    {
        return;
    }

    if (Message.IsEmpty())
    {
        Target->SetText(FText::GetEmpty());
        Target->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    }
    else
    {
        Target->SetText(FText::FromString(Message));
        Target->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
    }
}

void UMainMenuWidget::HandleBackendLogin(bool bSuccess, const FString& ErrorMessage)
{
    if (!bSuccess)
    {
        ShowErrorMessage(WaitingRoomMenuStatusText, FString::Printf(TEXT("Login failed: %s"), *ErrorMessage));
    }
    else
    {
        ShowErrorMessage(WaitingRoomMenuStatusText, FString());
    }
}

void UMainMenuWidget::HandleRoomJoined(bool bSuccess, const FString& ErrorMessage)
{
    if (!bSuccess)
    {
        const FString& Message = ErrorMessage.IsEmpty() ? TEXT("Unknown error") : ErrorMessage;
        switch (PendingRoomRequest)
        {
        case ERoomRequestType::Host:
            ShowErrorMessage(HostMenuErrorText, Message);
            break;
        case ERoomRequestType::Join:
            ShowErrorMessage(JoinMenuErrorText, Message);
            break;
        default:
            break;
        }

        PendingRoomRequest = ERoomRequestType::None;
        return;
    }

    ShowErrorMessage(HostMenuErrorText, FString());
    ShowErrorMessage(JoinMenuErrorText, FString());
    PendingRoomRequest = ERoomRequestType::None;

    EnterWaitingRoomForLocalPlayer();

    if (UWorld* World = GetWorld())
    {
        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            GI->Backend_GetRoom();
        }
    }
}

void UMainMenuWidget::HandleRoomUpdated(const FRoomSummary& RoomSummary)
{
    ApplyRoomSummary(RoomSummary);
}

void UMainMenuWidget::HandleRoomStarted(bool bSuccess, const FString& Info)
{
    if (!bSuccess)
    {
        const FString& Message = Info.IsEmpty() ? TEXT("Failed to start match") : Info;
        ShowErrorMessage(WaitingRoomMenuStatusText, Message);
        return;
    }

    ShowErrorMessage(WaitingRoomMenuStatusText, FString());
    StopWaitingRoomSlotUpdates();

    if (UWorld* World = GetWorld())
    {
        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            GI->StartHostedMatch();
        }
    }
}

void UMainMenuWidget::HandleRoomClosed(const FString& Reason)
{
    StopWaitingRoomSlotUpdates();

    const FString DisplayMessage = Reason.Contains(TEXT("ROOM_NOT_FOUND"))
        ? TEXT("The rooom owner closed the room.")
        : (Reason.IsEmpty() ? TEXT("The room has been closed.") : Reason);

    if (MenuSwitcher && JoinMenu)
    {
        MenuSwitcher->SetActiveWidget(JoinMenu);
    }

    ShowErrorMessage(JoinMenuErrorText, DisplayMessage);
    PendingRoomRequest = ERoomRequestType::None;
}

#endif