#include "TouchSimpleWidget.h"
#include "BombTagCharacter.h"

#include "Components/Button.h"
#include "GameFramework/PlayerController.h"

void UTouchSimpleWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (APlayerController* PC = GetOwningPlayer())
    {
        PawnChangedHandle = PC->GetOnNewPawnNotifier().AddUObject(this, &UTouchSimpleWidget::HandlePawnChanged);
    }

    if (Btn_Interact)
    {
        Btn_Interact->OnPressed.AddDynamic(this, &UTouchSimpleWidget::HandleInteract);
    }

    GetOwningBombTagCharacter();
}

void UTouchSimpleWidget::NativeDestruct()
{
    if (Btn_Interact)
    {
        Btn_Interact->OnPressed.RemoveDynamic(this, &UTouchSimpleWidget::HandleInteract);
    }

    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->GetOnNewPawnNotifier().Remove(PawnChangedHandle);
    }

    CachedCharacter = nullptr;

    Super::NativeDestruct();
}

void UTouchSimpleWidget::HandleMoveInput(FVector2D StickInput)
{
    if (ABombTagCharacter* Character = GetOwningBombTagCharacter())
    {
        Character->DoMove(StickInput.X, StickInput.Y);
    }
}

void UTouchSimpleWidget::HandleLookInput(FVector2D StickInput)
{
    if (ABombTagCharacter* Character = GetOwningBombTagCharacter())
    {
        Character->DoLook(StickInput.X, StickInput.Y);
    }
}

void UTouchSimpleWidget::HandleInteract()
{
    if (ABombTagCharacter* Character = GetOwningBombTagCharacter())
    {
        Character->DoInteract();
    }
}

ABombTagCharacter* UTouchSimpleWidget::GetOwningBombTagCharacter() const
{
    if (CachedCharacter.IsValid())
    {
        return CachedCharacter.Get();
    }

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            if (ABombTagCharacter* Character = Cast<ABombTagCharacter>(Pawn))
            {
                CachedCharacter = Character;
                return CachedCharacter.Get();
            }
        }
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            CachedCharacter = Cast<ABombTagCharacter>(PC->GetPawn());
            if (CachedCharacter.IsValid())
            {
                return CachedCharacter.Get();
            }
        }
    }

    return nullptr;
}

void UTouchSimpleWidget::HandlePawnChanged(APawn* NewPawn)
{
    CachedCharacter = Cast<ABombTagCharacter>(NewPawn);
}