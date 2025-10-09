#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResultEntryWidget.generated.h"

class UTextBlock;

UCLASS()
class GAME_API UResultEntryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* FirstIDText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SecondIDText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* ThirdIDText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* FourthIDText;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* PlayerRecordText;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;

private:
    UFUNCTION()
    void HandlePlayerRecordUpdated(int32 Win, int32 Lose, int32 TotalMatches);

    void UpdatePlayerRecordText(int32 Win, int32 Lose, int32 TotalMatches);
    void GoToMenu();

    TWeakObjectPtr<class UBombTagGameInstance> CachedGameInstance;
};