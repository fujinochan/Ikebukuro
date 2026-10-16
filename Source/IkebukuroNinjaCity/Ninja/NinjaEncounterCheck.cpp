#include "NinjaEncounterCheck.h"
#include "NinjaCharacter.h"
#include "RedNinja.h"
#include "NinjaPatrolArea.h"
#include "Camera/CameraActor.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ANinjaEncounterCheck::ANinjaEncounterCheck() { PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostPhysics; }
void ANinjaEncounterCheck::BeginPlay()
{
	Super::BeginPlay(); SetActorTickEnabled(FParse::Param(FCommandLine::Get(),TEXT("NinjaEncounterCheck")));
	if (!IsActorTickEnabled()) return;
	FParse::Value(FCommandLine::Get(),TEXT("EncounterFPS="),FPS); FPS=FMath::Clamp(FPS,30,120);
	FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1.0/FPS);
	bCity=GetWorld()->GetMapName().Contains(TEXT("L_Ikebukuro")); RealStart=FPlatformTime::Seconds();
}
void ANinjaEncounterCheck::PlacePlayer(FVector Center)
{
	Ninja->SetActorLocation(Center); Ninja->GetCharacterMovement()->StopMovementImmediately(); Ninja->GetCharacterMovement()->ClearAccumulatedForces();
	Ninja->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
}
void ANinjaEncounterCheck::Hold(ARedNinja* Enemy,bool Enabled)
{
	Enemy->bAIEnabled=Enabled; Enemy->SetActorTickEnabled(Enabled);
	Enemy->GetCharacterMovement()->StopMovementImmediately(); Enemy->GetCharacterMovement()->ClearAccumulatedForces();
}
void ANinjaEncounterCheck::Next(const FString& Result)
{
	Results.Add(Result); UE_LOG(LogTemp,Display,TEXT("ENCOUNTER_CASE_PASSED %s"),*Result);
	++Phase; PhaseTime=GetWorld()->GetTimeSeconds();
}
void ANinjaEncounterCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished) { if (FPlatformTime::Seconds()-FinishedAt>3) FPlatformMisc::RequestExit(false); return; }
	if (!Ninja)
	{
		Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)); if (!Ninja) return;
		for (TActorIterator<ARedNinja> It(GetWorld());It;++It)
		{
			if (It->ActorHasTag(TEXT("GroundA"))) GroundA=*It;
			if (It->ActorHasTag(TEXT("GroundB"))) GroundB=*It;
			if (It->ActorHasTag(TEXT("RoofA"))) Roof=*It;
		}
		if (!GroundA || !GroundB || !Roof) { Finish(TEXT("Three tagged enemies missing")); return; }
		GroundDomain=GroundA->PatrolArea; RoofDomain=Roof->PatrolArea; EnemyClass=Roof->GetClass();
		FarPlayer=bCity ? GroundA->PatrolArea->GetActorLocation()+FVector(0,7000,5000) : FVector(6500,6500,92);
		PlacePlayer(FarPlayer); PhaseTime=GetWorld()->GetTimeSeconds();
	}
	const double Now=GetWorld()->GetTimeSeconds(),Elapsed=Now-PhaseTime;
	if (FPlatformTime::Seconds()-RealStart>130 || Elapsed>25)
	{ Finish(FString::Printf(TEXT("Phase %d timeout; nodes=%d/%d"),Phase,GroundDomain->ValidNodes,RoofDomain->ValidNodes)); return; }
	if (Phase<6)
	{
		OrdinaryRecoveries=GroundA->SafetyRecoveries+GroundB->SafetyRecoveries+Roof->SafetyRecoveries;
		if (OrdinaryRecoveries) { Finish(TEXT("Ordinary navigation required safety teleport")); return; }
	}
	if (!bCity && Phase<6 && FMath::Abs(GroundA->GetActorLocation().X-1800)<15 && GroundA->GetActorLocation().Y>250 && GroundA->GetActorLocation().Y<1250) { Finish(TEXT("Enemy penetrated wall")); return; }
	if (Phase==0)
	{
		if (Elapsed>1 && GroundA->BrainState==ENinjaBrainState::Patrol && Roof->BrainState==ENinjaBrainState::Patrol && GroundB->BrainState==ENinjaBrainState::Patrol)
		{
			StartGround=PreviousGround=GroundA->GetActorLocation(); PreviousB=GroundB->GetActorLocation(); StartRoof=PreviousRoof=Roof->GetActorLocation();
			GroundHome=GroundA->HomeFoot; RoofHome=Roof->HomeFoot;
			Next(TEXT("Three enemies initialized with fixed-collision walking graphs"));
		}
	}
	else if (Phase==1)
	{
		GroundDistance+=FVector::Dist2D(PreviousGround,GroundA->GetActorLocation()); PreviousGround=GroundA->GetActorLocation();
		GroundBDistance+=FVector::Dist2D(PreviousB,GroundB->GetActorLocation()); PreviousB=GroundB->GetActorLocation();
		RoofDistance+=FVector::Dist2D(PreviousRoof,Roof->GetActorLocation()); PreviousRoof=Roof->GetActorLocation();
		if (!bOverview && Elapsed>2)
		{
			bOverview=true;
			const FVector Center=(GroundA->HomeFoot+Roof->HomeFoot)*.5;
			const FVector View=Center+FVector(-1800,-4500,3000);
			auto* Camera=GetWorld()->SpawnActor<ACameraActor>(View,(Center-View).Rotation());
			UGameplayStatics::GetPlayerController(this,0)->SetViewTarget(Camera);
		}
		if (Elapsed>3 && !bOverviewSaved)
		{
			bOverviewSaved=true;
			FString Screenshot; if (FParse::Value(FCommandLine::Get(),TEXT("EncounterScreenshot="),Screenshot)) FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Screenshot)/FPaths::GetBaseFilename(Screenshot)+TEXT("_overview.png"),false,false);
		}
		if (Elapsed<12) return;
		if (GroundDistance<500 || GroundBDistance<500 || RoofDistance<500) { Finish(TEXT("A patrol did not keep moving for 12 seconds")); return; }
		UGameplayStatics::GetPlayerController(this,0)->SetViewTarget(Ninja);
		Hold(GroundB,false); Hold(Roof,false);
		GroundA->DetectionRadius=3000; GroundA->LeashRadius=4000;
		const FVector Target=bCity ? GroundA->HomeFoot-FVector(600,0,0) : FVector(2600,800,0);
		TArray<FVector> Path;
		if (!GroundA->PatrolArea->FindRoute(GroundA->HomeFoot,Target,Path) || (!bCity && GroundA->PatrolArea->ClearSegment(GroundA->HomeFoot,Target))) { Finish(TEXT("Wall detour route invalid")); return; }
		Ninja->Health=3; PlacePlayer(Target+FVector(0,0,92)); Next(TEXT("Patrol moved on street and roof; walk route detours around wall"));
	}
	else if (Phase==2 && GroundA->BrainState==ENinjaBrainState::Attack && Ninja->Health<3)
	{
		if (!(GroundA->VisitedStates&(1<<int32(ENinjaBrainState::Chase)))) { Finish(TEXT("Enemy did not chase")); return; }
		PlacePlayer(FarPlayer); Next(TEXT("Chase reaches player and melee contact causes damage"));
	}
	else if (Phase==3 && FVector::Dist2D(GroundA->HomeFoot,GroundA->GetActorLocation())<110)
	{
		if (!(GroundA->VisitedStates&(1<<int32(ENinjaBrainState::Return)))) return;
		Hold(GroundA,false); Hold(Roof,true);
		Roof->DetectionRadius=1500;
		RoofGoal=Roof->PatrolArea->PatrolGoal(Roof->HomeFoot,1);
		if (FVector::Dist2D(RoofGoal,Roof->HomeFoot)<200) { Finish(TEXT("Roof has no safe encounter route")); return; }
		Ninja->Health=3; PlacePlayer(RoofGoal+FVector(0,0,92)); Next(TEXT("Losing player returns ground enemy to its home"));
	}
	else if (Phase==4 && Roof->BrainState==ENinjaBrainState::Attack)
	{
		PlacePlayer(Roof->PatrolArea->GetActorLocation()+FVector(Roof->PatrolArea->HalfExtent.X+500,0,92));
		Next(TEXT("Roof enemy chases on the same roof"));
	}
	else if (Phase==5 && FVector::Dist2D(Roof->HomeFoot,Roof->GetActorLocation())<110)
	{
		if (!(Roof->VisitedStates&(1<<int32(ENinjaBrainState::Return)))) return;
		TArray<FVector> Path;
		if (Roof->PatrolArea->FindRoute(Roof->HomeFoot,Roof->HomeFoot+FVector(3000,0,-800),Path)) { Finish(TEXT("Roof route escaped to a different surface")); return; }
		Roof->SetActorLocation(Roof->PatrolArea->GetActorLocation()+FVector(Roof->PatrolArea->HalfExtent.X+300,0,-300));
		Next(TEXT("Roof edge / unreachable target triggers return, not a jump or fall"));
	}
	else if (Phase==6 && Elapsed>.5)
	{
		if (Roof->SafetyRecoveries!=1 || FMath::Abs(Roof->GetActorLocation().Z-Roof->HomeFoot.Z-90)>30) { Finish(TEXT("Forced roof displacement recovery failed")); return; }
		ForcedRoofRecoveries=Roof->SafetyRecoveries;
		Hold(Roof,false); PlacePlayer(Roof->GetActorLocation()+FVector(0,0,450)); Ninja->JumpStage=2;
		Ninja->GetCharacterMovement()->Velocity=FVector(0,0,-300); Next(TEXT("External displacement recovers to original roof"));
	}
	else if (Phase==7)
	{
		if (Roof->bDefeated)
		{
			if (Ninja->JumpStage!=2 || Ninja->StompDefeats<1) { Finish(TEXT("Roof stomp failed or recharged jump")); return; }
			Hold(GroundB,false); PlacePlayer(GroundB->GetActorLocation()-FVector(500,0,-10));
			Ninja->GetCharacterMovement()->ClearAccumulatedForces();
			Next(TEXT("Stomp defeats roof enemy while preserving jump stage"));
		}
	}
	else if (Phase==8)
	{
		auto* PC=UGameplayStatics::GetPlayerController(this,0); FVector Camera; FRotator Look; PC->GetPlayerViewPoint(Camera,Look);
		PC->SetControlRotation((GroundB->GetActorLocation()-Camera).Rotation());
		if (Elapsed>.6) { Ninja->TryThrowShuriken(); Next(TEXT("Aimed shuriken launched in the encounter")); }
	}
	else if (Phase==9 && GroundB->bDefeated)
	{
		if (Ninja->ShurikenDefeats<1 || Ninja->Defeats!=2) { Finish(TEXT("Encounter projectile scoring failed")); return; }
		PlacePlayer(GroundA->GetActorLocation()+FVector(0,0,450)); Ninja->JumpStage=2; Ninja->GetCharacterMovement()->Velocity=FVector(0,0,-300);
		Next(TEXT("Ground enemy defeated by shuriken"));
	}
	else if (Phase==10 && GroundA->bDefeated)
	{
		if (Ninja->StompDefeats!=2 || Ninja->Defeats!=3) { Finish(TEXT("Ground stomp did not defeat the patrol enemy")); return; }
		FTransform Transform(RoofHome+FVector(0,0,110));
		auto* Replacement=GetWorld()->SpawnActorDeferred<ARedNinja>(EnemyClass,Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		// Navigation and melee were checked with the original roof enemy above.
		// Isolate projectile collision from a melee knockback during camera settling.
		Replacement->PatrolArea=RoofDomain; Replacement->bAIEnabled=false; Replacement->Tags.Add(TEXT("RoofA")); Replacement->FinishSpawning(Transform); Roof=Replacement;
		// The real roof is irregular: use the already validated walking goal,
		// rather than assuming a fixed offset from its center is supported.
		PlacePlayer(RoofGoal+FVector(0,0,92));
		Next(TEXT("Ground enemy defeated by stomp; spawn one verification-only roof replacement"));
	}
	else if (Phase==11)
	{
		auto* PC=UGameplayStatics::GetPlayerController(this,0); FVector Camera; FRotator Look; PC->GetPlayerViewPoint(Camera,Look);
		PC->SetControlRotation((Roof->GetActorLocation()-Camera).Rotation());
		if (Elapsed>.65) { Ninja->TryThrowShuriken(); Next(TEXT("Aimed shuriken launched at roof enemy")); }
	}
	else if (Phase==12 && Roof->bDefeated)
	{
		if (Ninja->ShurikenDefeats!=2 || Ninja->Defeats!=4) { Finish(TEXT("Roof projectile defeat failed")); return; }
		PlacePlayer(GroundHome+FVector(0,0,92)); Ninja->DoJumpEnd(); Ninja->JumpStage=0;
		Next(TEXT("Both defeat methods work on ground and roof"));
	}
	else if (Phase==13 && Elapsed>.4 && !Ninja->GetCharacterMovement()->IsFalling())
	{
		auto* PC=UGameplayStatics::GetPlayerController(this,0);
		PC->InputKey(FInputKeyEventArgs(nullptr,INPUTDEVICEID_NONE,EKeys::SpaceBar,IE_Pressed,1.f,false,FPlatformTime::Cycles64()));
		bSpaceHeld=true; PressTime=Now; ++Phase; PhaseTime=Now;
	}
	else if (Phase==14)
	{
		auto* PC=UGameplayStatics::GetPlayerController(this,0);
		MaxJumpStage=FMath::Max(MaxJumpStage,Ninja->JumpStage);
		if (bSpaceHeld && Now-PressTime>.06) { PC->InputKey(FInputKeyEventArgs(nullptr,INPUTDEVICEID_NONE,EKeys::SpaceBar,IE_Released,0.f,false,FPlatformTime::Cycles64())); bSpaceHeld=false; }
		if (!bSpaceHeld && Ninja->JumpStage>0 && Ninja->JumpStage<3 && Ninja->GetSecondsToJumpApex()<=0)
		{ PC->InputKey(FInputKeyEventArgs(nullptr,INPUTDEVICEID_NONE,EKeys::SpaceBar,IE_Pressed,1.f,false,FPlatformTime::Cycles64())); bSpaceHeld=true; PressTime=Now; }
		if (Elapsed>1 && !Ninja->GetCharacterMovement()->IsFalling() && Ninja->JumpStage==0)
		{ if (MaxJumpStage!=3) Finish(TEXT("Encounter triple jump failed")); else { Results.Add(TEXT("Mapped triple jump completes and recharges on real ground")); Finish(FString()); } }
	}
}
void ANinjaEncounterCheck::Finish(const FString& Error)
{
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed"));
	Report->SetStringField(TEXT("error"),Error); Report->SetBoolField(TEXT("city"),bCity); Report->SetNumberField(TEXT("fixed_fps"),FPS);
	TArray<TSharedPtr<FJsonValue>> Values; for (const FString& Result:Results) Values.Add(MakeShared<FJsonValueString>(Result)); Report->SetArrayField(TEXT("passed_cases"),Values);
	if (GroundDomain) Report->SetNumberField(TEXT("ground_nodes"),GroundDomain->ValidNodes);
	if (RoofDomain) Report->SetNumberField(TEXT("roof_nodes"),RoofDomain->ValidNodes);
	Report->SetNumberField(TEXT("ground_patrol_distance_cm"),GroundDistance); Report->SetNumberField(TEXT("second_ground_patrol_distance_cm"),GroundBDistance); Report->SetNumberField(TEXT("roof_patrol_distance_cm"),RoofDistance);
	Report->SetNumberField(TEXT("ordinary_safety_recoveries"),OrdinaryRecoveries); Report->SetNumberField(TEXT("roof_recoveries_after_forced_displacement"),ForcedRoofRecoveries); Report->SetNumberField(TEXT("max_jump_stage"),MaxJumpStage);
	FString Path,Json; FParse::Value(FCommandLine::Get(),TEXT("EncounterReport="),Path); FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FFileHelper::SaveStringToFile(Json,*Path);
	FString Screenshot; if (FParse::Value(FCommandLine::Get(),TEXT("EncounterScreenshot="),Screenshot)) FScreenshotRequest::RequestScreenshot(Screenshot,false,false);
	UE_LOG(LogTemp,Display,TEXT("ENCOUNTER_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error);
	bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
