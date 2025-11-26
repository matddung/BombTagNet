#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MatchResultTypes.h"
#include "BombTagPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UTextBlock;
class UBorder;
class UResultEntryWidget;
class UBombTagGameInstance;
class UMainMenuWidget;
class UTouchInterface;

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

    UFUNCTION(Client, Reliable)
    void ClientRequestMatchResultSubmission(const FBombTagMatchResultSnapshot& Snapshot);

    UFUNCTION(Server, Reliable)
    void ServerSubmitMatchResultHash(const FString& ResultHash, bool bClientAccepted);

    UFUNCTION(Client, Reliable)
    void ClientFinalizeMatchResult(const FBombTagMatchResultSnapshot& FinalSnapshot, bool bIsWinner);

    UFUNCTION(Server, Reliable)
    void ServerRequestStartMatch(const FString& RoomId, const FString& StartToken, const FString& DedicatedServerAddress, int32 DedicatedServerPort, const FString& TravelURL);

    UFUNCTION(Client, Reliable)
    void ClientNotifyMatchStartDenied(const FString& ErrorCode);

    UFUNCTION(Client, Reliable)
    void ClientDebugMatchStartSnapshot(const FString& RoomId, const FString& StartToken,
        const FString& DsAddr, int32 DsPort, const FString& TravelURL);

    UFUNCTION(Client, Reliable)
    void ClientDebugVerifyStartResult(const FString& ResultSummary, bool bOk,
        const FString& RoomId, const FString& MatchId, const FString& DsId);
	
protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnRep_Pawn() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(Server, Reliable)
    void ServerSetPlayerNickname(const FString& Nickname);

    void ShowMainMenuInternal(TSubclassOf<UUserWidget> InMenuClass);
    void ApplyDefaultGameInputMode();
    void ShowMobileControlsIfNeeded();
    void HideMobileControls();


protected:
    UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
    TArray<UInputMappingContext*> DefaultMappingContexts;

    UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
    TArray<UInputMappingContext*> MobileExcludedMappingContexts;

    UPROPERTY(EditAnywhere, Category = "Input|Touch Controls")
    TSoftObjectPtr<UTouchInterface> TouchInterfaceAsset;

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
    TObjectPtr<UTextBlock> TimerText;

    UPROPERTY()
    TObjectPtr<UBorder> BorderFlash;

    float BorderFlashElapsed = 0.f;
    bool bBorderFlashEnabled = true;

private:
    bool ValidateMatchSnapshot(const FBombTagMatchResultSnapshot& Snapshot) const;
    bool IsMenuGameMode() const;

#if !UE_SERVER
    UMainMenuWidget* ResolveWaitingRoomWidget() const;
#endif
};
