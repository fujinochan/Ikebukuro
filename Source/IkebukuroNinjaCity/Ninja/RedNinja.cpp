#include "RedNinja.h"
#include "NinjaCharacter.h"
#include "NinjaAppearance.h"
#include "NinjaPatrolArea.h"
#include "NinjaMatchDirector.h"
#include "NinjaExplosion.h"
#include "NinjaFeedback.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
ARedNinja::ARedNinja()
{
	GetCapsuleComponent()->InitCapsuleSize(42,90);
	GetCapsuleComponent()->CanCharacterStepUpOn=ECB_No;
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->MaxWalkSpeed=0;
	PrimaryActorTick.bCanEverTick=true;
	AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass=AAIController::StaticClass();
	GetCharacterMovement()->bOrientRotationToMovement=true;
	bUseControllerRotationYaw=false;
	GetCharacterMovement()->MaxStepHeight=35;
}
void ARedNinja::BeginPlay()
{
	Super::BeginPlay(); FNinjaAppearance::Apply(this,true);
	// Legacy checks isolate fixed city collision and the original stationary target.
	if (FParse::Param(FCommandLine::Get(),TEXT("CityAreaCheck")) || FParse::Param(FCommandLine::Get(),TEXT("CityTextureCheck")))
	{ GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); SetActorHiddenInGame(true); SetActorTickEnabled(false); return; }
	if (FParse::Param(FCommandLine::Get(),TEXT("NinjaCityCombatCheck"))) { SetActorTickEnabled(false); return; }
	if (bAIEnabled && !GetController()) SpawnDefaultController();
	SetActorTickEnabled(bAIEnabled && PatrolArea!=nullptr);
}
bool ARedNinja::TryDefeat(ENinjaDefeatMethod Method,ANinjaCharacter* Ninja)
{
	if (bDefeated || !IsValid(Ninja)) return false;
	auto* Match=ANinjaMatchDirector::Find(this);
	if (Match && !Match->CanDefeat(this)) return false;
	bDefeated=true; DefeatMethod=Method;
	ANinjaExplosion::Emit(this,GetActorLocation());
	ANinjaFeedback::Emit(this,GetActorLocation(),Method==ENinjaDefeatMethod::Stomp?TEXT("Stomp"):TEXT("Hit"));
	SetState(ENinjaBrainState::Defeated); SetActorTickEnabled(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement(); Ninja->RegisterDefeat(Method);
	if (Match) Match->CreditDefeat(this);
	// Retain identity briefly for a subsequent CharacterMovement landing callback.
	SetActorHiddenInGame(true); SetLifeSpan(0.8f); return true;
}

void ARedNinja::SetState(ENinjaBrainState State)
{
	if (BrainState!=State) { BrainState=State; Route.Reset(); RouteIndex=0; NextRouteTime=0; }
	VisitedStates|=1<<int32(State);
}
bool ARedNinja::CanSee(ANinjaCharacter* Player) const
{
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByObjectType(Hit,GetActorLocation(),Player->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(NinjaSight),true));
}
void ARedNinja::ReturnSafely()
{
	FVector Foot;
	if (!PatrolArea->Sample(HomeFoot,Foot)) { SetState(ENinjaBrainState::Blocked); GetCharacterMovement()->StopMovementImmediately(); return; }
	SetActorLocation(Foot+FVector(0,0,110));
	GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->ClearAccumulatedForces();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	++SafetyRecoveries; bReturning=true; SetState(ENinjaBrainState::Return);
}
void ARedNinja::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDefeated || !PatrolArea || !bAIEnabled) return;
	const double Now=GetWorld()->GetTimeSeconds();
	auto* Movement=GetCharacterMovement();
	FVector Foot=GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	if (!bInitialized)
	{
		if (Now<.5 || Movement->IsFalling()) return;
		if (!PatrolArea->bReady && !PatrolArea->BuildGraph()) { SetState(ENinjaBrainState::Blocked); return; }
		FVector Surface;
		if (!PatrolArea->Sample(Foot,Surface)) { SetState(ENinjaBrainState::Blocked); return; }
		HomeFoot=Surface; bInitialized=true; PatrolTarget=PatrolArea->PatrolGoal(HomeFoot,0); SetState(ENinjaBrainState::Patrol);
	}
	if (!ANinjaMatchDirector::AllowsGameplay(this)) { Movement->StopMovementImmediately(); return; }
	FVector Surface;
	if (Foot.Z<PatrolArea->GetActorLocation().Z-80 || !PatrolArea->Sample(Foot,Surface)) { ReturnSafely(); return; }
	if (Movement->IsFalling()) return;
	if (BrainState==ENinjaBrainState::Blocked && Now<NextRouteTime) { Movement->StopMovementImmediately(); return; }
	auto* Player=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
	if (!Player) return;
	const FVector PlayerFoot=Player->GetActorLocation()-FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const double Distance=FVector::Dist2D(Foot,PlayerFoot);
	TArray<FVector> Candidate;
	const bool bInLeash=FVector::Dist2D(HomeFoot,PlayerFoot)<=LeashRadius;
	const bool bOnLevel=FMath::Abs(PlayerFoot.Z-Foot.Z)<100;
	const bool bDetected=Distance<(BrainState==ENinjaBrainState::Chase || BrainState==ENinjaBrainState::Attack ? DetectionRadius*1.5 : DetectionRadius);
	const bool bTargetValid=!bReturning && Now>=ReacquireTime && bInLeash && bOnLevel && bDetected;
	FVector Goal;
	if (bTargetValid)
	{
		if (Now>=NextRouteTime || Route.IsEmpty())
		{
			if (!PatrolArea->FindRoute(Foot,PlayerFoot,Candidate)) { bReturning=true; SetState(ENinjaBrainState::Return); }
		}
		if (!bReturning)
		{
			SetState(Distance<=100 && CanSee(Player) ? ENinjaBrainState::Attack : ENinjaBrainState::Chase);
			Goal=PlayerFoot;
		}
	}
	else if (BrainState==ENinjaBrainState::Chase || BrainState==ENinjaBrainState::Attack) { bReturning=true; SetState(ENinjaBrainState::Return); }
	if (bReturning)
	{
		Goal=HomeFoot;
		if (FVector::Dist2D(Foot,HomeFoot)<90) { bReturning=false; ReacquireTime=Now+1; SetState(ENinjaBrainState::Patrol); }
	}
	if (!bReturning && BrainState!=ENinjaBrainState::Chase && BrainState!=ENinjaBrainState::Attack)
	{
		SetState(ENinjaBrainState::Patrol);
		if (FVector::Dist2D(Foot,PatrolTarget)<90)
		{ PatrolTarget=PatrolArea->PatrolGoal(HomeFoot,++PatrolIndex); Route.Reset(); RouteIndex=0; NextRouteTime=0; }
		Goal=PatrolTarget;
	}
	if (BrainState==ENinjaBrainState::Attack)
	{
		Movement->StopMovementImmediately();
		if (Now>=NextAttackTime) { if (Player->ReceiveContactDamage()) ++SuccessfulAttacks; NextAttackTime=Now+1.25; }
		return;
	}
	if (Now>=NextRouteTime || Route.IsEmpty())
	{
		NextRouteTime=Now+.6; RouteIndex=0;
		if (!PatrolArea->FindRoute(Foot,Goal,Route)) { SetState(ENinjaBrainState::Blocked); NextRouteTime=Now+.6; Movement->StopMovementImmediately(); return; }
	}
	while (RouteIndex<Route.Num() && FVector::Dist2D(Foot,Route[RouteIndex])<55) ++RouteIndex;
	if (!Route.IsValidIndex(RouteIndex)) { Movement->StopMovementImmediately(); return; }
	const FVector Direction=(Route[RouteIndex]-Foot).GetSafeNormal2D();
	const float Speed=BrainState==ENinjaBrainState::Chase ? 420 : bReturning ? 260 : 160;
	FVector Next;
	if (!PatrolArea->Sample(Foot+Direction*(Speed*DeltaSeconds+45),Next) || !PatrolArea->ClearSegment(Foot,Next))
	{ SetState(ENinjaBrainState::Blocked); Movement->StopMovementImmediately(); NextRouteTime=Now+.6; return; }
	Movement->MaxWalkSpeed=Speed; AddMovementInput(Direction,1);
}
