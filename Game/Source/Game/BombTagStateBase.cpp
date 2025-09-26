#include "BombTagStateBase.h"
#include "BombTagGameMode.h"

#include "Net/UnrealNetwork.h"

ABombTagStateBase::ABombTagStateBase()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    bReplicates = true;
}

void ABombTagStateBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority())
    {
        ServerUpdateRemainingTime(DeltaSeconds);
    }
}

float ABombTagStateBase::GetRemainingGameTime() const
{
    return RemainingGameTime;
}

void ABombTagStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABombTagStateBase, RemainingGameTime);
}

void ABombTagStateBase::ServerUpdateRemainingTime(float DeltaSeconds)
{
    if (ABombTagGameMode* GM = GetWorld()->GetAuthGameMode<ABombTagGameMode>())
    {
        RemainingGameTime = GM->GetRemainingGameTime();
    }
}