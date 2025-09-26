#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BombTagPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UTextBlock;
class UBorder;
class UResultEntryWidget;
class UBombTagGameInstance;

UCLASS()
class GAME_API ABombTagPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	ABombTagPlayerController();

	void SetBorderFlashEnabled(bool bEnabled);
	void ShowHUDWidget();

	UFUNCTION(Client, Reliable)
	void ClientShowMainMenu(TSubclassOf<UUserWidget> InMenuClass);

    UFUNCTION(Client, Reliable)
    void ClientShowResultScreen(TSubclassOf<UResultEntryWidget> ResultWidgetClass, bool bWinner);
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(Server, Reliable)
	void ServerSetPlayerNickname(const FString& Nickname);

	void ShowMainMenuInternal(TSubclassOf<UUserWidget> InMenuClass);
	bool CanDisplayPlayerUI() const;

protected:
    UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
    TArray<UInputMappingContext*> DefaultMappingContexts;

    UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
    TArray<UInputMappingContext*> MobileExcludedMappingContexts;

    UPROPERTY(EditAnywhere, Category = "Input|Touch Controls")
    TSubclassOf<UUserWidget> MobileControlsWidgetClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> MobileControlsWidget;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> HUDWidget;

    UPROPERTY()
    TObjectPtr<UUserWidget> MenuWidget;

    UPROPERTY()
    TSubclassOf<UUserWidget> DeferredMenuClass;

    UPROPERTY()
    TObjectPtr<UTextBlock> TimerText;

    UPROPERTY()
    TObjectPtr<UBorder> BorderFlash;

    float BorderFlashElapsed = 0.f;
    bool bBorderFlashEnabled = true;
    bool bWantsHUDWidget = false;
};
