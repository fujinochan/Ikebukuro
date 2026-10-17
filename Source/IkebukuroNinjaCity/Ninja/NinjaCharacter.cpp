#include "NinjaCharacter.h"
#include "NinjaAppearance.h"
#include "RedNinja.h"
#include "NinjaShuriken.h"
#include "NinjaMatchDirector.h"
#include "NinjaExplosion.h"
#include "NinjaFeedback.h"
#include "City/CityPlayArea.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

ANinjaCharacter::ANinjaCharacter()
{
	JumpMaxHoldTime = 0;
}

void ANinjaCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCameraBoom()->SocketOffset=CameraShoulderOffset;
	FNinjaAppearance::Apply(this,false);
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this,&ANinjaCharacter::OnNinjaHit);
	SafeSurface=GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	PreviousFeet=SafeSurface;
	bCityHandlesRecovery=UGameplayStatics::GetActorOfClass(this,ACityPlayArea::StaticClass())!=nullptr;
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
	auto* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (Input && MoveAction)
	{
		Input->BindAction(MoveAction,ETriggerEvent::Completed,this,&ANinjaCharacter::EndMoveInput);
		Input->BindAction(MoveAction,ETriggerEvent::Canceled,this,&ANinjaCharacter::EndMoveInput);
	}
	if (Input && ThrowAction) Input->BindAction(ThrowAction,ETriggerEvent::Started,this,&ANinjaCharacter::DoThrow);
	if (Input && SprintAction)
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
	if (!ANinjaMatchDirector::AllowsGameplay(this)) return;
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
	ANinjaFeedback::Emit(this,GetActorLocation()-FVector(0,0,75),JumpStage==3?TEXT("Triple"):TEXT("Jump"));
	JumpFeedback = JumpStage == 3 ? TEXT("TRIPLE JUMP!") : FString::Printf(TEXT("Jump %d / 3"), JumpStage);
}

void ANinjaCharacter::DoJumpEnd()
{
	bJumpButtonHeld = false;
	StopJumping();
}

void ANinjaCharacter::Landed(const FHitResult& Hit)
{
	if (auto* Enemy=Cast<ARedNinja>(Hit.GetActor()); Enemy && (Enemy->bDefeated || TryStomp(Enemy,Hit))) return;
	Super::Landed(Hit);
	JumpStage = 0;
	PredictedApexTime = 0;
	JumpFeedback = TEXT("Ready - SPACE to jump");
}

void ANinjaCharacter::DoSprintStart()
{
	if (!ANinjaMatchDirector::AllowsGameplay(this)) return;
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
}

void ANinjaCharacter::DoSprintEnd()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ANinjaCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAutoCamera(DeltaSeconds);
	if (ANinjaMatchDirector::AllowsGameplay(this) && !bCityHandlesRecovery && GetActorLocation().Z < -1500) RecoverToSurface(SafeSurface,true);
	if (ANinjaMatchDirector::AllowsGameplay(this) && !bCityHandlesRecovery)
		if (auto* PC=Cast<APlayerController>(GetController()); PC && PC->WasInputKeyJustPressed(EKeys::R)) RecoverToSurface(SafeSurface,false);
	PreviousVerticalVelocity=GetVelocity().Z;
	PreviousFeet=GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void ANinjaCharacter::DoThrow() { TryThrowShuriken(); }

bool ANinjaCharacter::TryThrowShuriken()
{
	if (!ANinjaMatchDirector::AllowsGameplay(this) || Health<=0) return false;
	const double Now=GetWorld()->GetTimeSeconds();
	AimCameraUntil=Now+.6; // Keep repeated throws on the manually selected aim.
	if (Now<NextThrowTime) return false;
	auto* PC=Cast<APlayerController>(GetController());
	if (!PC) return false;
	FVector Camera; FRotator Look; PC->GetPlayerViewPoint(Camera,Look);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NinjaAim),true,this);
	FHitResult Aim;
	const FVector FarPoint=Camera+Look.Vector()*10000;
	const FVector AimPoint=GetWorld()->LineTraceSingleByChannel(Aim,Camera,FarPoint,ECC_Visibility,Params)?Aim.ImpactPoint:FarPoint;
	const FVector Chest=GetActorLocation()+FVector(0,0,20);
	const FVector Hand=GetMesh()->DoesSocketExist(TEXT("hand_r"))?GetMesh()->GetSocketLocation(TEXT("hand_r")):Chest+GetActorRightVector()*45;
	const FVector Direction=(AimPoint-Hand).GetSafeNormal();
	FHitResult Block;
	const bool bBlocked=GetWorld()->SweepSingleByChannel(Block,Chest,Hand+Direction*12,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8),Params);
	FActorSpawnParameters Spawn; Spawn.Owner=this; Spawn.Instigator=this;
	Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Star=GetWorld()->SpawnActor<ANinjaShuriken>(bBlocked?Block.Location:Hand,Direction.Rotation(),Spawn);
	if (!Star) return false;
	Star->Launch(this,Direction);
	NextThrowTime=Now+0.3; ++ShotsFired; ANinjaFeedback::Emit(this,GetActorLocation(),TEXT("Throw"));
	if (bBlocked) Star->Impact(Block);
	return true;
}

bool ANinjaCharacter::TryStomp(ARedNinja* Enemy,const FHitResult& Hit)
{
	if (!ANinjaMatchDirector::AllowsGameplay(this)) return false;
	if (!Enemy || Enemy->bDefeated) return false;
	const float Top=Enemy->GetActorLocation().Z+Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	// A previous descending frame may precede a bounce. Never accept an actual
	// upward velocity as a stomp; retain the previous speed only for impact clipping.
	if (GetVelocity().Z>1 || FMath::Min(GetVelocity().Z,PreviousVerticalVelocity)>=-50 || Hit.ImpactNormal.Z<0.5 || Hit.ImpactPoint.Z<Top-25 || PreviousFeet.Z<Top-30) return false;
	if (!Enemy->TryDefeat(ENinjaDefeatMethod::Stomp,this)) return false;
	LaunchCharacter(FVector(0,0,650),false,true); return true;
}

void ANinjaCharacter::OnNinjaHit(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,FVector,const FHitResult& Hit)
{
	if (auto* Enemy=Cast<ARedNinja>(Other); Enemy && !Enemy->bDefeated)
	{
		if (TryStomp(Enemy,Hit)) return;
		const int32 PreviousKnockouts=Knockouts;
		if (ReceiveContactDamage() && Knockouts==PreviousKnockouts)
		{
			const FVector Away=(GetActorLocation()-Enemy->GetActorLocation()).GetSafeNormal2D();
			LaunchCharacter(Away*350+FVector(0,0,300),true,true);
		}
	}
}

bool ANinjaCharacter::IsInvulnerable() const { return GetWorld()->GetTimeSeconds()<InvulnerableUntil; }

bool ANinjaCharacter::ReceiveContactDamage()
{
	if (!ANinjaMatchDirector::AllowsGameplay(this) || Health<=0 || IsInvulnerable()) return false;
	ANinjaFeedback::Emit(this,GetActorLocation(),TEXT("Damage"));
	Health=FMath::Max(0,Health-1); InvulnerableUntil=GetWorld()->GetTimeSeconds()+1;
	CombatFeedback=TEXT("Hit! One second of protection");
	if (Health==0)
	{
		++Knockouts; ANinjaExplosion::Emit(this,GetActorLocation(),true);
		if (ANinjaMatchDirector::Find(this)) { SetActorHiddenInGame(true); return true; }
		Health=3; RecoverToSurface(SafeSurface,false);
		CombatFeedback=TEXT("Knocked out - returned with full health");
	}
	return true;
}

void ANinjaCharacter::RegisterDefeat(ENinjaDefeatMethod Method)
{
	if (!ANinjaMatchDirector::AllowsGameplay(this)) return;
	++Defeats;
	if (Method==ENinjaDefeatMethod::Stomp) { ++StompDefeats; CombatFeedback=TEXT("STOMP! Red ninja defeated"); }
	else { ++ShurikenDefeats; CombatFeedback=TEXT("SHURIKEN HIT! Red ninja defeated"); }
}

void ANinjaCharacter::SetSafeSurface(FVector Surface) { SafeSurface=Surface; }

void ANinjaCharacter::RecoverToSurface(FVector Surface,bool bFallPenalty)
{
	if (!ANinjaMatchDirector::AllowsGameplay(this)) return;
	SetSafeSurface(Surface);
	if (bFallPenalty)
	{
		ANinjaFeedback::Emit(this,GetActorLocation(),TEXT("Damage"));
		Health=FMath::Max(0,Health-1);
		if (Health==0) { ++Knockouts; ANinjaExplosion::Emit(this,GetActorLocation(),true); if (ANinjaMatchDirector::Find(this)) { SetActorHiddenInGame(true); return; } Health=3; }
		InvulnerableUntil=GetWorld()->GetTimeSeconds()+1;
	}
	SetActorLocation(Surface+FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+20));
	GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->ClearAccumulatedForces(); GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	JumpStage=0; PredictedApexTime=0;
	CombatFeedback=bFallPenalty?TEXT("Fell - lost one health, returned to safety"):TEXT("Returned to safety");
}

void ANinjaCharacter::DoMove(float Right,float Forward)
{
	if (!ANinjaMatchDirector::AllowsGameplay(this) || !GetController()) return;
	const FVector2D Input(Right,Forward);
	if (Input.IsNearlyZero()) { EndMoveInput(); return; }
	// Freeze the input's world frame while a direction is held. Otherwise auto-yaw
	// would rotate camera-relative input again every frame and make D/S orbit forever.
	if (!bHasMovementReference || !Input.GetSafeNormal().Equals(CameraMoveInput.GetSafeNormal(),.1f) ||
		GetWorld()->GetTimeSeconds()<ManualCameraUntil)
		MovementReferenceYaw=GetController()->GetControlRotation().Yaw;
	bHasMovementReference=true; CameraMoveInput=Input;
	const FRotationMatrix Basis(FRotator(0,MovementReferenceYaw,0));
	AddMovementInput(Basis.GetUnitAxis(EAxis::X),Forward);
	AddMovementInput(Basis.GetUnitAxis(EAxis::Y),Right);
}
void ANinjaCharacter::EndMoveInput()
{ bHasMovementReference=false; CameraMoveInput=FVector2D::ZeroVector; }
void ANinjaCharacter::UpdateAutoCamera(float DeltaSeconds)
{
	if (!ANinjaMatchDirector::AllowsGameplay(this) || Health<=0 || !GetController() ||
		!GetController()->IsLocalPlayerController()) return;
	const double Now=GetWorld()->GetTimeSeconds();
	if (bHasMovementReference && Now<ManualCameraUntil)
		MovementReferenceYaw=GetController()->GetControlRotation().Yaw;
	if (!bAutoAdjustCamera || !bHasMovementReference || GetVelocity().SizeSquared2D()<FMath::Square(120.f) ||
		Now<ManualCameraUntil || Now<AimCameraUntil || DeltaSeconds<=0) return;
	FRotator View=GetController()->GetControlRotation();
	const float Error=FMath::FindDeltaAngleDegrees(View.Yaw,GetActorRotation().Yaw);
	if (FMath::Abs(Error)<2.f) return;
	const float Step=Error*(1-FMath::Exp(-FMath::Max(0.f,CameraFollowSpeed)*DeltaSeconds));
	const float Limit=FMath::Max(0.f,CameraMaxYawSpeed)*DeltaSeconds;
	View.Yaw=FRotator::NormalizeAxis(View.Yaw+FMath::Clamp(Step,-Limit,Limit));
	GetController()->SetControlRotation(View); // Preserve the player's vertical view.
}
void ANinjaCharacter::DoLook(float Yaw,float Pitch)
{
	if (!ANinjaMatchDirector::AllowsGameplay(this)) return;
	if (!FMath::IsNearlyZero(Yaw) || !FMath::IsNearlyZero(Pitch))
		ManualCameraUntil=GetWorld()->GetTimeSeconds()+CameraManualGraceSeconds;
	Super::DoLook(Yaw,Pitch);
}
void ANinjaCharacter::ResetForMatch(FVector Foot,FRotator Rotation)
{
	SetActorHiddenInGame(false);
	EndMoveInput(); ManualCameraUntil=AimCameraUntil=0;
	Health=3; Defeats=StompDefeats=ShurikenDefeats=ShotsFired=ShurikenWorldHits=Knockouts=0;
	JumpStage=0; PredictedApexTime=NextThrowTime=InvulnerableUntil=0;
	bJumpButtonHeld=false; DoJumpEnd(); DoSprintEnd();
	SafeSurface=PreviousFeet=Foot; PreviousVerticalVelocity=0;
	SetActorLocation(Foot+FVector(0,0,110));
	GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->ClearAccumulatedForces(); GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (GetController()) GetController()->SetControlRotation(Rotation);
	JumpFeedback=TEXT("Ready - SPACE to jump"); CombatFeedback=TEXT("F / LMB: shuriken  |  Stomp red ninjas from above");
}
