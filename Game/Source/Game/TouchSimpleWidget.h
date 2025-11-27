#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TouchSimpleWidget.generated.h"

class UButton;
class ABombTagCharacter;
class APawn;


UCLASS()
class GAME_API UTouchSimpleWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintCallable, Category = "Touch Controls")
    void HandleMoveInput(FVector2D StickInput);

    UFUNCTION(BlueprintCallable, Category = "Touch Controls")
    void HandleLookInput(FVector2D StickInput);

    UFUNCTION(BlueprintCallable)
    void HandleInteract();

    ABombTagCharacter* GetOwningBombTagCharacter() const;

    UFUNCTION()
    void HandlePawnChanged(APawn* NewPawn);

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Btn_Interact;

    mutable TWeakObjectPtr<ABombTagCharacter> CachedCharacter;
    FDelegateHandle PawnChangedHandle;
};