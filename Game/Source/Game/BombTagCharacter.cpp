#include "BombTagCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Engine/LocalPlayer.h"
#include "InputActionValue.h"

ABombTagCharacter::ABombTagCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	BombIndicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombIndicator"));
	BombIndicator->SetupAttachment(RootComponent);
	BombIndicator->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	BombIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BombIndicator->SetVisibility(false);
}

void ABombTagCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABombTagCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ABombTagCharacter::Look);

		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABombTagCharacter::Look);

		EnhancedInputComponent->BindAction(Interact, ETriggerEvent::Started, this, &ABombTagCharacter::DoInteract);
	}
}

void ABombTagCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	DoMove(MovementVector.X, MovementVector.Y);
}

void ABombTagCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ABombTagCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ABombTagCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ABombTagCharacter::DoJumpStart()
{
	Jump();
}

void ABombTagCharacter::DoJumpEnd()
{
	StopJumping();
}

void ABombTagCharacter::DoInteract()
{
	if (!bHasBomb) return;

	if (!HasAuthority())
	{
		ServerDoInteract();
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<ABombTagCharacter> It(World); It; ++It)
	{
		ABombTagCharacter* Other = *It;
		if (Other && Other != this && !Other->HasBomb())
		{
			const float DistSq = FVector::DistSquared(Other->GetActorLocation(), GetActorLocation());
			if (DistSq <= FMath::Square(BombTransferDistance))
			{
				// 서버에서만 값을 바꾸고, 복제에 맡긴다
				SetHasBomb_Server(false);
				Other->SetHasBomb_Server(true);
				break;
			}
		}
	}
}

void ABombTagCharacter::ServerDoInteract_Implementation()
{
	DoInteract();
}

void ABombTagCharacter::SetHasBomb_Server(bool bNewHasBomb)
{
	check(HasAuthority());
	if (bHasBomb == bNewHasBomb) return;
	bHasBomb = bNewHasBomb;

	ForceNetUpdate();
}

void ABombTagCharacter::OnRep_HasBomb()
{
	if (BombIndicator)
	{
		BombIndicator->SetVisibility(bHasBomb);
	}
}

void ABombTagCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABombTagCharacter, bHasBomb);
}