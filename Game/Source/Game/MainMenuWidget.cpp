#include "MainMenuWidget.h"
#include "BombTagGameInstance.h"
#include "MatchService.h"

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

namespace
{
#if UE_BUILD_SHIPPING
    constexpr bool GVerboseTrafficUI = false;
#else
    constexpr bool GVerboseTrafficUI = true;
#endif
}

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

    MatchMenuBaseText = NSLOCTEXT("Match", "Searching", "Searching for Match");

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
            GI->OnMatchQueueStatus.AddDynamic(this, &UMainMenuWidget::HandleMatchQueueStatus);
            GI->OnBackendTraffic.AddDynamic(this, &UMainMenuWidget::HandleBackendTrafficMessage);

            if (!bLoginRequested && GI->HasPlayerNickname())
            {
                GI->Backend_Login(GI->GetPlayerNickname());
                bLoginRequested = true;
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
            bShowNewNickname = !GI->HasPlayerNickname();
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
    LeaveMatchQueue();

    if (UWorld* World = GetWorld())
    {
        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            GI->OnBackendLogin.RemoveDynamic(this, &UMainMenuWidget::HandleBackendLogin);
            GI->OnRoomJoined.RemoveDynamic(this, &UMainMenuWidget::HandleRoomJoined);
            GI->OnRoomUpdated.RemoveDynamic(this, &UMainMenuWidget::HandleRoomUpdated);
            GI->OnRoomStarted.RemoveDynamic(this, &UMainMenuWidget::HandleRoomStarted);
            GI->OnMatchQueueStatus.RemoveDynamic(this, &UMainMenuWidget::HandleMatchQueueStatus);
            GI->OnBackendTraffic.RemoveDynamic(this, &UMainMenuWidget::HandleBackendTrafficMessage);
        }
    }

    Super::NativeDestruct();
}

void UMainMenuWidget::OpenMatchMenu()
{
    StopWaitingRoomSlotUpdates();
    if (MenuSwitcher && MatchMenu) MenuSwitcher->SetActiveWidget(MatchMenu);
    MatchMenuBaseText = NSLOCTEXT("Match", "Searching", "Searching for Match");
    SetMatchMenuStatus(MatchMenuBaseText, true);
    JoinMatchQueue();
}

void UMainMenuWidget::OpenHostMenu()
{
    StopWaitingRoomSlotUpdates();
    LeaveMatchQueue();
    ShowErrorMessage(HostMenuErrorText, FString());
    if (MenuSwitcher && HostMenu) MenuSwitcher->SetActiveWidget(HostMenu);
}

void UMainMenuWidget::OpenJoinMenu()
{
    StopWaitingRoomSlotUpdates();
    LeaveMatchQueue();
    ShowErrorMessage(JoinMenuErrorText, FString());
    if (MenuSwitcher && JoinMenu) MenuSwitcher->SetActiveWidget(JoinMenu);
}

void UMainMenuWidget::OpenMyRecordMenu()
{
    StopWaitingRoomSlotUpdates();
    LeaveMatchQueue();
    UpdateMyRecordMenu();
    if (MenuSwitcher && MyRecordMenu) MenuSwitcher->SetActiveWidget(MyRecordMenu);
}

void UMainMenuWidget::OpenMainMenu()
{
    StopWaitingRoomSlotUpdates();
    LeaveMatchQueue();
    if (MenuSwitcher && MainMenu) MenuSwitcher->SetActiveWidget(MainMenu);
}

void UMainMenuWidget::OnWaitingRoomBackClicked()
{
    RequestLeaveCurrentRoom();
    OpenMainMenu();
}

void UMainMenuWidget::OpenWaitingRoomMenu()
{
    LeaveMatchQueue();
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

void UMainMenuWidget::JoinMatchQueue()
{
    if (UWorld* W = GetWorld())
    {
        if (UBombTagGameInstance* GI = W->GetGameInstance<UBombTagGameInstance>())
        {
            GI->Backend_JoinMatchQueue();
        }
    }
}

void UMainMenuWidget::LeaveMatchQueue()
{
    if (UWorld* W = GetWorld())
    {
        if (UBombTagGameInstance* GI = W->GetGameInstance<UBombTagGameInstance>())
        {
            GI->Backend_LeaveMatchQueue();
        }
    }

    bAnimateMatchMenuDots = false;
}

void UMainMenuWidget::OnHostMenuPasswordCheckBoxChanged(bool bIsChecked)
{
    if (HostMenuPasswordTextBox)
    {
        HostMenuPasswordTextBox->SetIsEnabled(bIsChecked);
    }
}

void UMainMenuWidget::SetMatchMenuStatus(const FText& StatusText, bool bAnimateDots, const FLinearColor& Color)
{
    FString Normalized = StatusText.ToString();
    if (Normalized.IsEmpty())
    {
        Normalized = NSLOCTEXT("Match", "Searching", "Searching for Match").ToString();
    }

    MatchMenuBaseText = FText::FromString(Normalized);
    bAnimateMatchMenuDots = bAnimateDots;

    if (MatchMenuTextBlock)
    {
        MatchMenuTextBlock->SetColorAndOpacity(FSlateColor(Color));

        if (bAnimateMatchMenuDots)
        {
            MatchDotCount = 1;
            const FString Dots = FString::ChrN(MatchDotCount, TEXT('.'));
            MatchMenuTextBlock->SetText(FText::FromString(Normalized + Dots));
        }
        else
        {
            MatchMenuTextBlock->SetText(MatchMenuBaseText);
        }
    }
}

void UMainMenuWidget::WaitingRoomStart()
{
    if (UWorld* World = GetWorld())
    {
        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            const FString RoomId = GI->GetCurrentRoomId();
            const FString Address = GI->GetPendingMatchServerAddress();
            const int32 Port = GI->GetPendingMatchServerPort();
            const FString Url = GI->GetPendingMatchTravelURL();
            HandleBackendTrafficMessage(FTrafficMsgFactory::MakeStartRequestInfo(RoomId, Address, Port, Url));
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
            ? NSLOCTEXT("WaitingRoom", "UnknownName", "(Unknown)")
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
        if (GetWorld())
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
            ? NSLOCTEXT("MainMenu", "NicknameUnset", "Please set a nickname")
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
        GI->Backend_Login(Entered);
        bLoginRequested = true;
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
    if (!bAnimateMatchMenuDots)
    {
        return;
    }

    MatchDotCount = (MatchDotCount % 3) + 1;
    const FString Dots = FString::ChrN(MatchDotCount, TEXT('.'));

    FString BaseString = MatchMenuBaseText.ToString();
    if (BaseString.IsEmpty())
    {
        BaseString = NSLOCTEXT("Match", "Searching", "Searching for Match").ToString();
    }

    if (MatchMenuTextBlock)
    {
        MatchMenuTextBlock->SetText(FText::FromString(BaseString + Dots));
    }
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

bool UMainMenuWidget::ShouldDisplayHostOnlyMessage(const FTrafficMsg& Message) const
{
    if (!Message.bHostOnly)
    {
        return true;
    }

    if (!IsLocalWaitingRoomHost())
    {
        return false;
    }
    return true;
}

bool UMainMenuWidget::IsLocalWaitingRoomHost() const
{
    if (const UWorld* World = GetWorld())
    {
        if (const UBombTagGameInstance* GameInstance = World->GetGameInstance<UBombTagGameInstance>())
        {
            const FString LocalPlayerId = GameInstance->GetLocalPlayerId();
            if (bHasCachedSummary && !CachedRoomSummary.HostId.IsEmpty() && !LocalPlayerId.IsEmpty())
            {
                if (CachedRoomSummary.HostId.Equals(LocalPlayerId, ESearchCase::CaseSensitive))
                {
                    return true;
                }
            }

            if (!GameInstance->GetPendingMatchStartToken().IsEmpty())
            {
                return true;
            }
        }
    }

    return false;
}

void UMainMenuWidget::HandleBackendTrafficMessage(const FTrafficMsg& Message)
{
    if (!WaitingRoomMenuStatusText)
    {
        return;
    }

    if (!ShouldDisplayHostOnlyMessage(Message))
    {
        return;
    }

    const bool bIsWarning = Message.Severity == ETrafficSeverity::Warn;
    const bool bIsError = Message.Severity == ETrafficSeverity::Error;

    if (!bIsWarning && !bIsError)
    {
        return;
    }

    const FLinearColor Color = bIsWarning ? FLinearColor::Yellow : FLinearColor::Red;
    WaitingRoomMenuStatusText->SetText(Message.Text);
    WaitingRoomMenuStatusText->SetColorAndOpacity(FSlateColor(Color));
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
        ShowErrorMessage(WaitingRoomMenuStatusText, FString());

        if (UWorld* World = GetWorld())
        {
            if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
            {
                if (Info.Contains(TEXT("MATCH_START_DENIED 7"), ESearchCase::IgnoreCase))
                {
                    const FString Address = GI->GetPendingMatchServerAddress();
                    const int32 Port = GI->GetPendingMatchServerPort();
                    const FString Url = GI->GetPendingMatchTravelURL();
                    HandleBackendTrafficMessage(FTrafficMsgFactory::MakeDenied7(Address, Port, Url));
                    return;
                }
            }
        }

        FTrafficMsg ErrorMsg;
        ErrorMsg.Severity = ETrafficSeverity::Error;
        ErrorMsg.TTLSeconds = 0.f;
        ErrorMsg.bHostOnly = false;
        ErrorMsg.Key = TEXT("start.denied.generic");
        const FString Reason = Info.IsEmpty() ? TEXT("Unknown Error") : Info;
        ErrorMsg.Text = FText::FromString(FString::Printf(TEXT("Match failed: %s"), *Reason));
        HandleBackendTrafficMessage(ErrorMsg);
        return;
    }

    ShowErrorMessage(WaitingRoomMenuStatusText, FString());
    StopWaitingRoomSlotUpdates();

    HandleBackendTrafficMessage(FTrafficMsgFactory::MakeStartApproved());

    if (UWorld* World = GetWorld())
    {
        if (UBombTagGameInstance* GI = World->GetGameInstance<UBombTagGameInstance>())
        {
            GI->RequestServerMatchStart();
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

void UMainMenuWidget::HandleMatchQueueStatus(bool bSuccess, const FMatchQueueStatus& Status, const FString& ErrorMessage)
{
    if (!MatchMenuTextBlock)
    {
        return;
    }

    if (!bSuccess)
    {
        FString CleanError = ErrorMessage;
        int32 ColonIndex = INDEX_NONE;
        if (CleanError.FindChar(TEXT(':'), ColonIndex))
        {
            CleanError = CleanError.Mid(ColonIndex + 1);
        }
        CleanError.TrimStartAndEndInline();
        if (CleanError.IsEmpty())
        {
            CleanError = TEXT("Unknown error");
        }

        const FText ErrorText = FText::Format(NSLOCTEXT("Match", "QueueError", "Matchmaking failed: {0}"), FText::FromString(CleanError));
        SetMatchMenuStatus(ErrorText, false, FLinearColor::Red);
        return;
    }

    switch (Status.Status)
    {
    case EMatchTicketStatus::Queued:
    {
        const FText Text = Status.Position > 0
            ? FText::Format(NSLOCTEXT("Match", "QueueWithPosition", "Waiting for players (Position {0})"), FText::AsNumber(Status.Position))
            : NSLOCTEXT("Match", "QueueWaiting", "Waiting for players");
        SetMatchMenuStatus(Text, true);
        break;
    }
    case EMatchTicketStatus::Forming:
    {
        const int32 PlayerCount = Status.Players.Num() > 0 ? Status.Players.Num() : FMath::Max(0, Status.MinPlayers);
        const int32 ReadySeconds = FMath::Max(0, Status.ReadyInSeconds);
        const FText Text = FText::Format(
            NSLOCTEXT("Match", "QueueForming", "Match ready with {0} players. Starting in {1}s"),
            FText::AsNumber(PlayerCount),
            FText::AsNumber(ReadySeconds)
        );
        SetMatchMenuStatus(Text, false);
        break;
    }
    case EMatchTicketStatus::Matched:
    {
        const int32 PlayerCount = Status.Players.Num() > 0 ? Status.Players.Num() : FMath::Max(Status.MinPlayers, 3);
        const FText Text = FText::Format(
            NSLOCTEXT("Match", "QueueMatched", "Match found! {0} players ready."),
            FText::AsNumber(PlayerCount)
        );
        SetMatchMenuStatus(Text, false, FLinearColor::Green);
        break;
    }
    case EMatchTicketStatus::Cancelled:
        SetMatchMenuStatus(NSLOCTEXT("Match", "QueueCancelled", "Matchmaking cancelled"), false, FLinearColor::Yellow);
        break;
    default:
        SetMatchMenuStatus(MatchMenuBaseText, true);
        break;
    }
}

#else // UE_SERVER

bool UMainMenuWidget::Initialize()
{
    return Super::Initialize();
}

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UMainMenuWidget::NativeDestruct()
{
    Super::NativeDestruct();
}

void UMainMenuWidget::OpenMatchMenu() {}

void UMainMenuWidget::OpenHostMenu() {}

void UMainMenuWidget::OpenJoinMenu() {}

void UMainMenuWidget::OpenMyRecordMenu() {}

void UMainMenuWidget::OpenMainMenu() {}

void UMainMenuWidget::OnWaitingRoomBackClicked() {}

void UMainMenuWidget::OpenWaitingRoomMenu() {}

void UMainMenuWidget::CreateHostMatch() {}

void UMainMenuWidget::JoinMatch() {}

void UMainMenuWidget::JoinMatchQueue() {}

void UMainMenuWidget::LeaveMatchQueue() {}

void UMainMenuWidget::OnHostMenuPasswordCheckBoxChanged(bool bIsChecked)
{
    (void)bIsChecked;
}

void UMainMenuWidget::SetMatchMenuStatus(const FText& StatusText, bool bAnimateDots, const FLinearColor& Color)
{
    (void)StatusText;
    (void)bAnimateDots;
    (void)Color;
}

void UMainMenuWidget::WaitingRoomStart() {}

void UMainMenuWidget::WaitingRoomPlayerMenu(int32 PlayerIndex)
{
    (void)PlayerIndex;
}

void UMainMenuWidget::ResetWaitingRoomSlots() {}

void UMainMenuWidget::SetWaitingRoomSlotWaiting(int32 PlayerIndex)
{
    (void)PlayerIndex;
}

void UMainMenuWidget::SetWaitingRoomSlotPopulated(int32 PlayerIndex, const FString& PlayerId, int32 WinCount, int32 LoseCount)
{
    (void)PlayerIndex;
    (void)PlayerId;
    (void)WinCount;
    (void)LoseCount;
}

void UMainMenuWidget::EnterWaitingRoomForLocalPlayer() {}

UWidgetSwitcher* UMainMenuWidget::GetWaitingRoomSlotSwitcher(int32 PlayerIndex) const
{
    (void)PlayerIndex;
    return nullptr;
}

UTextBlock* UMainMenuWidget::GetWaitingRoomSlotIdText(int32 PlayerIndex) const
{
    (void)PlayerIndex;
    return nullptr;
}

UTextBlock* UMainMenuWidget::GetWaitingRoomSlotRecordText(int32 PlayerIndex) const
{
    (void)PlayerIndex;
    return nullptr;
}

void UMainMenuWidget::StartWaitingRoomSlotUpdates() {}

void UMainMenuWidget::StopWaitingRoomSlotUpdates() {}

void UMainMenuWidget::RequestLeaveCurrentRoom() {}

void UMainMenuWidget::UpdateWaitingRoomSlotsFromGameState() {}

void UMainMenuWidget::UpdateMyRecordMenu() {}

void UMainMenuWidget::OpenNewNicknameMenu() {}

void UMainMenuWidget::ConfirmNewNickname() {}

void UMainMenuWidget::OnNewNicknameTextChanged(const FText& NewText)
{
    (void)NewText;
}

bool UMainMenuWidget::IsValidNickname(const FString& Nickname) const
{
    (void)Nickname;
    return true;
}

FText UMainMenuWidget::GetNicknameValidationErrorText(const FString& Nickname) const
{
    (void)Nickname;
    return FText::GetEmpty();
}

void UMainMenuWidget::UpdateNewNicknameError(const FString& Nickname)
{
    (void)Nickname;
}

bool UMainMenuWidget::IsAsciiAlphanumeric(TCHAR Character) const
{
    (void)Character;
    return true;
}

void UMainMenuWidget::UpdateMatchMenuDots() {}

void UMainMenuWidget::RequestRoomSummaryRefresh() {}

void UMainMenuWidget::ApplyRoomSummary(const FRoomSummary& RoomSummary)
{
    (void)RoomSummary;
}

void UMainMenuWidget::ShowErrorMessage(UTextBlock* Target, const FString& Message)
{
    (void)Target;
    (void)Message;
}

void UMainMenuWidget::HandleBackendLogin(bool bSuccess, const FString& ErrorMessage)
{
    (void)bSuccess;
    (void)ErrorMessage;
}

void UMainMenuWidget::HandleRoomJoined(bool bSuccess, const FString& ErrorMessage)
{
    (void)bSuccess;
    (void)ErrorMessage;
}

void UMainMenuWidget::HandleRoomUpdated(const FRoomSummary& RoomSummary)
{
    (void)RoomSummary;
}

void UMainMenuWidget::HandleRoomStarted(bool bSuccess, const FString& Info)
{
    (void)bSuccess;
    (void)Info;
}

void UMainMenuWidget::HandleRoomClosed(const FString& Reason)
{
    (void)Reason;
}

void UMainMenuWidget::HandleMatchQueueStatus(bool bSuccess, const FMatchQueueStatus& Status, const FString& ErrorMessage)
{
    (void)bSuccess;
    (void)Status;
    (void)ErrorMessage;
}

void UMainMenuWidget::HandleBackendTrafficMessage(const FTrafficMsg& Message)
{
    (void)Message;
}

bool UMainMenuWidget::ShouldDisplayHostOnlyMessage(const FTrafficMsg& Message) const
{
    (void)Message;
    return false;
}

bool UMainMenuWidget::IsLocalWaitingRoomHost() const
{
    return false;
}

#endif