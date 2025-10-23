#include "BombTagGameMode.h"
#include "BombTagStateBase.h"
#include "BombTagCharacter.h"
#include "BombTagPlayerController.h"
#include "BombTagGameInstance.h"

#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace
{
    void ExtractTravelTargets(const FString& URL, FString& OutMap, FString& OutGameMode)
    {
        // ServerTravel에 전달하는 URL에서 맵 경로와 게임모드 파라미터를 분리한다.
        OutMap = URL;
        OutGameMode.Reset();

        FString MapPart;
        FString Options;
        if (URL.Split(TEXT("?"), &MapPart, &Options))
        {
            OutMap = MapPart;
            const FString GameModeOption = UGameplayStatics::ParseOption(Options, TEXT("game"));
            if (!GameModeOption.IsEmpty())
            {
                OutGameMode = GameModeOption;
            }
        }
    }

    FString ResolveHostId(const UBombTagGameInstance* GameInstance)
    {
        // 매치 로그에 일관된 호스트 식별자를 남기기 위한 보조 함수.
        if (!GameInstance)
        {
            return FString(TEXT("UNKNOWN_HOST"));
        }

        const FString HostId = GameInstance->GetEffectiveHostPlayerId();
        return HostId.IsEmpty() ? FString(TEXT("UNKNOWN_HOST")) : HostId;
    }
}

#if !UE_SERVER
#include "ResultEntryWidget.h"
#endif

ABombTagGameMode::ABombTagGameMode()
{
    GameStateClass = ABombTagStateBase::StaticClass();
    PlayerControllerClass = ABombTagPlayerController::StaticClass();

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
    const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(this, true);
    const FString OwnerId = ResolveHostId(Cast<UBombTagGameInstance>(GetGameInstance()));
    // 매치 시작 시점에 맵/게임모드/시즌리스 설정을 모두 로그로 남겨 추적한다.
    UE_LOG(LogTemp, Log, TEXT("[Match] CurrentMap=%s GameMode=%s Seamless=%s Owner=%s"), *CurrentMap, *GetClass()->GetName(), bUseSeamlessTravel ? TEXT("true") : TEXT("false"), *OwnerId);
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

    ScheduleReturnToMenu();
}

void ABombTagGameMode::ScheduleReturnToMenu()
{
    GetWorldTimerManager().ClearTimer(ReturnToMenuTimerHandle);

    if (MenuReturnURL.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] MenuReturnURL is empty; cannot return to menu."));
        return;
    }

    const float Delay = FMath::Max(0.f, ReturnToMenuDelay);
    if (Delay <= KINDA_SMALL_NUMBER)
    {
        // 즉시 복귀해야 하면 타이머를 사용하지 않고 바로 처리한다.
        HandleReturnToMenu();
        return;
    }

    GetWorldTimerManager().SetTimer(ReturnToMenuTimerHandle, this, &ABombTagGameMode::HandleReturnToMenu, Delay, false);
}

void ABombTagGameMode::HandleReturnToMenu()
{
    GetWorldTimerManager().ClearTimer(ReturnToMenuTimerHandle);

    if (MenuReturnURL.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] MenuReturnURL is empty; cannot return to menu."));
        return;
    }

    FString MapName;
    FString GameModePath;
    ExtractTravelTargets(MenuReturnURL, MapName, GameModePath);
    bUseSeamlessTravel = true;

    const FString OwnerId = ResolveHostId(Cast<UBombTagGameInstance>(GetGameInstance()));
    // 서버만이 트래블을 수행하며, 동일한 포맷으로 로그를 남겨 클라이언트 추종 여부를 검증한다.
    UE_LOG(LogTemp, Log, TEXT("[Match] ServerTravel to %s (Map=%s GameMode=%s Owner=%s Seamless=%s)"), *MenuReturnURL, *MapName, *GameModePath, *OwnerId, bUseSeamlessTravel ? TEXT("true") : TEXT("false"));

    if (UWorld* World = GetWorld())
    {
        World->ServerTravel(MenuReturnURL, true);
    }
}