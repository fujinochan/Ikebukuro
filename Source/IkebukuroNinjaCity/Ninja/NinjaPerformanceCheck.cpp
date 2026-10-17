#include "NinjaPerformanceCheck.h"
#include "NinjaCharacter.h"
#include "NinjaMatchDirector.h"
#include "NinjaExplosion.h"
#include "RedNinja.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMemory.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "DynamicRHI.h"
#include "RHIGlobals.h"
#include "RenderTimer.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"

ANinjaPerformanceCheck::ANinjaPerformanceCheck()
{ PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork; }
void ANinjaPerformanceCheck::BeginPlay()
{
	Super::BeginPlay(); Started=Previous=FPlatformTime::Seconds();
	FApp::SetUseFixedTimeStep(false);
	GEngine->bSmoothFrameRate=false; GEngine->bUseFixedFrameRate=false;
	if (auto* Settings=GEngine->GetGameUserSettings())
	{
		Settings->SetFullscreenMode(EWindowMode::Windowed); Settings->SetScreenResolution(FIntPoint(1920,1080));
		Settings->ApplyResolutionSettings(false);
	}
}
void ANinjaPerformanceCheck::EnterScene()
{
	ANinjaExplosion::Clear(GetWorld()); Frames.Reset(); GPU.Reset(); Render.Reset();
	SceneStarted=Previous=FPlatformTime::Seconds(); BlastAt=-10;
	const FVector Foot=Scene==1?FVector(7618,-19861,2608):StartPoint;
	Ninja->SetActorLocation(Foot+FVector(0,0,100)); Ninja->GetCharacterMovement()->StopMovementImmediately();
	Ninja->GetCharacterMovement()->SetMovementMode(Scene==2?MOVE_Flying:MOVE_Falling);
	Ninja->SetActorEnableCollision(Scene!=2);
	Ninja->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);
	PC->SetControlRotation(FRotator(-15,Scene==1?-45:150,0));
	UE_LOG(LogTemp,Display,TEXT("PERF_SCENE %d begin"),Scene);
}
static TSharedPtr<FJsonObject> Distribution(TArray<double> Values)
{
	auto Result=MakeShared<FJsonObject>(); if (Values.IsEmpty()) return Result;
	double Sum=0; for (double Value:Values) Sum+=Value; Values.Sort();
	Result->SetNumberField(TEXT("mean_ms"),Sum/Values.Num());
	Result->SetNumberField(TEXT("p95_ms"),Values[FMath::Min(Values.Num()-1,FMath::FloorToInt(Values.Num()*.95))]);
	Result->SetNumberField(TEXT("max_ms"),Values.Last()); return Result;
}
void ANinjaPerformanceCheck::RecordScene()
{
	const TCHAR* Names[]={TEXT("street_8_patrolling_enemies"),TEXT("middle_rooftop"),TEXT("scripted_14mps_flight"),TEXT("eight_simultaneous_explosions")};
	auto Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("scene"),Names[Scene]); Row->SetNumberField(TEXT("frames"),Frames.Num());
	Row->SetObjectField(TEXT("wall_frame"),Distribution(Frames)); Row->SetObjectField(TEXT("gpu_frame"),Distribution(GPU)); Row->SetObjectField(TEXT("render_thread"),Distribution(Render));
	double Sum=0; int32 Slow=0; for (double Value:Frames) { Sum+=Value; Slow+=Value>33.333; }
	Row->SetNumberField(TEXT("average_fps"),Frames.IsEmpty()?0:1000*Frames.Num()/Sum);
	Row->SetNumberField(TEXT("frames_over_33ms_percent"),Frames.IsEmpty()?0:100.*Slow/Frames.Num());
	Row->SetNumberField(TEXT("process_memory_mb"),FPlatformMemory::GetStats().UsedPhysical/(1024.*1024.));
	Scenes.Add(MakeShared<FJsonValueObject>(Row));
	FString Shot; if (FParse::Value(FCommandLine::Get(),TEXT("PerfScreenshot="),Shot)) FScreenshotRequest::RequestScreenshot(Shot+FString::FromInt(Scene)+TEXT(".png"),false,false);
	UE_LOG(LogTemp,Display,TEXT("PERF_SCENE %d fps=%.2f frames=%d"),Scene,Frames.IsEmpty()?0:1000*Frames.Num()/Sum,Frames.Num());
}
void ANinjaPerformanceCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double Wall=FPlatformTime::Seconds(), Elapsed=Wall-Previous; Previous=Wall;
	if (bFinished) { if (Wall-FinishedAt>2) FPlatformMisc::RequestExit(false); return; }
	if (Wall-Started>240) { Finish(TEXT("Preparation or measurement timed out")); return; }
	if (!Match)
	{
		Match=ANinjaMatchDirector::Find(this); Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)); PC=UGameplayStatics::GetPlayerController(this,0);
		if (!Match || !Ninja || !PC) return;
		for (ARedNinja* E:Match->Enemies) { E->DetectionRadius=0; E->bRangedAttackEnabled=false; } // Keep real patrols without combat ending the sample.
		StartPoint=Ninja->GetActorLocation()-FVector(0,0,100);
	}
	if (Match->State==ENinjaMatchState::Ready) Match->Confirm();
	if (!Match->CanPlay()) return;
	if (Scene<0) { Scene=0; EnterScene(); return; }
	if (bAdvanceScene)
	{
		if (Wall-SceneStarted<11.3) return; // Render the labelled screenshot outside the measured interval.
		bAdvanceScene=false; if (++Scene==4) Finish(FString()); else EnterScene(); return;
	}
	const double Age=Wall-SceneStarted;
	if (Scene==2)
	{
		const double A=Age*.2;
		Ninja->SetActorLocation(FVector(10000+7000*FMath::Cos(A),-11000+7000*FMath::Sin(A),7000));
		PC->SetControlRotation(FRotator(-20,FMath::RadiansToDegrees(A)+90,0));
	}
	if (Scene==3 && Age-BlastAt>4.4)
	{
		ANinjaExplosion::Clear(GetWorld()); BlastAt=Age;
		for (int32 I=0; I<8; ++I)
		{
			const double A=I*PI/4;
			ANinjaExplosion::Emit(this,Ninja->GetActorLocation()+FVector(FMath::Cos(A)*700,FMath::Sin(A)*700,0));
		}
	}
	if (Age>=3 && Age<11)
	{
		Frames.Add(Elapsed*1000);
		const double GPUMs=FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()); if (GPUMs>0) GPU.Add(GPUMs);
		const double RenderMs=FPlatformTime::ToMilliseconds(GRenderThreadTime); if (RenderMs>0) Render.Add(RenderMs);
	}
	if (Age>=11)
	{
		if (Frames.Num()<10) { Finish(TEXT("Too few real frames to measure")); return; }
		RecordScene(); bAdvanceScene=true;
	}
}
void ANinjaPerformanceCheck::Finish(const FString& Error)
{
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed")); Report->SetStringField(TEXT("error"),Error);
	Report->SetStringField(TEXT("cpu"),FPlatformMisc::GetCPUBrand()); Report->SetStringField(TEXT("gpu"),GRHIAdapterName);
	Report->SetNumberField(TEXT("physical_memory_gb"),FPlatformMemory::GetConstants().TotalPhysical/(1024.*1024.*1024.));
	const FIntPoint Size=GEngine->GameViewport->Viewport->GetSizeXY(); Report->SetNumberField(TEXT("viewport_width"),Size.X); Report->SetNumberField(TEXT("viewport_height"),Size.Y);
	auto Settings=MakeShared<FJsonObject>();
	for (const TCHAR* Name:{TEXT("r.ScreenPercentage"),TEXT("r.VSync"),TEXT("t.MaxFPS"),TEXT("r.DynamicGlobalIlluminationMethod"),TEXT("r.ReflectionMethod"),TEXT("r.Lumen.DiffuseIndirect.Allow"),TEXT("r.Lumen.Reflections.Allow"),TEXT("r.Shadow.Virtual.Enable"),TEXT("r.RayTracing"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.TextureQuality")})
		if (const auto* CVar=IConsoleManager::Get().FindConsoleVariable(Name)) Settings->SetStringField(Name,CVar->GetString());
	Report->SetObjectField(TEXT("cvars"),Settings); Report->SetArrayField(TEXT("scenes"),Scenes);
	Report->SetStringField(TEXT("scope"),TEXT("UE Editor -game, real wall-clock frames, no fixed timestep, no vsync or cap; 3s warmup + 8s sample per scene; audio disabled; eight real AI patrols; flight scripted with collision disabled; no human full-round playtest"));
	FString Path,Json; FParse::Value(FCommandLine::Get(),TEXT("PerfReport="),Path); FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FFileHelper::SaveStringToFile(Json,*Path);
	UE_LOG(LogTemp,Display,TEXT("PERF_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error); bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
