#include "BombTagGameMode.h"
#include "BombTagStateBase.h"
#include "BombTagCharacter.h"
#include "BombTagPlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#if !UE_SERVER
#include "ResultEntryWidget.h"
#endif

ABombTagGameMode::ABombTagGameMode()
{
    GameStateClass = ABombTagStateBase::StaticClass();

    static ConstructorHelpers::FClassFinder<ABombTagCharacter> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    if (PlayerPawnBPClass.Succeeded())
    {
        DefaultPawnClass = PlayerPawnBPClass.Class;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to find default pawn class '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter'. Falling back to ABombTagCharacter."));
        DefaultPawnClass = ABombTagCharacter::StaticClass();
    }

    static ConstructorHelpers::FClassFinder<ABombTagPlayerController> PlayerControllerBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"));
    if (PlayerControllerBPClass.Succeeded())
    {
        PlayerControllerClass = PlayerControllerBPClass.Class;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to find player controller class '/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController'. Falling back to ABombTagPlayerController."));
        PlayerControllerClass = ABombTagPlayerController::StaticClass();
    }

#if !UE_SERVER
    static ConstructorHelpers::FClassFinder<UResultEntryWidget> ResultEntryBPClass(TEXT("/Game/UI/WBP_ResultEntry"));
    if (ResultEntryBPClass.Succeeded())
    {
        ResultEntryWidgetClass = ResultEntryBPClass.Class;
    }
#endif
}

void ABombTagGameMode::BeginPlay()
{
    Super::BeginPlay();
    BeginStartCountdown();
}

void ABombTagGameMode::OnGameTimerExpired()
{
    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(this, ABombTagCharacter::StaticClass(), Actors);

    TArray<ABombTagCharacter*> Characters;
    Characters.Reserve(Actors.Num());
    for (AActor* A : Actors)
    {
        if (ABombTagCharacter* Ch = Cast<ABombTagCharacter>(A))
        {
            Characters.Add(Ch);
        }
    }

    if (Characters.Num() <= 2)
    {
        TSet<APlayerController*> WinningControllers;
        for (ABombTagCharacter* Ch : Characters)
        {
            if (!Ch->HasBomb())
            {
                if (APlayerController* PC = Cast<APlayerController>(Ch->GetController()))
                {
                    WinningControllers.Add(PC);
                }
            }
        }

        BeginMatchResultSubmission(WinningControllers);
        return;
    }

    ABombTagCharacter* BombHolder = nullptr;
    for (ABombTagCharacter* Ch : Characters)
    {
        if (Ch->HasBomb())
        {
            BombHolder = Ch;
            break;
        }
    }

    if (BombHolder)
    {
        if (APlayerController* Controller = Cast<APlayerController>(BombHolder->GetController()))
        {
            Controller->StartSpectatingOnly();
        }
        BombHolder->Destroy();
    }

    RespawnPlayers();
    BeginStartCountdown();
}

float ABombTagGameMode::GetRemainingGameTime() const
{
    return GetWorldTimerManager().GetTimerRemaining(GameTimerHandle);
}

void ABombTagGameMode::StartNewRound()
{
    GetWorldTimerManager().ClearTimer(GameTimerHandle);
    GetWorldTimerManager().SetTimer(GameTimerHandle, this, &ABombTagGameMode::OnGameTimerExpired, GameDuration, false);

    TArray<AActor*> Actors;
    UGameplayStatics::GetAllActorsOfClass(this, ABombTagCharacter::StaticClass(), Actors);

    TArray<ABombTagCharacter*> Characters;
    for (AActor* A : Actors)
    {
        if (ABombTagCharacter* Ch = Cast<ABombTagCharacter>(A))
        {
            Ch->SetHasBomb_Server(false);
            Characters.Add(Ch);
        }
    }

    if (Characters.Num() > 0)
    {
        const int32 Index = FMath::RandRange(0, Characters.Num() - 1);
        Characters[Index]->SetHasBomb_Server(true);
    }
}

void ABombTagGameMode::RespawnPlayers()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC) continue;

        if (APawn* Pawn = PC->GetPawn())
        {
            PC->UnPossess();
            Pawn->Destroy();
        }

        if (PC->PlayerState && PC->PlayerState->IsSpectator())
        {
            continue;
        }

        RestartPlayer(PC);
    }
}

void ABombTagGameMode::BeginStartCountdown()
{
    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    CountdownTime = FMath::CeilToInt(StartDelay);
    GetWorldTimerManager().SetTimer(CountdownTimerHandle, this, &ABombTagGameMode::HandleStartCountdown, 1.f, true);
}

void ABombTagGameMode::HandleStartCountdown()
{
    if (CountdownTime > 0)
    {
        --CountdownTime;
        return;
    }

    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    StartNewRound();
}

void ABombTagGameMode::BeginMatchResultSubmission(const TSet<APlayerController*>& WinningControllers)
{
    PendingMatchResultSnapshot = BuildMatchResultSnapshot(WinningControllers);
    PendingMatchResultHash = PendingMatchResultSnapshot.BuildCanonicalSignature();
    PendingMatchResultVotes.Reset();
    PendingMatchParticipants.Reset();
    RespondedMatchParticipants.Reset();

    bAwaitingMatchResult = true;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ABombTagPlayerController* PC = Cast<ABombTagPlayerController>(It->Get()))
        {
            PendingMatchParticipants.Add(PC);
            PC->ClientRequestMatchResultSubmission(PendingMatchResultSnapshot);
        }
    }

    if (PendingMatchParticipants.Num() == 0)
    {
        FinalizeMatchResult(PendingMatchResultSnapshot);
    }
}

FBombTagMatchResultSnapshot ABombTagGameMode::BuildMatchResultSnapshot(const TSet<APlayerController*>& WinningControllers) const
{
    FBombTagMatchResultSnapshot Snapshot;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ABombTagPlayerController* PC = Cast<ABombTagPlayerController>(It->Get()))
        {
            FBombTagPlayerMatchResult Entry;

            if (APlayerState* PlayerState = PC->PlayerState)
            {
                Entry.PlayerName = PlayerState->GetPlayerName();
            }
            else
            {
                Entry.PlayerName = PC->GetName();
            }

            Entry.bIsWinner = WinningControllers.Contains(PC);
            Snapshot.PlayerResults.Add(Entry);
        }
    }

    return Snapshot;
}

void ABombTagGameMode::RegisterMatchResultSubmission(ABombTagPlayerController* PlayerController, const FString& ResultHash, bool bClientAccepted)
{
    if (!bAwaitingMatchResult || !PlayerController)
    {
        return;
    }

    if (!PendingMatchParticipants.Contains(PlayerController))
    {
        UE_LOG(LogTemp, Warning, TEXT("Received match result submission from unexpected controller %s"), *GetNameSafe(PlayerController));
        return;
    }

    if (RespondedMatchParticipants.Contains(PlayerController))
    {
        UE_LOG(LogTemp, Verbose, TEXT("Ignoring duplicate match result submission from %s"), *GetNameSafe(PlayerController));
        return;
    }

    RespondedMatchParticipants.Add(PlayerController);

    if (bClientAccepted)
    {
        int32& VoteCount = PendingMatchResultVotes.FindOrAdd(ResultHash);
        ++VoteCount;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Controller %s rejected the proposed match result"), *GetNameSafe(PlayerController));
    }

    EvaluateMatchResultSubmissions();
}

void ABombTagGameMode::EvaluateMatchResultSubmissions()
{
    if (!bAwaitingMatchResult)
    {
        return;
    }

    const int32 ParticipantCount = PendingMatchParticipants.Num();
    if (ParticipantCount <= 0)
    {
        FinalizeMatchResult(PendingMatchResultSnapshot);
        return;
    }

    const int32 Quorum = (ParticipantCount / 2) + 1;

    if (const int32* VoteCount = PendingMatchResultVotes.Find(PendingMatchResultHash))
    {
        if (*VoteCount >= Quorum)
        {
            UE_LOG(LogTemp, Log, TEXT("Match result quorum reached with %d/%d confirmations."), *VoteCount, ParticipantCount);
            FinalizeMatchResult(PendingMatchResultSnapshot);
            return;
        }
    }

    if (RespondedMatchParticipants.Num() >= ParticipantCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("Match result quorum not reached (%d/%d). Using server authoritative snapshot."),
            PendingMatchResultVotes.FindRef(PendingMatchResultHash), ParticipantCount);
        FinalizeMatchResult(PendingMatchResultSnapshot);
    }
}

void ABombTagGameMode::FinalizeMatchResult(const FBombTagMatchResultSnapshot& Snapshot)
{
    if (!bAwaitingMatchResult)
    {
        return;
    }

    bAwaitingMatchResult = false;

    PendingMatchResultVotes.Reset();
    PendingMatchParticipants.Reset();
    RespondedMatchParticipants.Reset();
    PendingMatchResultHash.Reset();
    PendingMatchResultSnapshot = Snapshot;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ABombTagPlayerController* PC = Cast<ABombTagPlayerController>(It->Get()))
        {
            FString PlayerName;
            if (APlayerState* PlayerState = PC->PlayerState)
            {
                PlayerName = PlayerState->GetPlayerName();
            }

            const bool bWinner = Snapshot.IsPlayerWinner(PlayerName);
            PC->ClientFinalizeMatchResult(Snapshot, bWinner);
            PC->ClientShowResultScreen(ResultEntryWidgetClass, bWinner);
        }
    }
}