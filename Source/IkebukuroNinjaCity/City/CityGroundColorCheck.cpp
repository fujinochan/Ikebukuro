#include "CityGroundColorCheck.h"
#include "LocalCityTileset.h"
#include "Ninja/NinjaMatchDirector.h"
#include "Ninja/RedNinja.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProperties.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ACityGroundColorCheck::ACityGroundColorCheck()
{ PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork; }
void ACityGroundColorCheck::Capture(const TCHAR* Name)
{ FString Prefix; if (FParse::Value(FCommandLine::Get(),TEXT("GroundScreenshot="),Prefix)) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-")+Name+TEXT(".png"),false,false); }
void ACityGroundColorCheck::Finish(const FString& Error)
{
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed")); Report->SetStringField(TEXT("error"),Error);
	Report->SetBoolField(TEXT("baseline"),FParse::Param(FCommandLine::Get(),TEXT("GroundBaseline"))); Report->SetBoolField(TEXT("cooked_data"),FPlatformProperties::RequiresCookedData());
	Report->SetStringField(TEXT("map"),GetWorld()->GetMapName()); Report->SetStringField(TEXT("scope"),TEXT("Rendered local city, floor and road material assignments, overhead and street captures; AI held for visual inspection."));
	FString Path,Json; FParse::Value(FCommandLine::Get(),TEXT("GroundReport="),Path); FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FFileHelper::SaveStringToFile(Json,*Path);
	Phase=99; PhaseAt=FPlatformTime::Seconds();
}
void ACityGroundColorCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double Now=FPlatformTime::Seconds(); if (!Started) Started=Now;
	if (Phase==99) { if (Now-PhaseAt>1) FPlatformMisc::RequestExit(false); return; }
	if (Now-Started>90) { Finish(TEXT("Ground color check timed out before city became ready")); return; }
	if (Phase==0)
	{
		Match=ANinjaMatchDirector::Find(this); if (!Match || Match->State!=ENinjaMatchState::Ready) return;
		for (ARedNinja* Enemy:Match->Enemies) { Enemy->SetActorTickEnabled(false); Enemy->GetCharacterMovement()->StopMovementImmediately(); }
		const bool Baseline=FParse::Param(FCommandLine::Get(),TEXT("GroundBaseline"));
		bool Floor=false,Road=false;
		for (TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
		{
			auto* C=It->GetStaticMeshComponent();
			if (C->GetStaticMesh() && C->GetStaticMesh()->GetName()==TEXT("SM_FillGround"))
			{ Floor=true; if (!Baseline && (!C->GetMaterial(0) || C->GetMaterial(0)->GetName()!=TEXT("M_CityGroundPhotoColors"))) { Finish(TEXT("Fill ground material is not the photo color material")); return; } }
		}
		for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It) if (It->DatasetRelativePath.Contains(TEXT("/Roads/")))
		{ Road=true; if (!Baseline && (!It->GetMaterial() || It->GetMaterial()->GetName()!=TEXT("M_CityGroundPhotoColors"))) { Finish(TEXT("Road tiles do not have the photo color material")); return; } }
		if (!Floor || !Road) { Finish(TEXT("Ground or road actor missing")); return; }
		auto* PC=UGameplayStatics::GetPlayerController(this,0); PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); if (PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
		Camera=GetWorld()->SpawnActor<ACameraActor>(FVector(12000,-3000,100000),FRotator(-90,0,0)); Camera->GetCameraComponent()->SetFieldOfView(48); PC->SetViewTarget(Camera);
		Phase=1; PhaseAt=Now;
	}
	else if (Phase==1 && Now-PhaseAt>5) { Capture(TEXT("overhead")); Phase=2; PhaseAt=Now; }
	else if (Phase==2 && Now-PhaseAt>1) { UGameplayStatics::GetPlayerController(this,0)->SetViewTarget(UGameplayStatics::GetPlayerCharacter(this,0)); Phase=3; PhaseAt=Now; }
	else if (Phase==3 && Now-PhaseAt>4) { Capture(TEXT("street")); Phase=4; PhaseAt=Now; }
	else if (Phase==4 && Now-PhaseAt>1) Finish(FString());
}
