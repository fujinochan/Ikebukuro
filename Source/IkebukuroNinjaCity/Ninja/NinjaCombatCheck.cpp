#include "NinjaCombatCheck.h"
#include "NinjaCharacter.h"
#include "RedNinja.h"
#include "NinjaShuriken.h"
#include "City/LocalCityTileset.h"
#include "Camera/CameraActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ANinjaCombatCheck::ANinjaCombatCheck() { PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostPhysics; }
void ANinjaCombatCheck::BeginPlay()
{
	Super::BeginPlay();
	bCityMode=FParse::Param(FCommandLine::Get(),TEXT("NinjaCityCombatCheck"));
	SetActorTickEnabled(bCityMode || FParse::Param(FCommandLine::Get(),TEXT("NinjaCombatCheck")));
	if (!IsActorTickEnabled()) return;
	FParse::Value(FCommandLine::Get(),TEXT("CombatFPS="),FPS); FPS=FMath::Clamp(FPS,30,120);
	FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1.0/FPS);
	RealStart=FPlatformTime::Seconds();
}
void ANinjaCombatCheck::Key(FKey Input,bool Pressed)
{
	auto* PC=UGameplayStatics::GetPlayerController(this,0);
	PC->InputKey(FInputKeyEventArgs(nullptr,INPUTDEVICEID_NONE,Input,Pressed?IE_Pressed:IE_Released,Pressed?1.f:0.f,false,FPlatformTime::Cycles64()));
}
void ANinjaCombatCheck::AimAt(FVector Target)
{
	auto* PC=UGameplayStatics::GetPlayerController(this,0);
	FVector Camera; FRotator Rotation; PC->GetPlayerViewPoint(Camera,Rotation);
	PC->SetControlRotation((Target-Camera).Rotation());
}
void ANinjaCombatCheck::Place(FVector Position,FVector Velocity)
{
	Ninja->SetActorLocation(Position); Ninja->GetCharacterMovement()->StopMovementImmediately();
	Ninja->GetCharacterMovement()->ClearAccumulatedForces();
	Ninja->GetCharacterMovement()->SetMovementMode(MOVE_Falling); Ninja->GetCharacterMovement()->Velocity=Velocity;
}
ARedNinja* ANinjaCombatCheck::EnemyAt(FVector Position)
{
	UClass* Class=LoadClass<ARedNinja>(nullptr,TEXT("/Game/Ninja/Blueprints/BP_RedNinja.BP_RedNinja_C"));
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Result=GetWorld()->SpawnActor<ARedNinja>(Class,Position,FRotator(0,180,0),Params);
	Temporary.Add(Result); return Result;
}
ANinjaShuriken* ANinjaCombatCheck::StarAt(FVector Position,float Speed)
{
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Result=GetWorld()->SpawnActor<ANinjaShuriken>(Position,FRotator::ZeroRotator,Params);
	Result->Launch(Ninja,FVector::ForwardVector,Speed); Temporary.Add(Result); return Result;
}
void ANinjaCombatCheck::StartCase()
{
	Key(EKeys::W,false); Key(EKeys::LeftShift,false);
	for (const auto& Actor:Temporary) if (IsValid(Actor)) Actor->Destroy(); Temporary.Reset();
	Enemy=nullptr; Rear=nullptr; Star=nullptr; Phase=0; bSawBounce=false;
	auto* PC=UGameplayStatics::GetPlayerController(this,0); PC->SetViewTarget(Ninja); PC->SetControlRotation(FRotator::ZeroRotator);
	Place(FVector(0,-3000,92)); Ninja->DoJumpEnd(); Ninja->JumpStage=0;
	Ninja->Health=3; Ninja->SetSafeSurface(FVector(0,-3000,0));
	CountBefore=Ninja->Defeats; ShotsBefore=Ninja->ShotsFired; HealthBefore=Ninja->Health;
	StartTime=GetWorld()->GetTimeSeconds(); PhaseTime=StartTime;
	if (Case==0) Enemy=EnemyAt(FVector(900,-3000,92));
	if (Case==1) { Enemy=EnemyAt(FVector(600,-3000,92)); Rear=EnemyAt(FVector(1200,-3000,92)); }
	if (Case==2)
	{
		Enemy=EnemyAt(FVector(1000,-3000,92));
		auto* Wall=GetWorld()->SpawnActor<AStaticMeshActor>(FVector(450,-3000,90),FRotator::ZeroRotator);
		Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Wall->SetActorScale3D(FVector(.15,5,1.8)); Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll")); Temporary.Add(Wall);
		const FVector Camera(-500,-3000,500);
		auto* View=GetWorld()->SpawnActor<ACameraActor>(Camera,(Enemy->GetActorLocation()-Camera).Rotation());
		Temporary.Add(View); PC->SetViewTarget(View);
	}
	if (Case==3) { Place(FVector(0,-3000,1000)); Star=StarAt(FVector(100,-3000,1000),5000); }
	if (Case==4) { Enemy=EnemyAt(FVector(0,-3000,92)); Place(FVector(0,-3000,580),FVector(0,0,-300)); Ninja->JumpStage=2; }
	if (Case==5) Enemy=EnemyAt(FVector(500,-3000,92));
	if (Case==6)
	{
		Enemy=EnemyAt(FVector(0,-3000,600)); Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
	if (Case==7) Place(FVector(0,-3000,-2000));
}
void ANinjaCombatCheck::Pass(const FString& Name)
{
	Results.Add(Name); UE_LOG(LogTemp,Display,TEXT("COMBAT_CASE_PASSED %s"),*Name);
	++Case;
	if (Case>=10) { Finish(FString()); return; }
	StartCase();
}
void ANinjaCombatCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished) { if (FPlatformTime::Seconds()-FinishedAt>3) FPlatformMisc::RequestExit(false); return; }
	if (!Ninja)
	{
		Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
		if (!Ninja || GetWorld()->GetTimeSeconds()<1) { Ninja=nullptr; return; }
		if (!bCityMode) StartCase();
		else
		{
			for (TActorIterator<ARedNinja> It(GetWorld());It;++It) { Enemy=*It; break; }
			if (!Enemy) { Finish(TEXT("City enemy missing")); return; }
			PhaseTime=GetWorld()->GetTimeSeconds();
		}
	}
	if (FPlatformTime::Seconds()-RealStart>100) { Finish(TEXT("Combat wall-clock timeout")); return; }
	if (bCityMode) { TickCity(); return; }
	const double Now=GetWorld()->GetTimeSeconds(), Elapsed=Now-StartTime, Step=Now-PhaseTime;
	if (Elapsed>8) { Finish(FString::Printf(TEXT("Case %d timed out; phase=%d health=%d stage=%d z=%.1f"),Case,Phase,Ninja->Health,Ninja->JumpStage,Ninja->GetActorLocation().Z)); return; }
	if (Case==0)
	{
		if (Phase==0) AimAt(Enemy->GetActorLocation());
		if (Phase==0 && Elapsed>.5) { Key(EKeys::F,true); Phase=1; PhaseTime=Now; }
		else if (Phase==1 && Step>.05)
		{
			Key(EKeys::F,false);
			if (Ninja->ShotsFired!=ShotsBefore+1 || Ninja->TryThrowShuriken()) { Finish(TEXT("F input or throw cooldown failed")); return; }
			Phase=2;
		}
		else if (Phase==2 && Enemy->bDefeated)
		{
			if (Ninja->ShurikenDefeats<1 || Ninja->Health!=3) { Finish(TEXT("Shuriken scoring or owner ignore failed")); return; }
			Pass(TEXT("Mapped F throw, cooldown, owner ignored, shuriken defeat"));
		}
	}
	else if (Case==1)
	{
		if (Phase==0 && Elapsed>.4) { Star=StarAt(FVector(0,-3000,100),20000); Phase=1; PhaseTime=Now; }
		else if (Phase==1 && Step>.15)
		{
			if (!Enemy->bDefeated || Rear->bDefeated || Ninja->Defeats!=CountBefore+1) { Finish(TEXT("High speed sweep or first-target stop failed")); return; }
			const int32 Before=Ninja->Defeats;
			if (Enemy->TryDefeat(ENinjaDefeatMethod::Stomp,Ninja) || Ninja->Defeats!=Before) { Finish(TEXT("Duplicate defeat accepted")); return; }
			Pass(TEXT("200m/s swept shuriken and duplicate defeat guard"));
		}
	}
	else if (Case==2)
	{
		if (Phase==0 && Elapsed>.5) { Ninja->TryThrowShuriken(); Phase=1; PhaseTime=Now; }
		else if (Phase==1 && Step>.5)
		{
			if (Enemy->bDefeated || Ninja->Defeats!=CountBefore || Ninja->ShurikenWorldHits<1) { Finish(TEXT("Camera-over-wall hand trajectory bypassed wall")); return; }
			Pass(TEXT("Camera can see target but hand trajectory stops at 15cm wall"));
		}
	}
	else if (Case==3)
	{
		if (Phase==0 && Elapsed>.35)
		{
			if (!IsValid(Star) || FMath::Abs(Star->GetActorLocation().Z-1000)>.1 || Star->GetActorLocation().X<1500) { Finish(TEXT("Projectile is not straight")); return; }
			if (!Ninja->GetCharacterMovement()->IsFalling()) { Finish(TEXT("Air throw setup failed")); return; }
			Key(EKeys::LeftMouseButton,true); Phase=1; PhaseTime=Now;
		}
		else if (Phase==1 && Step>.05) { Key(EKeys::LeftMouseButton,false); Phase=2; }
		else if (Phase==2 && Elapsed>1.4)
		{
			if (IsValid(Star) || Ninja->ShotsFired!=ShotsBefore+1) { Finish(TEXT("Range expiry or airborne mouse input failed")); return; }
			Pass(TEXT("Gravity-free straight flight, 60m expiry and airborne LMB input"));
		}
	}
	else if (Case==4)
	{
		if (Phase==0 && Enemy->bDefeated)
		{
			if (Ninja->JumpStage!=2 || Ninja->Defeats!=CountBefore+1) { Finish(TEXT("Stomp reset jump stage or score failed")); return; }
			Phase=1;
		}
		if (Phase==1 && Ninja->GetVelocity().Z>400) bSawBounce=true;
		if (Phase==1 && bSawBounce && !Ninja->GetCharacterMovement()->IsFalling())
		{
			if (Ninja->JumpStage!=0) { Finish(TEXT("Ground did not recharge jump after stomp")); return; }
			Pass(TEXT("Descending top stomp, bounce, stage preserved until real ground"));
		}
	}
	else if (Case==5)
	{
		if (Phase==0 && Elapsed>1.2) { Key(EKeys::W,true); Key(EKeys::LeftShift,true); Phase=1; }
		if (Phase==1 && Ninja->Health<3)
		{
			if (Enemy->bDefeated || Ninja->Defeats!=CountBefore || Ninja->Health!=2 || Ninja->ReceiveContactDamage()) { Finish(TEXT("Side contact or invulnerability failed")); return; }
			Pass(TEXT("Side contact damages once, never stomps, repeated damage rejected"));
		}
	}
	else if (Case==6)
	{
		if (Phase==0 && Elapsed>1.2) { Place(FVector(0,-3000,92)); Ninja->LaunchCharacter(FVector(0,0,1100),true,true); Ninja->JumpStage=1; Phase=1; PhaseTime=Now; }
		else if (Phase==1 && Step>.8)
		{
			if (Enemy->bDefeated || Ninja->Defeats!=CountBefore || Ninja->Health==3) { Finish(FString::Printf(TEXT("Ascending underside: defeated=%d health=%d player=%s enemy=%s"),Enemy->bDefeated,Ninja->Health,*Ninja->GetActorLocation().ToString(),*Enemy->GetActorLocation().ToString())); return; }
			Pass(TEXT("Ascending underside contact does not defeat enemy"));
		}
	}
	else if (Case==7 && Elapsed>.2)
	{
		if (Ninja->Health!=2 || FVector::Dist2D(Ninja->GetActorLocation(),FVector(0,-3000,0))>10 || Ninja->GetActorLocation().Z<0) { Finish(TEXT("Fall recovery penalty failed")); return; }
		Pass(TEXT("Fall recovers to checkpoint and costs one health"));
	}
	else if (Case==8)
	{
		if (Phase==0 && Elapsed>1.2) { Ninja->Health=1; HealthBefore=Ninja->Knockouts; Place(FVector(2000,-3000,92)); Ninja->ReceiveContactDamage(); Phase=1; PhaseTime=Now; }
		else if (Phase==1 && Step>.2)
		{
			if (Ninja->Knockouts!=HealthBefore+1 || Ninja->Health!=3 || FVector::Dist2D(Ninja->GetActorLocation(),FVector(0,-3000,0))>10) { Finish(TEXT("Zero-health recovery failed")); return; }
			Pass(TEXT("Zero health recovers once with full health"));
		}
	}
	else if (Case==9 && Elapsed>.2)
	{
		for (int32 Order=0; Order<2; ++Order)
		{
			auto* Target=EnemyAt(FVector(2000+Order*500,-3000,92));
			auto* Projectile=StarAt(Target->GetActorLocation()-FVector(100,0,0),5000);
			FHitResult Hit(Target,Target->GetCapsuleComponent(),Target->GetActorLocation(),FVector::UpVector);
			const int32 Before=Ninja->Defeats;
			if (Order==0) { Target->TryDefeat(ENinjaDefeatMethod::Stomp,Ninja); Projectile->Impact(Hit); }
			else { Projectile->Impact(Hit); Target->TryDefeat(ENinjaDefeatMethod::Stomp,Ninja); }
			if (Ninja->Defeats!=Before+1) { Finish(TEXT("Same-frame defeat race counted twice")); return; }
		}
		Pass(TEXT("Same-frame stomp and projectile dispatch count once in both orders"));
	}
}
void ANinjaCombatCheck::Finish(const FString& Error)
{
	Key(EKeys::F,false); Key(EKeys::LeftMouseButton,false);
	Key(EKeys::W,false); Key(EKeys::LeftShift,false);
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed"));
	Report->SetStringField(TEXT("error"),Error); Report->SetNumberField(TEXT("fixed_simulation_fps"),FPS);
	Report->SetBoolField(TEXT("city_mode"),bCityMode); Report->SetNumberField(TEXT("city_enemy_ground_error_cm"),CityGroundError);
	TArray<TSharedPtr<FJsonValue>> Values; for (const auto& Result:Results) Values.Add(MakeShared<FJsonValueString>(Result));
	Report->SetArrayField(TEXT("passed_cases"),Values);
	FString Path; FParse::Value(FCommandLine::Get(),TEXT("CombatReport="),Path);
	FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FFileHelper::SaveStringToFile(Json,*Path);
	FString Screenshot; if (FParse::Value(FCommandLine::Get(),TEXT("CombatScreenshot="),Screenshot))
	{
		auto* PC=UGameplayStatics::GetPlayerController(this,0); PC->SetViewTarget(Ninja); PC->SetControlRotation(FRotator(-10,0,0));
		if (!bCityMode) Place(FVector(0,0,92));
		FScreenshotRequest::RequestScreenshot(Screenshot,false,false);
	}
	UE_LOG(LogTemp,Display,TEXT("COMBAT_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error);
	bFinished=true; FinishedAt=FPlatformTime::Seconds();
}

void ANinjaCombatCheck::TickCity()
{
	const double Now=GetWorld()->GetTimeSeconds(),Elapsed=Now-PhaseTime;
	auto* PC=UGameplayStatics::GetPlayerController(this,0);
	if (Phase==1 || Phase==2) AimAt(Enemy->GetActorLocation());
	if (Phase==0 && Elapsed>1)
	{
		for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It) if (It->GetLoadProgress()<99) return;
		if (Enemy->GetCharacterMovement()->IsFalling()) return;
		FHitResult Ground; FCollisionQueryParams Params(SCENE_QUERY_STAT(CityEnemyFeet),true,Ninja); Params.AddIgnoredActor(Enemy);
		const FVector Point=Enemy->GetActorLocation();
		if (!GetWorld()->LineTraceSingleByChannel(Ground,Point+FVector(0,0,1000),Point-FVector(0,0,1000),ECC_Visibility,Params)) { Finish(TEXT("City enemy ground missing")); return; }
		CityGroundError=FMath::Abs(Point.Z-Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Ground.ImpactPoint.Z);
		if (CityGroundError>10) { Finish(TEXT("City enemy is not standing on the street")); return; }
		const FVector Direction=(Point-Ninja->GetActorLocation()).GetSafeNormal2D();
		Place(Point-Direction*500+FVector(0,0,10));
		PC->SetControlRotation((Point-Ninja->GetActorLocation()).Rotation());
		Phase=1; PhaseTime=Now;
	}
	else if (Phase==1 && Elapsed>.5)
	{
		Phase=2; PhaseTime=Now;
	}
	else if (Phase==2 && Elapsed>.5)
	{
		FString Screenshot;
		if (FParse::Value(FCommandLine::Get(),TEXT("CombatScreenshot="),Screenshot)) FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Screenshot)/FPaths::GetBaseFilename(Screenshot)+TEXT("_appearance.png"),false,false);
		Phase=3; PhaseTime=Now;
	}
	else if (Phase==3 && Elapsed>.2) { Key(EKeys::F,true); Phase=4; PhaseTime=Now; }
	else if (Phase==4 && Elapsed>.05) { Key(EKeys::F,false); Phase=5; PhaseTime=Now; }
	else if (Phase==5)
	{
		if (Enemy->bDefeated && Ninja->ShurikenDefeats==1 && Ninja->Health==3)
		{
			Results.Add(TEXT("City red ninja stands on fixed road and is defeated by mapped shuriken input")); Finish(FString());
		}
		else if (Elapsed>3) Finish(TEXT("City shuriken did not defeat the red ninja"));
	}
}
