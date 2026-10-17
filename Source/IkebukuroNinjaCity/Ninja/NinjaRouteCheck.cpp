#include "NinjaRouteCheck.h"
#include "NinjaCharacter.h"
#include "RedNinja.h"
#include "NinjaPatrolArea.h"
#include "NinjaMatchDirector.h"
#include "NinjaFeedback.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWave.h"
#include "UnrealClient.h"

ANinjaRouteCheck::ANinjaRouteCheck() { PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostPhysics; }
void ANinjaRouteCheck::BeginPlay()
{
	Super::BeginPlay(); SetActorTickEnabled(FParse::Param(FCommandLine::Get(),TEXT("NinjaRouteCheck")));
	if (!IsActorTickEnabled()) return;
	FParse::Value(FCommandLine::Get(),TEXT("RouteFPS="),FPS); FPS=FMath::Clamp(FPS,30,60);
	FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1.0/FPS); Started=FPlatformTime::Seconds();
}
void ANinjaRouteCheck::PlaceAtEntry()
{
	const FVector Entry=Roofs[RoofIndex]->AssistRoute[0];
	Ninja->SetActorLocation(Entry+FVector(0,0,110)); Ninja->GetCharacterMovement()->StopMovementImmediately();
	Ninja->GetCharacterMovement()->ClearAccumulatedForces(); Ninja->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Ninja->DoJumpEnd(); Ninja->JumpStage=0; StepIndex=1; bJumped=false; StepTime=GetWorld()->GetTimeSeconds();
	StepBudget=FMath::Max(8.,FVector::Dist2D(Entry,Roofs[RoofIndex]->AssistRoute[1])/200+5);
	UGameplayStatics::GetPlayerController(this,0)->SetControlRotation((Roofs[RoofIndex]->GetActorLocation()-Ninja->GetActorLocation()).Rotation());
}
void ANinjaRouteCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double Wall=FPlatformTime::Seconds();
	if (bFinished) { if (Wall-FinishedAt>3) FPlatformMisc::RequestExit(false); return; }
	if (!Match)
	{
		Match=ANinjaMatchDirector::Find(this); Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
		if (!Match || !Ninja) return; PhaseTime=GetWorld()->GetTimeSeconds();
	}
	if (Wall-Started>160) { Finish(FString::Printf(TEXT("Phase %d timeout; roof %d step %d; match %d"),Phase,RoofIndex,StepIndex,int32(Match->State))); return; }
	const double Now=GetWorld()->GetTimeSeconds();
	auto* Move=Ninja->GetCharacterMovement();
	if (Phase==0 && Match->State==ENinjaMatchState::Ready)
	{
		TSet<int32> Orders;
		for (ARedNinja* Enemy:Match->Enemies)
		{
			Orders.Add(Enemy->RouteOrder); InitialPositions.Add(Enemy->GetActorLocation()); PatrolDistances.Add(0);
			Enemy->bRangedAttackEnabled=false;
			if (!Enemy->IsReadyForMatch() || !Enemy->PatrolArea || Enemy->PatrolArea->ValidNodes<3) { Finish(TEXT("Enemy graph unavailable")); return; }
			if (!Enemy->AssistRoute.IsEmpty()) Roofs.Add(Enemy);
		}
		if (Match->Enemies.Num()!=8 || Orders.Num()!=8 || Roofs.Num()!=4) { Finish(TEXT("Expected eight unique targets and four roof approaches")); return; }
		for (int32 I=1;I<=8;++I) if (!Orders.Contains(I)) { Finish(TEXT("Route order missing")); return; }
		for (const TCHAR* Cue:{TEXT("Jump"),TEXT("Triple"),TEXT("Throw"),TEXT("Hit"),TEXT("Stomp"),TEXT("Damage"),TEXT("Impact")})
			if (!LoadObject<USoundWave>(nullptr,*(FString(TEXT("/Game/Ninja/Audio/S_"))+Cue+TEXT(".S_")+Cue))) { Finish(TEXT("Sound asset missing")); return; }
		Roofs.Sort([](const ARedNinja& A,const ARedNinja& B){return A.RouteOrder<B.RouteOrder;});
		Cases.Add(TEXT("Eight unique enemies, four ready roof approaches and seven sound assets loaded"));
		Ninja->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
		Match->Confirm(); Phase=1;
	}
	else if (Phase==1 && Match->CanPlay()) { Phase=2; PhaseTime=Now; }
	else if (Phase==2)
	{
		for (int32 I=0;I<Match->Enemies.Num();++I) PatrolDistances[I]=FMath::Max(PatrolDistances[I],FVector::Dist2D(Match->Enemies[I]->GetActorLocation(),InitialPositions[I]));
		if (Now-PhaseTime<=10) return;
		for (int32 I=0;I<Match->Enemies.Num();++I)
		{
			ARedNinja* E=Match->Enemies[I];
			if (E->SafetyRecoveries || E->bDefeated || PatrolDistances[I]<80) { Finish(FString::Printf(TEXT("Enemy %d did not patrol safely; recovery=%d movement=%.0f"),E->RouteOrder,E->SafetyRecoveries,PatrolDistances[I])); return; }
			E->SetActorTickEnabled(false); E->GetCharacterMovement()->StopMovementImmediately(); E->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Cases.Add(TEXT("All eight enemies physically patrol for ten seconds with zero safety recovery"));
		Move->MaxWalkSpeed=500; PlaceAtEntry(); Phase=3;
	}
	else if (Phase==3)
	{
		const auto& Route=Roofs[RoofIndex]->AssistRoute; const FVector Target=Route[StepIndex];
		const FVector Foot=Ninja->GetActorLocation()-FVector(0,0,Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector Delta=Target-Foot; const bool Grounded=Move->IsMovingOnGround();
		if (Now-StepTime>StepBudget) { Finish(FString::Printf(TEXT("Roof %d step %d unreachable: foot=%s target=%s"),Roofs[RoofIndex]->RouteOrder,StepIndex,*Foot.ToString(),*Target.ToString())); return; }
		if (Grounded && FMath::Abs(Delta.Z)<90 && Delta.Size2D()<65)
		{
			++Landings; ++StepIndex; bJumped=false; StepTime=Now; Move->StopMovementImmediately();
			if (Route.IsValidIndex(StepIndex)) StepBudget=FMath::Max(8.,FVector::Dist2D(Foot,Route[StepIndex])/200+5);
			if (StepIndex>=Route.Num())
			{
				Cases.Add(FString::Printf(TEXT("Roof %d: all %d auxiliary landings reached with single jumps and physical walking, no intermediate teleport"),Roofs[RoofIndex]->RouteOrder,Route.Num()-1));
				FString Prefix; if (FParse::Value(FCommandLine::Get(),TEXT("RouteScreenshot="),Prefix)) FScreenshotRequest::RequestScreenshot(Prefix+FString::Printf(TEXT("_roof%d.png"),Roofs[RoofIndex]->RouteOrder),false,false);
				++RoofIndex; if (RoofIndex>=Roofs.Num())
				{
					Cases.Add(TEXT("All four city roof approaches traversed"));
					ShortcutTarget=Roofs[2]->HomeFoot; bool Found=false;
					for (int32 Radius=700;Radius<=2500 && !Found;Radius+=300) for (int32 A=0;A<8 && !Found;++A)
					{
						const FVector XY=ShortcutTarget+FVector(FMath::Cos(A*PI/4)*Radius,FMath::Sin(A*PI/4)*Radius,0);
						FHitResult Floor,Ceiling; FCollisionQueryParams Query(SCENE_QUERY_STAT(ShortcutLaunch),true);
						if (!GetWorld()->LineTraceSingleByObjectType(Floor,FVector(XY.X,XY.Y,6000),FVector(XY.X,XY.Y,-1000),FCollisionObjectQueryParams(ECC_WorldStatic),Query) || Floor.ImpactPoint.Z>1200 || Floor.ImpactNormal.Z<.85) continue;
						const FVector Center=Floor.ImpactPoint+FVector(0,0,Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+4);
						if (GetWorld()->SweepSingleByObjectType(Ceiling,Center,Center+FVector(0,0,3500),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(43,90),Query)) continue;
						Ninja->SetActorLocation(Center); Move->StopMovementImmediately(); Move->SetMovementMode(MOVE_Falling); Found=true;
					}
					if (!Found) { Finish(TEXT("No clear street launch for triple shortcut")); return; }
					Move->MaxWalkSpeed=700; Phase=4; PhaseTime=Now; bJumped=false; return;
				}
				PlaceAtEntry();
			}
			return;
		}
		if (Grounded && !bJumped && Delta.Z>45) { Ninja->DoJumpStart(); Ninja->DoJumpEnd(); bJumped=true; }
		// Brake lateral velocity near the destination using normal opposing air input.
		const FVector Desired=Delta.GetSafeNormal2D()*FMath::Min(500.,Delta.Size2D()*4);
		const FVector Input=(Desired-FVector(Move->Velocity.X,Move->Velocity.Y,0))/350;
		if (Grounded) Ninja->AddMovementInput(Delta.GetSafeNormal2D(),FMath::Clamp(Delta.Size2D()/150,.2,1.));
		else Ninja->AddMovementInput(Input.GetClampedToMaxSize(1),1);
	}
	else if (Phase==4)
	{
		const FVector Foot=Ninja->GetActorLocation()-FVector(0,0,Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		const FVector Delta=ShortcutTarget-Foot;
		if (Now-PhaseTime>12) { Finish(TEXT("Triple shortcut failed to reach the mid roof")); return; }
		if (!bJumped && Move->IsMovingOnGround()) { Ninja->DoJumpStart(); Ninja->DoJumpEnd(); bJumped=true; }
		if (Ninja->CanChainJump() && FMath::Abs(Ninja->GetSecondsToJumpApex())<.04) { Ninja->DoJumpStart(); Ninja->DoJumpEnd(); }
		bTripleReached|=Ninja->JumpStage==3;
		if (bTripleReached && Foot.Z>ShortcutTarget.Z+130)
		{
			const FVector Desired=Delta.GetSafeNormal2D()*FMath::Min(700.,Delta.Size2D()*4);
			Ninja->AddMovementInput(((Desired-FVector(Move->Velocity.X,Move->Velocity.Y,0))/350).GetClampedToMaxSize(1),1);
		}
		if (bTripleReached && Move->IsMovingOnGround())
		{
			if (Delta.Size2D()>90 || FMath::Abs(Delta.Z)>90) { Finish(TEXT("Triple jump landed outside the mid-roof shortcut target")); return; }
			Cases.Add(TEXT("Street-to-mid-roof shortcut reached by three apex-timed jumps using normal movement, no in-flight teleport")); Finish(FString());
		}
	}
}
void ANinjaRouteCheck::Finish(const FString& Error)
{
	if (Error.IsEmpty() && !FParse::Param(FCommandLine::Get(),TEXT("NullRHI")))
	{
		FString Prefix; if (FParse::Value(FCommandLine::Get(),TEXT("RouteScreenshot="),Prefix)) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("_shortcut.png"),false,false);
	}
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed")); Report->SetStringField(TEXT("error"),Error);
	Report->SetNumberField(TEXT("fps"),FPS); Report->SetNumberField(TEXT("landings"),Landings); Report->SetNumberField(TEXT("feedback_events"),ANinjaFeedback::EmittedCount);
	Report->SetBoolField(TEXT("rendered"),!FParse::Param(FCommandLine::Get(),TEXT("NullRHI")));
	Report->SetStringField(TEXT("scope"),TEXT("Ranged fire and pawn contact disabled to isolate patrol and roof approaches; starts at each ground entrance; not an end-to-end human playtest"));
	TArray<TSharedPtr<FJsonValue>> Results; for (const auto& Case:Cases) Results.Add(MakeShared<FJsonValueString>(Case)); Report->SetArrayField(TEXT("passed_cases"),Results);
	TArray<TSharedPtr<FJsonValue>> Enemies; if (Match) for (ARedNinja* E:Match->Enemies)
	{
		auto Row=MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("order"),E->RouteOrder); Row->SetNumberField(TEXT("nodes"),E->PatrolArea?E->PatrolArea->ValidNodes:0);
		Row->SetNumberField(TEXT("recovery"),E->SafetyRecoveries); Row->SetBoolField(TEXT("ready"),E->IsReadyForMatch()); Row->SetStringField(TEXT("position"),E->GetActorLocation().ToString()); Enemies.Add(MakeShared<FJsonValueObject>(Row));
	}
	Report->SetArrayField(TEXT("enemies"),Enemies);
	FString Json,Path; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FParse::Value(FCommandLine::Get(),TEXT("RouteReport="),Path); FFileHelper::SaveStringToFile(Json,*Path);
	UE_LOG(LogTemp,Display,TEXT("ROUTE_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error); bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
