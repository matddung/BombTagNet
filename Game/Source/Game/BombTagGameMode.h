#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BombTagGameMode.generated.h"

class UResultEntryWidget;

UCLASS(abstract)
class GAME_API ABombTagGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
    ABombTagGameMode();

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnGameTimerExpired();

    void StartNewRound();
    void HandleStartCountdown();
    void RespawnPlayers();
    void BeginStartCountdown();

public:
    UFUNCTION(BlueprintPure, Category = "Game")
    float GetRemainingGameTime() const;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Game")
    float GameDuration = 10.f;

    UPROPERTY(EditDefaultsOnly, Category = "Game")
    float StartDelay = 3.f;

    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UResultEntryWidget> ResultEntryWidgetClass;

    FTimerHandle GameTimerHandle;
    FTimerHandle CountdownTimerHandle;

    int32 CountdownTime = 0;
};