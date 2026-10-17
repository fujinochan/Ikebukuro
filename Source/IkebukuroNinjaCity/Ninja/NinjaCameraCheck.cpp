#include "NinjaCameraCheck.h"
#include "NinjaCharacter.h"
#include "NinjaMatchDirector.h"
#include "RedNinja.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ANinjaCameraCheck::ANinjaCameraCheck()
{ PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork; }
void ANinjaCameraCheck::BeginPlay()
{
	Super::BeginPlay(); Started=FPlatformTime::Seconds();
	FParse::Value(FCommandLine::Get(),TEXT("CameraFPS="),FPS); FPS=FMath::Clamp(FPS,30,120);
	FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./FPS);
}
void ANinjaCameraCheck::Key(FKey Input,bool bPressed)
{ PC->InputKey(FInputKeyEventArgs(nullptr,INPUTDEVICEID_NONE,Input,bPressed?IE_Pressed:IE_Released,bPressed?1.f:0.f,false,FPlatformTime::Cycles64())); }
void ANinjaCameraCheck::Stop()
{ Key(EKeys::D,false); Key(EKeys::S,false); Ninja->DoMove(0,0); Ninja->GetCharacterMovement()->StopMovementImmediately(); }
void ANinjaCameraCheck::Next(const FString& Case)
{ Cases.Add(Case); UE_LOG(LogTemp,Display,TEXT("CAMERA_CASE %s"),*Case); ++Phase; PhaseTime=GetWorld()->GetTimeSeconds(); }
void ANinjaCameraCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double Wall=FPlatformTime::Seconds();
	if (bFinished) { if (Wall-FinishedAt>2) FPlatformMisc::RequestExit(false); return; }
	if (Wall-Started>120) { Finish(FString::Printf(TEXT("Timeout at phase %d"),Phase)); return; }
	if (!Match)
	{
		Match=ANinjaMatchDirector::Find(this); Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)); PC=UGameplayStatics::GetPlayerController(this,0);
		if (!Match || !Ninja || !PC) return;
	}
	const float Now=GetWorld()->GetTimeSeconds(), Age=Now-PhaseTime;
	const FRotator View=PC->GetControlRotation();
	auto Error=[](float A,float B){ return FMath::Abs(FMath::FindDeltaAngleDegrees(A,B)); };
	if (Phase==2 || Phase==4)
	{
		const float Rate=Error(PreviousYaw,View.Yaw)*FPS; PeakYawRate=FMath::Max(PeakYawRate,Rate);
		if (Rate>Ninja->CameraMaxYawSpeed+1) { Finish(TEXT("Camera snapped faster than its angular speed limit")); return; }
	}
	PreviousYaw=View.Yaw;
	if (Phase==0 && Match->State==ENinjaMatchState::Ready)
	{
		for (ARedNinja* E:Match->Enemies) { E->SetActorTickEnabled(false); E->GetCharacterMovement()->StopMovementImmediately(); }
		Match->Confirm(); Next(TEXT("Prepared round uses normal start/countdown"));
	}
	else if (Phase==1 && Match->CanPlay())
	{
		// Isolate camera and mapped input from buildings, floor edges and enemy damage.
		Ninja->SetActorEnableCollision(false); Ninja->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		Ninja->GetCharacterMovement()->StopMovementImmediately(); Ninja->SetActorRotation(FRotator::ZeroRotator);
		PC->SetControlRotation(FRotator(-15,0,0)); PreviousYaw=0; SegmentStart=Ninja->GetActorLocation();
		Key(EKeys::D,true); Next(TEXT("Hold mapped D from a fixed initial world direction"));
	}
	else if (Phase==2 && Age>2.f)
	{
		const FVector Travel=Ninja->GetActorLocation()-SegmentStart;
		if (Error(View.Yaw,90)>6 || Travel.Y<700 || FMath::Abs(Travel.X)>80 || FMath::Abs(View.Pitch+15)>.1f) { Finish(FString::Printf(TEXT("D follow/orbit failure: yaw %.2f travel %.1f %.1f pitch %.1f"),View.Yaw,Travel.X,Travel.Y,View.Pitch)); return; }
		Stop(); SavedYaw=View.Yaw; Next(TEXT("D moves straight while camera settles behind the right turn; pitch is preserved"));
	}
	else if (Phase==3 && Age>.35f)
	{
		if (Error(View.Yaw,SavedYaw)>.1f) { Finish(TEXT("Camera continued turning after mapped key release")); return; }
		SegmentStart=Ninja->GetActorLocation(); Key(EKeys::S,true); Next(TEXT("Releasing movement holds the view; hold mapped S for a reversal"));
	}
	else if (Phase==4 && Age>3.f)
	{
		const FVector Travel=Ninja->GetActorLocation()-SegmentStart;
		if (Error(View.Yaw,-90)>8 || Travel.Y>-1200 || FMath::Abs(Travel.X)>160) { Finish(FString::Printf(TEXT("S reversal/orbit failure: yaw %.1f travel %.1f %.1f"),View.Yaw,Travel.X,Travel.Y)); return; }
		Stop(); Ninja->DoLook(20,5); Next(TEXT("S reverses direction and auto-yaw takes the short arc without a circular movement path"));
	}
	else if (Phase==5 && Age>.1f)
	{
		if (Error(View.Yaw,PreviousYaw)>1) return;
		SavedYaw=View.Yaw; SavedPitch=View.Pitch; Key(EKeys::D,true); Next(TEXT("Manual look changes the camera; begin moving during its grace period"));
	}
	else if (Phase==6 && Age>.65f)
	{
		if (Error(View.Yaw,SavedYaw)>.1f || FMath::Abs(View.Pitch-SavedPitch)>.1f) { Finish(TEXT("Automatic camera fought manual view input")); return; }
		Next(TEXT("Manual look keeps exclusive control while moving during the 1.2-second grace period"));
	}
	else if (Phase==7 && Age>2.f)
	{
		if (Error(View.Yaw,Ninja->GetActorRotation().Yaw)>7 || Error(View.Yaw,SavedYaw)<30 || FMath::Abs(View.Pitch-SavedPitch)>.1f) { Finish(TEXT("Auto-follow failed to resume after manual look or changed pitch")); return; }
		FString Shot; if (FParse::Value(FCommandLine::Get(),TEXT("CameraScreenshot="),Shot)) FScreenshotRequest::RequestScreenshot(Shot,false,false);
		Stop(); Ninja->bAutoAdjustCamera=false; PC->SetControlRotation(FRotator(0,0,0)); Key(EKeys::D,true);
		Next(TEXT("Automatic following resumes smoothly after manual look; vertical angle stays manual"));
	}
	else if (Phase==8 && Age>.8f)
	{
		if (Error(View.Yaw,0)>.1f) { Finish(TEXT("Camera opt-out was ignored")); return; }
		Stop(); Ninja->bAutoAdjustCamera=true; PC->SetControlRotation(FRotator(0,0,0)); Key(EKeys::D,true);
		Ninja->DoThrow(); Next(TEXT("Auto-camera can be disabled; begin moving and throwing to check aiming priority"));
	}
	else if (Phase==9 && Age>.4f)
	{
		if (Error(View.Yaw,0)>.1f) { Finish(TEXT("Auto-camera changed aim during throw protection")); return; }
		Match->TogglePause(); SavedYaw=View.Yaw; PauseWall=Wall;
		Next(TEXT("Throwing preserves aim for 0.6 seconds; pause the moving round"));
	}
	else if (Phase==10 && Wall-PauseWall>.4)
	{
		if (!UGameplayStatics::IsGamePaused(this) || Error(View.Yaw,SavedYaw)>.1f) { Finish(TEXT("Camera moved during pause")); return; }
		Match->TogglePause(); Next(TEXT("Pause holds the camera and resumes without losing input state"));
	}
	else if (Phase==11 && Age>1.7f)
	{
		if (Error(View.Yaw,Ninja->GetActorRotation().Yaw)>8) { Finish(TEXT("Following did not recover after aim protection and pause")); return; }
		Stop(); Ninja->Health=1; Ninja->ReceiveContactDamage(); SavedYaw=View.Yaw;
		Next(TEXT("Follow resumes after aiming and pause; trigger player defeat"));
	}
	else if (Phase==12 && Age>.3f)
	{
		if (Match->State!=ENinjaMatchState::Lost || Error(View.Yaw,SavedYaw)>.1f) { Finish(TEXT("View moved after result")); return; }
		Ninja->SetActorEnableCollision(true); Match->Confirm();
		Next(TEXT("Defeat holds the camera; retry restores normal player collision"));
	}
	else if (Phase==13 && Match->State==ENinjaMatchState::Ready)
	{
		if (Ninja->Health!=3 || Ninja->IsHidden()) { Finish(TEXT("Retry failed")); return; }
		Next(TEXT("Retry reaches ready with a visible healthy player and reset camera input state")); Finish(FString());
	}
}
void ANinjaCameraCheck::Finish(const FString& Error)
{
	UGameplayStatics::SetGamePaused(this,false); if (PC && Ninja) Stop();
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed")); Report->SetStringField(TEXT("error"),Error);
	Report->SetNumberField(TEXT("fixed_simulation_fps"),FPS); Report->SetNumberField(TEXT("peak_follow_yaw_degrees_per_second"),PeakYawRate);
	Report->SetStringField(TEXT("scope"),TEXT("Mapped D/S movement and real character/controller ticks; flying with collision disabled isolates camera from terrain. Fixed simulation FPS is not measured rendering performance."));
	TArray<TSharedPtr<FJsonValue>> Rows; for (const auto& Case:Cases) Rows.Add(MakeShared<FJsonValueString>(Case)); Report->SetArrayField(TEXT("passed_cases"),Rows);
	FString Json,Path; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FParse::Value(FCommandLine::Get(),TEXT("CameraReport="),Path); FFileHelper::SaveStringToFile(Json,*Path);
	UE_LOG(LogTemp,Display,TEXT("CAMERA_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error); bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
