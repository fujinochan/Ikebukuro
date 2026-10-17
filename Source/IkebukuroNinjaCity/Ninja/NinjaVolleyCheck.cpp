#include "NinjaVolleyCheck.h"
#include "NinjaCharacter.h"
#include "NinjaMatchDirector.h"
#include "NinjaPatrolArea.h"
#include "NinjaShuriken.h"
#include "NinjaExplosion.h"
#include "RedNinja.h"
#include "City/CityPlayArea.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ANinjaVolleyCheck::ANinjaVolleyCheck()
{ PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork; }
void ANinjaVolleyCheck::BeginPlay()
{
	Super::BeginPlay(); Started=FPlatformTime::Seconds();
	FParse::Value(FCommandLine::Get(),TEXT("VolleyFPS="),FPS); FPS=FMath::Clamp(FPS,30,120);
	FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./FPS);
}
void ANinjaVolleyCheck::Next(const FString& Case)
{ Cases.Add(Case); UE_LOG(LogTemp,Display,TEXT("VOLLEY_CASE %s"),*Case); ++Phase; PhaseTime=GetWorld()->GetTimeSeconds(); }
void ANinjaVolleyCheck::ClearStars()
{ for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) It->Destroy(); }
void ANinjaVolleyCheck::PlacePlayer(float Distance)
{
	Ninja->SetActorLocation(Enemy->GetActorLocation()+FVector(Distance,0,0));
	Ninja->GetCharacterMovement()->StopMovementImmediately(); Ninja->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
}
void ANinjaVolleyCheck::MakeWall(float Distance)
{
	if (!Wall)
	{
		Wall=GetWorld()->SpawnActor<AStaticMeshActor>(); Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);
	}
	Wall->SetActorLocation(Enemy->GetActorLocation()+FVector(Distance,0,15)); Wall->SetActorScale3D(FVector(.15,40,10));
}
ANinjaShuriken* ANinjaVolleyCheck::FireTestStar()
{
	auto* Shot=GetWorld()->SpawnActor<ANinjaShuriken>(Enemy->GetActorLocation()+FVector(70,0,15),FRotator::ZeroRotator);
	Shot->LaunchEnemy(Enemy,FVector::ForwardVector); return Shot;
}
void ANinjaVolleyCheck::Capture(const FString& Name)
{
	FString Prefix; if (FParse::Value(FCommandLine::Get(),TEXT("VolleyScreenshot="),Prefix)) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-")+Name+TEXT(".png"),false,false);
}
void ANinjaVolleyCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double WallTime=FPlatformTime::Seconds();
	if (bFinished) { if (WallTime-FinishedAt>2) FPlatformMisc::RequestExit(false); return; }
	if (WallTime-Started>150) { Finish(FString::Printf(TEXT("Timeout in phase %d"),Phase)); return; }
	if (!Match)
	{
		Match=ANinjaMatchDirector::Find(this); Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
		if (!Match || !Ninja) return;
	}
	const float Age=GetWorld()->GetTimeSeconds()-PhaseTime;
	if (Phase==0 && Match->State==ENinjaMatchState::Ready)
	{
		Enemy=Match->Enemies[0]; Teammate=Match->Enemies[1];
		for (TActorIterator<ACityPlayArea> It(GetWorld());It;++It) It->SetActorTickEnabled(false); // Deliberately place the isolated range target outside normal bounds.
		for (ARedNinja* E:Match->Enemies) if (E!=Enemy) E->SetActorTickEnabled(false);
		if (Ninja->Health!=1 || Enemy->TryFireVolley(Ninja) || Enemy->PatrolArea->PatrolExtentScale!=2 || Enemy->PatrolArea->PatrolTravelRadius!=800) { Finish(TEXT("Ready state, one-hit rule or doubled patrol tuning failed")); return; }
		const double Travel=FVector::Dist2D(Enemy->HomeFoot,Enemy->PatrolArea->PatrolGoal(Enemy->HomeFoot,0));
		if (Travel<=400 || Travel>801) { Finish(FString::Printf(TEXT("Ground patrol did not expand beyond 4m: %.1f"),Travel)); return; }
		Match->Confirm(); Next(TEXT("Ready state rejects fire; player starts with one life; safe ground patrol radius extends beyond the old 4m limit"));
	}
	else if (Phase==1 && Match->CanPlay())
	{
		Enemy->GetCharacterMovement()->DisableMovement(); Ninja->SetActorEnableCollision(false); PlacePlayer(10001);
		Camera=GetWorld()->SpawnActor<ACameraActor>();
		const FVector Eye=Enemy->GetActorLocation()+FVector(-700,-1200,650);
		Camera->SetActorLocation(Eye); Camera->SetActorRotation((Enemy->GetActorLocation()+FVector(500,0,0)-Eye).Rotation());
		UGameplayStatics::GetPlayerController(this,0)->SetViewTarget(Camera);
		Next(TEXT("Start normal AI ticks with player just outside the 100m range"));
	}
	else if (Phase==2 && Age>2.2f)
	{
		if (Enemy->VolleysFired!=0 || Enemy->TryFireVolley(Ninja)) { Finish(TEXT("Enemy fired beyond 100m")); return; }
		if (FMath::Abs(Enemy->GetCharacterMovement()->MaxWalkSpeed-320)>.1) { Finish(TEXT("Patrol walking speed is not doubled to 320cm/s")); return; }
		PlacePlayer(10000); Next(TEXT("No fire at 100.01m; patrol speed is doubled to 3.2m/s; enter exactly 100m"));
	}
	else if (Phase==3 && Enemy->VolleysFired==1)
	{
		TArray<float> Yaws;
		for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) if (It->IsHostile())
		{
			Yaws.Add(It->Movement->Velocity.Rotation().Yaw);
			if (It->Movement->ProjectileGravityScale!=0 || It->Movement->bIsHomingProjectile || It->MaximumRange<10000) { Finish(TEXT("Enemy shot is not a straight, non-homing long-range projectile")); return; }
		}
		Yaws.Sort(); if (Yaws.Num()!=3 || FMath::Abs(Yaws[0]+30)>.1 || FMath::Abs(Yaws[1])>.1 || FMath::Abs(Yaws[2]-30)>.1 || Enemy->TryFireVolley(Ninja)) { Finish(TEXT("Fan count/angles or immediate cooldown failed")); return; }
		FirstVolley=Enemy->LastVolleyTime; Next(TEXT("Real AI emits exactly three straight shots at -30/0/+30 degrees at 100m; immediate re-fire rejected"));
	}
	else if (Phase==4 && Age>.12f)
	{
		Capture(TEXT("fan")); Next(TEXT("Capture the red three-way fan"));
	}
	else if (Phase==5 && Enemy->VolleysFired>=2)
	{
		if (Enemy->VolleysFired!=2 || FMath::Abs(Enemy->LastVolleyTime-FirstVolley-2)>2.f/FPS+.01) { Finish(TEXT("Volley interval is not two simulation seconds")); return; }
		for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) { Star=*It; break; }
		PausedPosition=Star->GetActorLocation(); VolleyBase=Enemy->VolleysFired; Match->TogglePause(); PauseWall=WallTime;
		Next(TEXT("Second AI volley occurs two seconds later; pause with live projectiles"));
	}
	else if (Phase==6 && WallTime-PauseWall>.35)
	{
		if (!UGameplayStatics::IsGamePaused(this) || Enemy->VolleysFired!=VolleyBase || !Star->GetActorLocation().Equals(PausedPosition,.01)) { Finish(TEXT("Pause advanced shooting or projectile motion")); return; }
		Match->TogglePause(); PlacePlayer(10001); Next(TEXT("Pause freezes firing and projectiles; leave firing range after resume"));
	}
	else if (Phase==7 && Age>2.2f)
	{
		if (Enemy->VolleysFired!=VolleyBase) { Finish(TEXT("Enemy kept firing outside range")); return; }
		Enemy->SetActorTickEnabled(false); ClearStars();
		Enemy->SetActorLocation(Enemy->GetActorLocation()+FVector(0,0,2000)); // Keep collision fixtures clear of the practice map's existing walls.
		Teammate->GetCharacterMovement()->DisableMovement();
		Ninja->SetActorEnableCollision(true); PlacePlayer(4000);
		Teammate->SetActorLocation(Enemy->GetActorLocation()+FVector(500,0,0)); MakeWall(2000); Star=FireTestStar();
		Next(TEXT("Leaving range stops volleys; fire through a teammate toward a thin world wall"));
	}
	else if (Phase==8 && Age>.35f)
	{
		if (!IsValid(Star) || Star->GetActorLocation().X<Enemy->GetActorLocation().X+600 || Teammate->bDefeated) { Finish(TEXT("Friendly red ninja blocked or was damaged by a red shot")); return; }
		Next(TEXT("Hostile shot passes a red teammate without friendly damage or obstruction"));
	}
	else if (Phase==9 && Age>1.6f)
	{
		if (IsValid(Star) || Ninja->Health!=1 || !Match->CanPlay()) { Finish(TEXT("Thin wall did not stop the hostile projectile")); return; }
		MakeWall(35); PlacePlayer(1000);
		if (!Enemy->TryFireVolley(Ninja)) { Finish(TEXT("Muzzle wall test could not fire")); return; }
		for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) if (!It->IsActorBeingDestroyed()) { Finish(TEXT("Muzzle spawned a shot through a nearby wall")); return; }
		Wall->Destroy(); Wall=nullptr; Star=FireTestStar(); PlayerBurstBase=ANinjaExplosion::PlayerBursts;
		UGameplayStatics::GetPlayerController(this,0)->SetViewTarget(Ninja);
		UGameplayStatics::GetPlayerController(this,0)->SetControlRotation(FRotator(-10,180,0));
		Next(TEXT("15cm wall stops a travelling shot; muzzle sweep also prevents spawning through a close wall"));
	}
	else if (Phase==10 && Match->State==ENinjaMatchState::Lost)
	{
		if (Ninja->Health!=0 || !Ninja->IsHidden() || ANinjaExplosion::PlayerBursts!=PlayerBurstBase+1 || Match->ResultReason!=TEXT("One hit - ninja down")) { Finish(TEXT("One hostile hit did not cause a single knockout explosion and game loss")); return; }
		if (Ninja->ReceiveContactDamage()) { Finish(TEXT("Post-result damage was accepted")); return; }
		Next(TEXT("Actual swept hostile projectile causes one-hit loss and one large explosion; late damage rejected"));
	}
	else if (Phase==11 && Age>.5f)
	{
		Capture(TEXT("one-hit"));
		for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) { Finish(TEXT("Result retained player/enemy projectiles")); return; }
		Next(TEXT("Round end removes both factions' projectiles"));
	}
	else if (Phase==12 && Age>.15f)
	{
		Match->Confirm(); Next(TEXT("Retry the one-hit round"));
	}
	else if (Phase==13 && Match->State==ENinjaMatchState::Ready)
	{
		if (Ninja->Health!=1 || Ninja->IsHidden()) { Finish(TEXT("Retry did not restore the one-hit player")); return; }
		for (ARedNinja* E:Match->Enemies) if (!E->bRangedAttackEnabled || E->RangedAttackRadius!=10000 || E->VolleyInterval!=2 || E->VolleysFired!=0) { Finish(TEXT("Retry lost ranged tuning or retained volley counters")); return; }
		Next(TEXT("Retry restores all enemies, one life, 100m range, two-second interval and fresh firing state")); Finish(FString());
	}
}
void ANinjaVolleyCheck::Finish(const FString& Error)
{
	UGameplayStatics::SetGamePaused(this,false);
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed")); Report->SetStringField(TEXT("error"),Error); Report->SetNumberField(TEXT("fixed_fps"),FPS); Report->SetNumberField(TEXT("phase"),Phase);
	Report->SetStringField(TEXT("scope"),TEXT("Actual AI ticks and swept hostile projectiles; shooter motion disabled and player placed/flying to isolate 100m boundaries, angles, timing and collision; expanded ground/roof patrols are verified separately"));
	TArray<TSharedPtr<FJsonValue>> Rows; for (const auto& Case:Cases) Rows.Add(MakeShared<FJsonValueString>(Case)); Report->SetArrayField(TEXT("passed_cases"),Rows);
	FString Json,Path; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FParse::Value(FCommandLine::Get(),TEXT("VolleyReport="),Path); FFileHelper::SaveStringToFile(Json,*Path);
	UE_LOG(LogTemp,Display,TEXT("VOLLEY_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error); bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
