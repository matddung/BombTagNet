#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MatchResultTypes.h"
#include "BombTagGameMode.generated.h"

class UResultEntryWidget;
class ABombTagPlayerController;

UCLASS()
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

    void BeginMatchResultSubmission(const TSet<APlayerController*>& WinningControllers);
    FBombTagMatchResultSnapshot BuildMatchResultSnapshot(const TSet<APlayerController*>& WinningControllers) const;
    void EvaluateMatchResultSubmissions();
    void FinalizeMatchResult(const FBombTagMatchResultSnapshot& Snapshot);

public:
    UFUNCTION(BlueprintPure, Category = "Game")
    float GetRemainingGameTime() const;

    void RegisterMatchResultSubmission(ABombTagPlayerController* PlayerController, const FString& ResultHash, bool bClientAccepted);

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

    bool bAwaitingMatchResult = false;
    FBombTagMatchResultSnapshot PendingMatchResultSnapshot;
    FString PendingMatchResultHash;
    TMap<FString, int32> PendingMatchResultVotes;
    TSet<TWeakObjectPtr<ABombTagPlayerController>> PendingMatchParticipants;
    TSet<TWeakObjectPtr<ABombTagPlayerController>> RespondedMatchParticipants;
};