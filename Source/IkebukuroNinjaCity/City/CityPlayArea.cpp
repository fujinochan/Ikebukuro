#include "CityPlayArea.h"
#include "LocalCityTileset.h"
#include "Ninja/NinjaCharacter.h"
#include "Ninja/NinjaMatchDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ACityPlayArea::ACityPlayArea()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void ACityPlayArea::BeginPlay()
{
	Super::BeginPlay();
	bCheck = FParse::Param(FCommandLine::Get(), TEXT("CityAreaCheck"));
	if (bCheck) { FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1.0/60); }
	RealStart = FPlatformTime::Seconds();
}

void ACityPlayArea::PlaceAt(FVector Surface, bool bRecovery)
{
	if (!Ninja) return;
	if (bRecovery)
	{
		Ninja->RecoverToSurface(Surface,Ninja->GetActorLocation().Z < -1500);
		++RecoveryCount; Ninja->JumpFeedback=TEXT("Returned to the safe street"); return;
	}
	Ninja->SetActorLocation(Surface + FVector(0,0,Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+20));
	Ninja->GetCharacterMovement()->StopMovementImmediately();
	Ninja->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
}

void ACityPlayArea::Key(const FKey& Input, bool bPressed)
{
	if (auto* PC = UGameplayStatics::GetPlayerController(this,0))
		PC->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, Input, bPressed?IE_Pressed:IE_Released,
			bPressed?1.f:0.f, false, FPlatformTime::Cycles64()));
}

TArray<double> ACityPlayArea::ProbeGround() const
{
	TArray<double> Heights;
	for (int32 X=0; X<=10; ++X) for (int32 Y=0; Y<=10; ++Y)
	{
		const FVector Point(FMath::Lerp(PlayMinimum.X+100,PlayMaximum.X-100,X/10.),
			FMath::Lerp(PlayMinimum.Y+100,PlayMaximum.Y-100,Y/10.),50000);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Point, FVector(Point.X,Point.Y,-10000), ECC_Visibility,
			FCollisionQueryParams(SCENE_QUERY_STAT(CityAreaGround), true))) Heights.Add(Hit.ImpactPoint.Z);
		else Heights.Add(-1000000);
	}
	return Heights;
}

void ACityPlayArea::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Ninja = Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
	if (!Ninja) return;
	Ninja->SetSafeSurface(SafeStreet);
	if (!ANinjaMatchDirector::AllowsGameplay(this)) return;
	auto* PC = Cast<APlayerController>(Ninja->GetController());
	if (!PC) return;
	const FVector Position = Ninja->GetActorLocation();
	const bool bR = PC->IsInputKeyDown(EKeys::R);
	if (Position.X<PlayMinimum.X || Position.X>PlayMaximum.X || Position.Y<PlayMinimum.Y ||
		Position.Y>PlayMaximum.Y || Position.Z < -1500 || (bR && !bPreviousR)) PlaceAt(SafeStreet,true);
	bPreviousR = bR;
	if (!bCheck) return;
	if (bFinished)
	{
		if (FPlatformTime::Seconds()-FinishedAt>3) FPlatformMisc::RequestExit(false);
		return;
	}
	if (FPlatformTime::Seconds()-RealStart>120) { Finish(TEXT("City verification timed out")); return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const double Elapsed = Now-PhaseTime;
	if (Phase==0 && Now>2 && !Ninja->GetCharacterMovement()->IsFalling())
	{
		GroundHeights = ProbeGround();
		for (double H:GroundHeights) if (H < -10000) { Finish(TEXT("Ground hole in play area")); return; }
		TArray<ALocalCityTileset*> Tiles;
		TArray<FString> Urls;
		for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It)
		{
			Tiles.Add(*It); Urls.Add(It->GetUrl()); It->SetUrl(TEXT(""));
		}
		const auto After = ProbeGround();
		bStableCollision = After.Num()==GroundHeights.Num();
		for (int32 I=0; I<After.Num(); ++I) bStableCollision &= FMath::Abs(After[I]-GroundHeights[I]) < 0.01;
		for (int32 I=0;I<Tiles.Num();++I) Tiles[I]->SetUrl(Urls[I]);
		if (!bStableCollision) { Finish(TEXT("Collision changed when visual tiles were removed")); return; }
		PlaceAt(SafeStreet,false);
		FRotator Rotation = (StreetRunEnd-SafeStreet).Rotation(); Rotation.Pitch = -15;
		PC->SetControlRotation(Rotation);
		Phase=1; PhaseTime=Now;
	}
	else if (Phase==1 && Elapsed>0.5)
	{
		Key(EKeys::LeftShift,true); Key(EKeys::W,true);
		Phase=2; PhaseTime=Now;
	}
	else if (Phase==2 && Elapsed>1.5)
	{
		RunDistance = FVector::Dist2D(Ninja->GetActorLocation(), SafeStreet);
		Key(EKeys::W,false); Key(EKeys::LeftShift,false);
		if (RunDistance<1200 || Ninja->GetActorLocation().Z < SafeStreet.Z-200) { Finish(TEXT("Street sprint failed")); return; }
		PlaceAt(SafeStreet,false);
		Phase=3; PhaseTime=Now;
	}
	else if (Phase==3 && Elapsed>0.5 && !Ninja->GetCharacterMovement()->IsFalling())
	{
		UE_LOG(LogTemp,Display,TEXT("CITY_STREET actor=%s camera=%s"),*Ninja->GetActorLocation().ToString(),*Ninja->GetFollowCamera()->GetComponentLocation().ToString());
		for (TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
			if (It->GetStaticMeshComponent()->GetStaticMesh() && It->GetStaticMeshComponent()->GetStaticMesh()->GetName()==TEXT("SM_FillGround"))
				UE_LOG(LogTemp,Display,TEXT("CITY_GROUND hidden=%d visible=%d bounds=%s"),It->IsHidden(),It->GetStaticMeshComponent()->IsVisible(),*It->GetComponentsBoundingBox().ToString());
		FString Screenshot;
		if (FParse::Value(FCommandLine::Get(),TEXT("CityAreaScreenshot="),Screenshot))
			FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Screenshot)/FPaths::GetBaseFilename(Screenshot)+TEXT("_street.png"),false,false);
		Key(EKeys::SpaceBar,true); bSpaceDown=true; PressTime=Now;
		Phase=4; PhaseTime=Now;
	}
	else if (Phase==4)
	{
		MaxJumpStage = FMath::Max(MaxJumpStage,Ninja->JumpStage);
		if (bSpaceDown && Now-PressTime>0.08) { Key(EKeys::SpaceBar,false); bSpaceDown=false; }
		if (!bSpaceDown && Ninja->JumpStage>0 && Ninja->JumpStage<3 && Ninja->GetSecondsToJumpApex()<=0)
		{
			Key(EKeys::SpaceBar,true); bSpaceDown=true; PressTime=Now;
		}
		if (Elapsed>1 && !Ninja->GetCharacterMovement()->IsFalling() && Ninja->JumpStage==0)
		{
			if (MaxJumpStage!=3 || RoofTargets.Num()<3) { Finish(TEXT("City triple jump or roof targets failed")); return; }
			PlaceAt(RoofTargets[0]+FVector(0,0,300),false);
			Phase=5; PhaseTime=Now;
		}
	}
	else if (Phase==5 && Elapsed>1.5)
	{
		const double FeetZ = Ninja->GetActorLocation().Z-Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (Ninja->GetCharacterMovement()->IsFalling() || FMath::Abs(FeetZ-RoofTargets[RoofIndex].Z)>10)
		{
			FHitResult FloorHit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(RoofDiagnostic), true, Ninja);
			GetWorld()->SweepSingleByChannel(FloorHit, Ninja->GetActorLocation()+FVector(0,0,100), Ninja->GetActorLocation()-FVector(0,0,200), FQuat::Identity, ECC_Visibility,
				FCollisionShape::MakeCapsule(Ninja->GetCapsuleComponent()->GetScaledCapsuleRadius(),Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Params);
			Finish(FString::Printf(TEXT("Roof landing: feet=%.2f target=%.2f falling=%d normal=%s floorNormal=%s"), FeetZ,RoofTargets[RoofIndex].Z,Ninja->GetCharacterMovement()->IsFalling(),*FloorHit.ImpactNormal.ToString(),*Ninja->GetCharacterMovement()->CurrentFloor.HitResult.ImpactNormal.ToString())); return;
		}
		++RoofIndex;
		if (RoofIndex<RoofTargets.Num()) PlaceAt(RoofTargets[RoofIndex]+FVector(0,0,300),false);
		else
		{
			ExpectedRecovery=RecoveryCount+1;
			Ninja->SetActorLocation(FVector(PlayMinimum.X-300, SafeStreet.Y, SafeStreet.Z+100));
			Phase=6;
		}
		PhaseTime=Now;
	}
	else if (Phase==6 && Elapsed>0.6)
	{
		if (RecoveryCount!=ExpectedRecovery || FVector::Dist2D(Ninja->GetActorLocation(),SafeStreet)>100)
		{ Finish(TEXT("Boundary recovery failed")); return; }
		ExpectedRecovery=RecoveryCount+1;
		Ninja->SetActorLocation(FVector(SafeStreet.X, SafeStreet.Y,-5000));
		Phase=7; PhaseTime=Now;
	}
	else if (Phase==7 && Elapsed>0.6)
	{
		if (RecoveryCount!=ExpectedRecovery) { Finish(TEXT("Fall recovery failed")); return; }
		ExpectedRecovery=RecoveryCount+1;
		Key(EKeys::R,true);
		Phase=70; PhaseTime=Now;
	}
	else if (Phase==70 && Elapsed>0.4)
	{
		Key(EKeys::R,false);
		if (RecoveryCount!=ExpectedRecovery) { Finish(TEXT("R key recovery failed")); return; }
		PC->SetControlRotation(FRotator(-15,0,0));
		Phase=8; PhaseTime=Now;
	}
	else if (Phase==8 && Elapsed>0.3)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CityCameraCheck), true, Ninja);
		const FVector Start = Ninja->GetActorLocation();
		const FVector Camera = Ninja->GetFollowCamera()->GetComponentLocation();
		if (GetWorld()->LineTraceSingleByChannel(Hit,Start,Camera,ECC_Visibility,Params) && Hit.Distance < FVector::Distance(Start,Camera)-5)
			++CameraObstructions;
		++CameraIndex;
		if (CameraIndex<4) PC->SetControlRotation(FRotator(-15,CameraIndex*90,0));
		else
		{
			const FVector Center((PlayMinimum.X+PlayMaximum.X)/2,(PlayMinimum.Y+PlayMaximum.Y)/2,1000);
			const FVector Location = Center+FVector(23000,28000,35000);
			auto* CameraActor = GetWorld()->SpawnActor<ACameraActor>(Location,(Center-Location).Rotation());
			PC->SetViewTarget(CameraActor);
			Phase=9;
		}
		PhaseTime=Now;
	}
	else if (Phase==9 && Elapsed>5)
	{
		bool Ready=true;
		for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It) Ready &= It->GetLoadProgress()>=99;
		if (Ready)
		{
			for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It) It->SetCreatePhysicsMeshes(true);
			Phase=10; PhaseTime=Now;
		}
	}
	else if (Phase==10 && Elapsed>4)
	{
		for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It) if (It->GetLoadProgress()<99) return;
		TArray<UStaticMeshComponent*> Proxies;
		for (TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
		{
			auto* Component=It->GetStaticMeshComponent();
			if (Component->GetStaticMesh() && Component->GetStaticMesh()->GetName().EndsWith(TEXT("Collision")))
			{ Component->SetCollisionEnabled(ECollisionEnabled::NoCollision); Proxies.Add(Component); }
		}
		const auto VisualHeights=ProbeGround();
		double Error=0;
		for (int32 I=0; I<VisualHeights.Num(); ++I) Error=FMath::Max(Error,FMath::Abs(VisualHeights[I]-GroundHeights[I]));
		VisualCollisionErrorCm=Error;
		for (auto* Component:Proxies) Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It) It->SetCreatePhysicsMeshes(false);
		UE_LOG(LogTemp,Display,TEXT("CITY_VISUAL_COLLISION max error cm=%.4f"),Error);
		if (Error>3) { Finish(FString::Printf(TEXT("Visual and fixed collision mismatch: %.2fcm"),Error)); return; }
		Phase=11; PhaseTime=Now;
	}
	else if (Phase==11 && Elapsed>4)
	{
		bool Ready=true;
		for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It) Ready &= It->GetLoadProgress()>=99;
		if (Ready) Finish(CameraObstructions==0 ? FString() : TEXT("Camera obstruction detected"));
	}
}

void ACityPlayArea::Finish(const FString& Error)
{
	Key(EKeys::W,false); Key(EKeys::LeftShift,false); Key(EKeys::SpaceBar,false);
	auto Report=MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed"));
	Report->SetStringField(TEXT("error"),Error);
	Report->SetNumberField(TEXT("street_sprint_distance_cm"),RunDistance);
	Report->SetNumberField(TEXT("max_jump_stage"),MaxJumpStage);
	Report->SetNumberField(TEXT("successful_roof_landings"),RoofIndex);
	Report->SetNumberField(TEXT("ground_probe_count"),GroundHeights.Num());
	Report->SetBoolField(TEXT("collision_stable_after_visual_tile_removal"),bStableCollision);
	Report->SetNumberField(TEXT("visual_collision_max_height_error_cm"),VisualCollisionErrorCm);
	Report->SetNumberField(TEXT("recovery_count"),RecoveryCount);
	Report->SetNumberField(TEXT("camera_obstructions"),CameraObstructions);
	Report->SetNumberField(TEXT("phase"),Phase);
	Report->SetStringField(TEXT("character_position"),Ninja->GetActorLocation().ToString());
	FString Path;
	if (!FParse::Value(FCommandLine::Get(),TEXT("CityAreaReport="),Path)) Path=FPaths::ProjectSavedDir()/TEXT("Validation/step4-city.json");
	FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
	FFileHelper::SaveStringToFile(Json,*Path);
	FString Screenshot;
	if (FParse::Value(FCommandLine::Get(),TEXT("CityAreaScreenshot="),Screenshot)) FScreenshotRequest::RequestScreenshot(Screenshot,false,false);
	UE_LOG(LogTemp,Display,TEXT("CITY_AREA_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error);
	bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
