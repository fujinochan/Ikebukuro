#include "NinjaCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

ANinjaCharacter::ANinjaCharacter()
{
	JumpMaxHoldTime = 0;
}

void ANinjaCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Apply the Ninja tuning even when inheriting saved template Blueprint defaults.
	auto* Movement = GetCharacterMovement();
	Movement->GravityScale = 1.5f;
	Movement->AirControl = 0.65f;
	Movement->MaxAcceleration = 5000;
	Movement->BrakingDecelerationWalking = 5000;
	Movement->BrakingDecelerationFalling = 0;
	Movement->MaxWalkSpeed = WalkSpeed;
	if (const auto* PC = Cast<APlayerController>(GetController()))
	{
		if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (SprintMappingContext) Subsystem->AddMappingContext(SprintMappingContext, 1);
		}
	}
}

void ANinjaCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
	if (const auto* PC = Cast<APlayerController>(GetController()))
	{
		if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (SprintMappingContext) Subsystem->RemoveMappingContext(SprintMappingContext);
		}
	}
	Super::EndPlay(Reason);
}

void ANinjaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (const auto* PC = Cast<APlayerController>(GetController()))
	{
		if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (SprintMappingContext) Subsystem->AddMappingContext(SprintMappingContext, 1);
		}
	}
	if (auto* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent); Input && SprintAction)
	{
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ANinjaCharacter::DoSprintStart);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ANinjaCharacter::DoSprintEnd);
		Input->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ANinjaCharacter::DoSprintEnd);
	}
}

float ANinjaCharacter::GetSecondsToJumpApex() const
{
	return static_cast<float>(PredictedApexTime - GetWorld()->GetTimeSeconds());
}

bool ANinjaCharacter::CanChainJump() const
{
	return JumpStage > 0 && JumpStage < 3 && GetCharacterMovement()->IsFalling() &&
		FMath::Abs(GetSecondsToJumpApex()) <= TimingHalfWindow;
}

void ANinjaCharacter::DoJumpStart()
{
	if (bJumpButtonHeld) return;
	bJumpButtonHeld = true;
	const bool bGrounded = GetCharacterMovement()->IsMovingOnGround();
	if (!bGrounded && !CanChainJump())
	{
		JumpFeedback = JumpStage >= 3 ? TEXT("3 / 3 - land to recharge") :
			JumpStage == 0 ? TEXT("Land before jumping") :
			GetSecondsToJumpApex() > 0 ? TEXT("Too early - try near the apex") : TEXT("Too late - land and retry");
		return;
	}
	JumpStage = bGrounded ? 1 : JumpStage + 1;
	const float Speed = JumpStage == 1 ? JumpSpeeds.X : JumpStage == 2 ? JumpSpeeds.Y : JumpSpeeds.Z;
	const float Gravity = FMath::Max(FMath::Abs(GetCharacterMovement()->GetGravityZ()), 1.f);
	PredictedApexTime = GetWorld()->GetTimeSeconds() + Speed / Gravity;
	LaunchCharacter(FVector(0, 0, Speed), false, true);
	JumpFeedback = JumpStage == 3 ? TEXT("TRIPLE JUMP!") : FString::Printf(TEXT("Jump %d / 3"), JumpStage);
}

void ANinjaCharacter::DoJumpEnd()
{
	bJumpButtonHeld = false;
	StopJumping();
}

void ANinjaCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	JumpStage = 0;
	PredictedApexTime = 0;
	JumpFeedback = TEXT("Ready - SPACE to jump");
}

void ANinjaCharacter::DoSprintStart()
{
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
}

void ANinjaCharacter::DoSprintEnd()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}
