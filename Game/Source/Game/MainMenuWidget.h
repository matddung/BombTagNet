#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UWidgetSwitcher;
class UWidget;
class UTextBlock;
class UEditableTextBox;
class UCheckBox;

UCLASS()
class GAME_API UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual bool Initialize() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> MenuSwitcher;

    // Menu
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> MainMenu;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> MatchMenu;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> HostMenu;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> JoinMenu;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> MyRecordMenu;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> WaitingRoomMenu;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> NewNicknameMenu;

    // MenuButton
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> MatchButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> HostButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> JoinButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> MyRecordButton;

    // MatchMenu
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> MatchMenuTextBlock;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> MatchMenuBackButton;

    // HostMenu
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableTextBox> HostMenuTitleTextBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableTextBox> HostMenuPasswordTextBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> HostMenuPasswordCheckBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> HostMenuCreateButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> HostMenuBackButton;

    // JoinMenu
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableTextBox> JoinMenuTitleTextBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableTextBox> JoinMenuPasswordTextBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> JoinMenuJoinButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> JoinMenuBackButton;

    // MyRecordMenu
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> MyRecordMenuNicknameText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> MyRecordMenuWinText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> MyRecordMenuLoseText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> MyRecordMenuRateText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> MyRecordMenuBackButton;

    // WaitingRoomMenu
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> WaitingRoomMenuBackButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> WaitingRoomMenuStartButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> WaitingRoomMenuPlayer1Button;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> WaitingRoomMenuPlayer2Button;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> WaitingRoomMenuPlayer3Button;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> WaitingRoomMenuPlayer4Button;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> WaitingRoomMenuPlayer1Switcher;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> WaitingRoomMenuPlayer2Switcher;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> WaitingRoomMenuPlayer3Switcher;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> WaitingRoomMenuPlayer4Switcher;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WaitingRoomMenuPlayer1IDText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WaitingRoomMenuPlayer2IDText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WaitingRoomMenuPlayer3IDText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WaitingRoomMenuPlayer4IDText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WaitingRoomMenuPlayer1RecordText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WaitingRoomMenuPlayer2RecordText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WaitingRoomMenuPlayer3RecordText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> WaitingRoomMenuPlayer4RecordText;

    // NewNicknameMenu
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableTextBox> NewNicknameMenuNicknameTextBox;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> NewNicknameMenuConfirmButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> NewNicknameMenuErrorText;

private:
    UFUNCTION() void OpenMatchMenu();
    UFUNCTION() void OpenHostMenu();
    UFUNCTION() void OpenJoinMenu();
    UFUNCTION() void OpenMyRecordMenu();
    UFUNCTION() void OpenMainMenu();

    UFUNCTION() void CreateHostMatch();
    UFUNCTION() void UpdateMatchMenuDots();
    UFUNCTION() void JoinMatch();

    UFUNCTION() void OnHostMenuPasswordCheckBoxChanged(bool bIsChecked);

    UFUNCTION() void OpenWaitingRoomMenu();
    UFUNCTION() void WaitingRoomStart();
    UFUNCTION() void WaitingRoomPlayerMenu(int32 PlayerIndex);

    void UpdateMyRecordMenu();

    UFUNCTION() void OpenNewNicknameMenu();
    UFUNCTION() void ConfirmNewNickname();
    UFUNCTION() void OnNewNicknameTextChanged(const FText& NewText);

    bool   IsValidNickname(const FString& Nickname) const;
    FText  GetNicknameValidationErrorText(const FString& Nickname) const;
    void   UpdateNewNicknameError(const FString& Nickname);
    bool   IsAsciiAlphanumeric(TCHAR Character) const;

    void ResetWaitingRoomSlots();
    void SetWaitingRoomSlotWaiting(int32 PlayerIndex);
    void SetWaitingRoomSlotPopulated(int32 PlayerIndex, const FString& PlayerId, int32 WinCount, int32 LoseCount);
    void EnterWaitingRoomForLocalPlayer();
    void HandleWaitingRoomJoinSucceeded();
    UWidgetSwitcher* GetWaitingRoomSlotSwitcher(int32 PlayerIndex) const;
    UTextBlock* GetWaitingRoomSlotIdText(int32 PlayerIndex) const;
    UTextBlock* GetWaitingRoomSlotRecordText(int32 PlayerIndex) const;
    void StartWaitingRoomSlotUpdates();
    void StopWaitingRoomSlotUpdates();
    void UpdateWaitingRoomSlotsFromGameState();

    FTimerHandle   MatchDotsTimerHandle;
    int32          MatchDotCount = 1;
    FDelegateHandle WaitingRoomJoinDelegateHandle;
    FTimerHandle   WaitingRoomRefreshTimerHandle;
};