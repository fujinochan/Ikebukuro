#include "NinjaMatchDirector.h"
#include "NinjaCharacter.h"
#include "RedNinja.h"
#include "NinjaShuriken.h"
#include "City/LocalCityTileset.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

ANinjaMatchDirector::ANinjaMatchDirector()
{
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true;
	// Resolve outcomes after physics/projectiles so the last kill wins a tied deadline.
	PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}
ANinjaMatchDirector* ANinjaMatchDirector::Find(const UObject* Context)
{
	if (!Context || !Context->GetWorld()) return nullptr;
	for (TActorIterator<ANinjaMatchDirector> It(Context->GetWorld());It;++It) if (It->bActive) return *It;
	return nullptr;
}
bool ANinjaMatchDirector::AllowsGameplay(const UObject* Context)
{
	const auto* Match=Find(Context); return !Match || Match->CanPlay();
}
bool ANinjaMatchDirector::CanPlay() const { return State==ENinjaMatchState::Playing && !UGameplayStatics::IsGamePaused(this); }
void ANinjaMatchDirector::BeginPlay()
{
	Super::BeginPlay();
	for (const TCHAR* Flag:{TEXT("CityAreaCheck"),TEXT("CityTextureCheck"),TEXT("NinjaEncounterCheck"),TEXT("NinjaCombatCheck"),TEXT("NinjaCityCombatCheck")})
		if (FParse::Param(FCommandLine::Get(),Flag)) bActive=false;
	SetActorTickEnabled(bActive); if (!bActive) return;
	Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
	for (TActorIterator<ARedNinja> It(GetWorld());It;++It)
	{
		FNinjaMatchSpawn Spawn; Spawn.Class=It->GetClass(); Spawn.Transform=It->GetActorTransform();
		Spawn.Area=It->PatrolArea; Spawn.Tags=It->Tags; Spawn.bAI=It->bAIEnabled;
		Spawns.Add(Spawn); Enemies.Add(*It);
	}
	if (auto* PC=UGameplayStatics::GetPlayerController(this,0))
	{
		EnableInput(PC);
		InputComponent->BindKey(EKeys::Enter,IE_Pressed,this,&ANinjaMatchDirector::Confirm).bExecuteWhenPaused=true;
		InputComponent->BindKey(EKeys::P,IE_Pressed,this,&ANinjaMatchDirector::TogglePause).bExecuteWhenPaused=true;
		InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&ANinjaMatchDirector::TogglePause).bExecuteWhenPaused=true;
	}
}
bool ANinjaMatchDirector::IsPrepared() const
{
	if (!Ninja || !Ninja->GetCharacterMovement()->IsMovingOnGround() || Enemies.IsEmpty()) return false;
	for (ARedNinja* Enemy:Enemies) if (!IsValid(Enemy) || Enemy->GetCharacterMovement()->IsFalling() || !Enemy->IsReadyForMatch()) return false;
	for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It)
	{
		if (It->GetLoadProgress()<99) return false;
		TArray<UStaticMeshComponent*> Meshes; It->GetComponents(Meshes);
		if (!Meshes.ContainsByPredicate([](auto* Mesh){return Mesh->GetStaticMesh() && Mesh->IsVisible();})) return false;
	}
	FHitResult Hit; const FVector Feet=Ninja->GetActorLocation()-FVector(0,0,Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return GetWorld()->LineTraceSingleByObjectType(Hit,Feet+FVector(0,0,10),Feet-FVector(0,0,30),FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(MatchStartFloor),true));
}
void ANinjaMatchDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (UGameplayStatics::IsGamePaused(this)) return;
	if (!Ninja) Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
	if (State==ENinjaMatchState::Loading)
	{
		PreparedSeconds=IsPrepared()?PreparedSeconds+DeltaSeconds:0;
		if (PreparedSeconds>=.5)
		{
			StartFoot=Ninja->GetActorLocation()-FVector(0,0,Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			if (RoundNumber==1) StartRotation=Ninja->GetController()->GetControlRotation();
			State=ENinjaMatchState::Ready;
		}
	}
	else if (State==ENinjaMatchState::Countdown)
	{
		CountdownSeconds=FMath::Max(0.f,CountdownSeconds-DeltaSeconds);
		if (CountdownSeconds<=0) State=ENinjaMatchState::Playing;
	}
	else if (State==ENinjaMatchState::Playing)
	{
		RemainingSeconds=FMath::Max(0.f,RemainingSeconds-DeltaSeconds);
		if (RemainingEnemies()==0) Finish(true,TEXT("All red ninjas defeated"));
		else if (Ninja->Health<=0) Finish(false,TEXT("No health remaining"));
		else if (RemainingSeconds<=0) Finish(false,TEXT("Time is up"));
	}
}
bool ANinjaMatchDirector::CanDefeat(ARedNinja* Enemy) const { return CanPlay() && Enemies.Contains(Enemy) && !Credited.Contains(Enemy); }
void ANinjaMatchDirector::CreditDefeat(ARedNinja* Enemy) { if (CanDefeat(Enemy)) Credited.Add(Enemy); }
void ANinjaMatchDirector::Confirm()
{
	if (State==ENinjaMatchState::Ready) { CountdownSeconds=3; State=ENinjaMatchState::Countdown; }
	else if (State==ENinjaMatchState::Won || State==ENinjaMatchState::Lost) Retry();
}
void ANinjaMatchDirector::TogglePause()
{
	if (State==ENinjaMatchState::Playing || State==ENinjaMatchState::Countdown)
		UGameplayStatics::SetGamePaused(this,!UGameplayStatics::IsGamePaused(this));
}
void ANinjaMatchDirector::Finish(bool bWon,const FString& Reason)
{
	State=bWon?ENinjaMatchState::Won:ENinjaMatchState::Lost; ResultReason=Reason;
	Ninja->GetCharacterMovement()->StopMovementImmediately(); Ninja->GetCharacterMovement()->ClearAccumulatedForces(); Ninja->GetCharacterMovement()->DisableMovement();
	Ninja->DoJumpEnd(); Ninja->DoSprintEnd();
	for (ARedNinja* Enemy:Enemies) if (IsValid(Enemy))
	{ Enemy->GetCharacterMovement()->StopMovementImmediately(); Enemy->GetCharacterMovement()->ClearAccumulatedForces(); Enemy->GetCharacterMovement()->DisableMovement(); }
	for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) It->Destroy();
}
void ANinjaMatchDirector::Retry()
{
	if (State!=ENinjaMatchState::Won && State!=ENinjaMatchState::Lost) return;
	UGameplayStatics::SetGamePaused(this,false); State=ENinjaMatchState::Loading;
	for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) It->Destroy();
	for (ARedNinja* Enemy:Enemies) if (IsValid(Enemy)) Enemy->Destroy();
	Enemies.Reset(); Credited.Reset(); RemainingSeconds=300; CountdownSeconds=3; PreparedSeconds=0; ResultReason.Reset(); ++RoundNumber;
	Ninja->ResetForMatch(StartFoot,StartRotation);
	for (const auto& Spawn:Spawns)
	{
		auto* Enemy=GetWorld()->SpawnActorDeferred<ARedNinja>(Spawn.Class,Spawn.Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Enemy->PatrolArea=Spawn.Area; Enemy->Tags=Spawn.Tags; Enemy->bAIEnabled=Spawn.bAI;
		Enemies.Add(Enemy); Enemy->FinishSpawning(Spawn.Transform);
	}
}
void ANinjaMatchDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bActive) UGameplayStatics::SetGamePaused(this,false);
	Super::EndPlay(Reason);
}
