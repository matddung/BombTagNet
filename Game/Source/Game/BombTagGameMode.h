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
    void ScheduleReturnToMenu();
    void HandleReturnToMenu();

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

    UPROPERTY(EditDefaultsOnly, Category = "Match")
    float ReturnToMenuDelay = 5.f; // 결과 연출 후 서버가 메뉴로 복귀하기까지의 지연 시간.

    UPROPERTY(EditDefaultsOnly, Category = "Match")
    FString MenuReturnURL = TEXT("/Game/Maps/MenuMap?game=/Game/Blueprints/BP_MenuGameMode.BP_MenuGameMode_C"); // 서버 전용 메뉴 복귀 목적지.

    FTimerHandle ReturnToMenuTimerHandle;
};