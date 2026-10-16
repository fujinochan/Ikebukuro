#include "NinjaMovementCheck.h"
#include "NinjaCharacter.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ANinjaMovementCheck::ANinjaMovementCheck()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void ANinjaMovementCheck::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(FParse::Param(FCommandLine::Get(), TEXT("NinjaMovementCheck")));
	FParse::Value(FCommandLine::Get(), TEXT("NinjaCheckFPS="), FPS);
	FPS = FMath::Clamp(FPS, 30, 120);
	if (IsActorTickEnabled())
	{
		FApp::SetUseFixedTimeStep(true);
		FApp::SetFixedDeltaTime(1.0 / FPS);
	}
	RealStart = FPlatformTime::Seconds();
}

void ANinjaMovementCheck::Key(const FKey& InputKey, bool bPressed)
{
	if (auto* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, InputKey,
			bPressed ? IE_Pressed : IE_Released, bPressed ? 1.f : 0.f, false, FPlatformTime::Cycles64()));
	}
}

void ANinjaMovementCheck::Space()
{
	Key(EKeys::SpaceBar, true);
	bSpaceDown = true;
	PressTime = GetWorld()->GetTimeSeconds();
}

void ANinjaMovementCheck::PrepareCase()
{
	Key(EKeys::W, false);
	Key(EKeys::LeftShift, false);
	Key(EKeys::SpaceBar, false);
	bSpaceDown = false;
	Ninja->SetActorLocation(FVector(0, Case >= 4 ? 3000 : 6000, 150));
	Ninja->GetCharacterMovement()->StopMovementImmediately();
	Ninja->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (auto* PC = Cast<APlayerController>(Ninja->GetController())) PC->SetControlRotation(FRotator(-20, 0, 0));
	Phase = 3;
	PhaseStart = GetWorld()->GetTimeSeconds();
	MaxStage = 0;
	bBadPressSent = false;
	bRejected = false;
	ThirdTime = 0;
}

void ANinjaMovementCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Phase == 8)
	{
		if (FPlatformTime::Seconds() - DoneAt > 2) FPlatformMisc::RequestExit(false);
		return;
	}
	if (FPlatformTime::Seconds() - RealStart > 150) { Finish(TEXT("Verification timeout")); return; }
	Ninja = Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!Ninja) return;
	const double Now = GetWorld()->GetTimeSeconds();
	const double Elapsed = Now - PhaseStart;
	if (Phase == 0)
	{
		if (Now < 1 || Ninja->GetCharacterMovement()->IsFalling()) return;
		Ninja->SetActorLocation(FVector(0, 6000, 100));
		Cast<APlayerController>(Ninja->GetController())->SetControlRotation(FRotator(-20, 0, 0));
		Key(EKeys::W, true);
		Phase = 1;
		PhaseStart = Now;
	}
	else if (Phase == 1 && Elapsed > 1)
	{
		WalkMeasured = Ninja->GetVelocity().Size2D();
		if (WalkMeasured < 650 || WalkMeasured > 750) { Finish(TEXT("Mapped W / walk speed failed")); return; }
		Key(EKeys::LeftShift, true);
		Phase = 2;
		PhaseStart = Now;
	}
	else if (Phase == 2 && Elapsed > 1)
	{
		RunMeasured = Ninja->GetVelocity().Size2D();
		if (RunMeasured < 1350 || !Ninja->bIsSprinting) { Finish(TEXT("Mapped Shift / sprint failed")); return; }
		PrepareCase();
	}
	else if (Phase == 3 && Elapsed > 0.4 && !Ninja->GetCharacterMovement()->IsFalling())
	{
		if (Case >= 4)
		{
			Key(EKeys::W, true);
			Key(EKeys::LeftShift, true);
			Phase = 4;
			PhaseStart = Now;
		}
		else
		{
			BaseZ = Ninja->GetActorLocation().Z;
			PeakZ = BaseZ;
			Space(); Phase = 5; PhaseStart = Now;
		}
	}
	else if (Phase == 4 && Elapsed > 0.3)
	{
		BaseZ = Ninja->GetActorLocation().Z;
		PeakZ = BaseZ;
		MaxX = Ninja->GetActorLocation().X;
		Space(); Phase = 5; PhaseStart = Now;
	}
	else if (Phase == 5)
	{
		PeakZ = FMath::Max(PeakZ, static_cast<float>(Ninja->GetActorLocation().Z));
		MaxX = FMath::Max(MaxX, static_cast<float>(Ninja->GetActorLocation().X));
		MaxStage = FMath::Max(MaxStage, Ninja->JumpStage);
		if (bSpaceDown && Case != 0 && Now - PressTime > 0.08)
		{
			Key(EKeys::SpaceBar, false); bSpaceDown = false;
		}
		if (Case == 1 && !bBadPressSent && Elapsed > 0.18 && !bSpaceDown)
		{
			Space(); bBadPressSent = true;
		}
		if (Case == 2 && !bBadPressSent && Ninja->JumpStage == 1 && Ninja->GetSecondsToJumpApex() < -0.25 && !bSpaceDown)
		{
			Space(); bBadPressSent = true;
		}
		if (Case == 1) bRejected |= Ninja->JumpFeedback.StartsWith(TEXT("Too early"));
		if (Case == 2) bRejected |= Ninja->JumpFeedback.StartsWith(TEXT("Too late"));
		if ((Case == 3 || Case == 5) && !bSpaceDown && Ninja->JumpStage > 0 && Ninja->JumpStage < 3 && Ninja->GetSecondsToJumpApex() <= 0)
			Space();
		if (MaxStage == 3 && ThirdTime == 0) ThirdTime = Now;
		if (Case == 3 && ThirdTime > 0 && !bBadPressSent && Now - ThirdTime > 0.2 && !bSpaceDown)
		{
			Space(); bBadPressSent = true;
		}
		if (Case == 3) bRejected |= Ninja->JumpFeedback.StartsWith(TEXT("3 / 3"));
		if (Case == 3 && Ninja->CanChainJump() && Ninja->JumpStage == 2 && !bScreenshot)
		{
			FString Path;
			if (FParse::Value(FCommandLine::Get(), TEXT("NinjaCheckScreenshot="), Path))
				FScreenshotRequest::RequestScreenshot(Path, false, false);
			bScreenshot = true;
		}
		if (Elapsed > (Case == 4 ? 3.6 : Case == 0 ? 2.3 : 0.6) && !Ninja->GetCharacterMovement()->IsFalling() && Ninja->JumpStage == 0)
		{
			const bool Triple = Case == 3 || Case == 5;
			const float Height = PeakZ - BaseZ;
			const bool bPassed = MaxStage == (Triple ? 3 : 1) && Height > (Triple ? 3000 : 350) &&
				((Case != 1 && Case != 2 && Case != 3) || bRejected) &&
				(Case != 4 || (MaxX > 4400 && MaxX < 4500)) &&
				(Case != 5 || (MaxX > 5550 && FMath::Abs(Ninja->GetActorLocation().Z - BaseZ) < 20));
			const TCHAR* Names[] = {TEXT("hold_no_repeat"), TEXT("early_rejected"), TEXT("late_rejected"),
				TEXT("triple_and_fourth_rejected"), TEXT("single_blocked_by_30m"), TEXT("triple_clears_30m")};
			auto Result = MakeShared<FJsonObject>();
			Result->SetStringField(TEXT("case"), Names[Case]);
			Result->SetBoolField(TEXT("passed"), bPassed);
			Result->SetNumberField(TEXT("max_stage"), MaxStage);
			Result->SetNumberField(TEXT("height_cm"), Height);
			Result->SetNumberField(TEXT("max_x_cm"), MaxX);
			Result->SetBoolField(TEXT("landed_and_reset"), true);
			Results.Add(MakeShared<FJsonValueObject>(Result));
			if (!bPassed) { Finish(FString::Printf(TEXT("Case %s failed"), Names[Case])); return; }
			++Case;
			if (Case == 6) Finish(); else PrepareCase();
		}
	}
}

void ANinjaMovementCheck::Finish(const FString& Error)
{
	Key(EKeys::SpaceBar, false); Key(EKeys::LeftShift, false); Key(EKeys::W, false);
	auto Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("status"), Error.IsEmpty() ? TEXT("passed") : TEXT("failed"));
	Report->SetStringField(TEXT("error"), Error);
	Report->SetNumberField(TEXT("simulation_fps"), FPS);
	Report->SetNumberField(TEXT("walk_speed_cm_s"), WalkMeasured);
	Report->SetNumberField(TEXT("run_speed_cm_s"), RunMeasured);
	Report->SetStringField(TEXT("input_method"), TEXT("PlayerController mapped W / LeftShift / Space key events"));
	Report->SetArrayField(TEXT("cases"), Results);
	FString Path;
	if (!FParse::Value(FCommandLine::Get(), TEXT("NinjaCheckReport="), Path))
		Path = FPaths::ProjectSavedDir() / TEXT("Validation/step3-movement.json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FString Json;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *Path);
	UE_LOG(LogTemp, Display, TEXT("NINJA_MOVEMENT_%s %s"), Error.IsEmpty() ? TEXT("PASSED") : TEXT("FAILED"), *Error);
	Phase = 8;
	DoneAt = FPlatformTime::Seconds();
}
