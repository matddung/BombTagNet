#include "BombTagGameMode.h"
#include "BombTagStateBase.h"
#include "BombTagCharacter.h"
#include "BombTagPlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"

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
#if !UE_SERVER
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("BOOM!"));
        }
#endif

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

        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (ABombTagPlayerController* PC = Cast<ABombTagPlayerController>(It->Get()))
            {
                const bool bWinner = WinningControllers.Contains(PC);

                PC->ClientMessage(bWinner ? TEXT("WIN") : TEXT("Lose"));

                PC->ClientShowResultScreen(ResultEntryWidgetClass, bWinner);
            }
        }

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

    UE_LOG(LogTemp, Warning, TEXT("StartNewRound: %d chars"), Characters.Num());

    if (Characters.Num() > 0)
    {
        const int32 Index = FMath::RandRange(0, Characters.Num() - 1);
        UE_LOG(LogTemp, Warning, TEXT("Give bomb to: %s"), *Characters[Index]->GetName());
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
#if !UE_SERVER
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, FString::FromInt(CountdownTime));
        }
#endif
        --CountdownTime;
        return;
    }

#if !UE_SERVER
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Go!"));
    }
#endif

    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    StartNewRound();
}