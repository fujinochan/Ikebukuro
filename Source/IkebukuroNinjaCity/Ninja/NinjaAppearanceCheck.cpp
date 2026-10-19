#include "NinjaAppearanceCheck.h"
#include "NinjaCharacter.h"
#include "RedNinja.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ANinjaAppearanceCheck::ANinjaAppearanceCheck()
{ PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork; }
void ANinjaAppearanceCheck::Capture(const TCHAR* View)
{
	FString Prefix;
	if (FParse::Value(FCommandLine::Get(),TEXT("AppearanceScreenshot="),Prefix)) FScreenshotRequest::RequestScreenshot(Prefix+TEXT("-")+View+TEXT(".png"),false,false);
}
void ANinjaAppearanceCheck::Finish(const FString& Error)
{
	auto Report=MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed")); Report->SetStringField(TEXT("error"),Error);
	Report->SetStringField(TEXT("scope"),TEXT("Visual capture of actual black/red costumes in front, rear, face closeup and jumping animation; separate gameplay checks cover combat."));
	TArray<TSharedPtr<FJsonValue>> Counts;
	for (ACharacter* C:{static_cast<ACharacter*>(Ninja.Get()),static_cast<ACharacter*>(Enemy.Get())}) if (C)
	{
		const auto Parts=C->GetComponentsByTag(UStaticMeshComponent::StaticClass(),TEXT("NinjaCostume"));
		if (Parts.IsEmpty()) { Report->SetStringField(TEXT("status"),TEXT("failed")); Report->SetStringField(TEXT("error"),TEXT("Costume components were not created")); }
		Counts.Add(MakeShared<FJsonValueNumber>(Parts.Num()));
		for (auto* P:Parts) if (CastChecked<UStaticMeshComponent>(P)->GetCollisionEnabled()!=ECollisionEnabled::NoCollision) Report->SetStringField(TEXT("status"),TEXT("failed"));
	}
	Report->SetArrayField(TEXT("costume_parts"),Counts);
	FString Path,Json; FParse::Value(FCommandLine::Get(),TEXT("AppearanceReport="),Path); FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FFileHelper::SaveStringToFile(Json,*Path);
	Phase=99; PhaseAt=FPlatformTime::Seconds();
}
void ANinjaAppearanceCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double Now=FPlatformTime::Seconds();
	if (!Started) Started=Now;
	if (Phase==99) { if (Now-PhaseAt>1) FPlatformMisc::RequestExit(false); return; }
	if (Now-Started>45) { Finish(TEXT("Costume preview timed out")); return; }
	if (Phase==0 && Now-Started>2)
	{
		Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
		if (!Ninja) return;
		for (TActorIterator<ARedNinja> It(GetWorld());It;++It)
		{
			It->SetActorTickEnabled(false); It->GetCharacterMovement()->StopMovementImmediately();
			if (!Enemy) Enemy=*It; else It->SetActorHiddenInGame(true);
		}
		if (!Enemy) { Finish(TEXT("No red ninja in preview map")); return; }
		Ninja->SetActorLocation(FVector(0,-110,100)); Enemy->SetActorLocation(FVector(0,110,100));
		Ninja->SetActorRotation(FRotator::ZeroRotator); Enemy->SetActorRotation(FRotator::ZeroRotator);
		auto* PC=UGameplayStatics::GetPlayerController(this,0); PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
		if (PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
		Camera=GetWorld()->SpawnActor<ACameraActor>(FVector(550,-260,230),FRotator::ZeroRotator);
		Camera->SetActorRotation((FVector(0,0,100)-Camera->GetActorLocation()).Rotation()); Camera->GetCameraComponent()->SetFieldOfView(48);
		PC->SetViewTarget(Camera); Phase=1; PhaseAt=Now;
	}
	else if (Phase==1 && Now-PhaseAt>2) { Capture(TEXT("front")); Phase=2; PhaseAt=Now; }
	else if (Phase==2 && Now-PhaseAt>1)
	{
		Camera->SetActorLocation(FVector(-550,-260,230)); Camera->SetActorRotation((FVector(0,0,100)-Camera->GetActorLocation()).Rotation()); Phase=3; PhaseAt=Now;
	}
	else if (Phase==3 && Now-PhaseAt>1) { Capture(TEXT("rear")); Phase=4; PhaseAt=Now; }
	else if (Phase==4 && Now-PhaseAt>1)
	{
		const FVector Head=Ninja->GetMesh()->GetSocketLocation(TEXT("head"))+FVector(0,0,9);
		Camera->SetActorLocation(Head+FVector(145,-25,8)); Camera->SetActorRotation((Head-Camera->GetActorLocation()).Rotation()); Phase=5; PhaseAt=Now;
	}
	else if (Phase==5 && Now-PhaseAt>1) { Capture(TEXT("face")); Phase=6; PhaseAt=Now; }
	else if (Phase==6 && Now-PhaseAt>1)
	{
		Camera->SetActorLocation(FVector(680,-320,390)); Camera->SetActorRotation((FVector(0,0,220)-Camera->GetActorLocation()).Rotation());
		Ninja->LaunchCharacter(FVector(0,0,600),true,true); Enemy->LaunchCharacter(FVector(0,0,600),true,true); Phase=7; PhaseAt=Now;
	}
	else if (Phase==7 && Now-PhaseAt>.32) { Capture(TEXT("jump")); Phase=8; PhaseAt=Now; }
	else if (Phase==8 && Now-PhaseAt>1) Finish(FString());
}
