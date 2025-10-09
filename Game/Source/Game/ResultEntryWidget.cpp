#include "ResultEntryWidget.h"
#include "BombTagPlayerController.h"
#include "MenuGameMode.h"
#include "BombTagGameInstance.h"

#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#if !UE_SERVER

void UResultEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(TakeWidget());
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
        PC->bShowMouseCursor = true;

        if (ABombTagPlayerController* BTPC = Cast<ABombTagPlayerController>(PC))
        {
            BTPC->SetBorderFlashEnabled(false);
        }
    }

    if (UWorld* World = GetWorld())
    {
        if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
        {
            TArray<APlayerState*> PlayerStates = GS->PlayerArray;
            PlayerStates.Sort([](const APlayerState& A, const APlayerState& B)
                {
                    return A.GetScore() > B.GetScore();
                });

            auto SetTextIfValid = [](UTextBlock* TextBlock, const FString& Name)
                {
                    if (TextBlock)
                    {
                        TextBlock->SetText(FText::FromString(Name));
                    }
                };

            if (PlayerStates.Num() > 0) SetTextIfValid(FirstIDText, PlayerStates[0]->GetPlayerName());
            if (PlayerStates.Num() > 1) SetTextIfValid(SecondIDText, PlayerStates[1]->GetPlayerName());
            if (PlayerStates.Num() > 2) SetTextIfValid(ThirdIDText, PlayerStates[2]->GetPlayerName());
            if (PlayerStates.Num() > 3) SetTextIfValid(FourthIDText, PlayerStates[3]->GetPlayerName());
        }
    }

    if (UBombTagGameInstance* GameInstance = Cast<UBombTagGameInstance>(GetGameInstance()))
    {
        CachedGameInstance = GameInstance;
        GameInstance->OnPlayerRecordUpdated.AddDynamic(this, &UResultEntryWidget::HandlePlayerRecordUpdated);

        int32 Win = 0;
        int32 Lose = 0;
        int32 Total = 0;
        GameInstance->GetPlayerRecord(Win, Lose, Total);
        UpdatePlayerRecordText(Win, Lose, Total);
    }
}

void UResultEntryWidget::NativeDestruct()
{
    if (CachedGameInstance.IsValid())
    {
        CachedGameInstance->OnPlayerRecordUpdated.RemoveDynamic(this, &UResultEntryWidget::HandlePlayerRecordUpdated);
        CachedGameInstance = nullptr;
    }

    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->SetIgnoreMoveInput(false);
        PC->SetIgnoreLookInput(false);
        PC->bShowMouseCursor = false;

        if (ABombTagPlayerController* BTPC = Cast<ABombTagPlayerController>(PC))
        {
            BTPC->SetBorderFlashEnabled(true);
        }
    }

    Super::NativeDestruct();
}

FReply UResultEntryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    GoToMenu();
    return FReply::Handled();
}

FReply UResultEntryWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
    GoToMenu();
    return FReply::Handled();
}

void UResultEntryWidget::GoToMenu()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        const FString GameModePath = AMenuGameMode::StaticClass()->GetPathName();
        const FString TravelURL = GameModePath.IsEmpty()
            ? FString(TEXT("/Game/Maps/MenuMap"))
            : FString::Printf(TEXT("/Game/Maps/MenuMap?game=%s"), *GameModePath);
        PC->ClientTravel(TravelURL, TRAVEL_Absolute);
        return;
    }

    if (UWorld* World = GetWorld())
    {
        const FString GameModePath = AMenuGameMode::StaticClass()->GetPathName();
        const FString Options = GameModePath.IsEmpty()
            ? FString()
            : FString::Printf(TEXT("game=%s"), *GameModePath);
        UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/Maps/MenuMap")), true, Options);
    }
}

void UResultEntryWidget::HandlePlayerRecordUpdated(int32 Win, int32 Lose, int32 TotalMatches)
{
    UpdatePlayerRecordText(Win, Lose, TotalMatches);
}

void UResultEntryWidget::UpdatePlayerRecordText(int32 Win, int32 Lose, int32 TotalMatches)
{
    if (!PlayerRecordText)
    {
        return;
    }

    const FText RecordText = FText::FromString(FString::Printf(TEXT("Win : %d Loss : %d (Total : %d)"), Win, Lose, TotalMatches));
    PlayerRecordText->SetText(RecordText);
}

#endif