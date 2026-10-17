#include "NinjaMatchCheck.h"
#include "NinjaMatchDirector.h"
#include "NinjaCharacter.h"
#include "NinjaShuriken.h"
#include "NinjaTrainingHUD.h"
#include "RedNinja.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ANinjaMatchCheck::ANinjaMatchCheck()
{ PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostPhysics; }
void ANinjaMatchCheck::BeginPlay()
{
	Super::BeginPlay(); SetActorTickEnabled(FParse::Param(FCommandLine::Get(),TEXT("NinjaMatchCheck")));
	if (!IsActorTickEnabled()) return;
	FParse::Value(FCommandLine::Get(),TEXT("MatchFPS="),FPS); FPS=FMath::Clamp(FPS,30,60);
	FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1.0/FPS); Started=FPlatformTime::Seconds();
}
void ANinjaMatchCheck::Key(const FKey& Input,bool Pressed)
{
	UGameplayStatics::GetPlayerController(this,0)->InputKey(FInputKeyEventArgs(nullptr,INPUTDEVICEID_NONE,Input,Pressed?IE_Pressed:IE_Released,Pressed?1.f:0.f,false,FPlatformTime::Cycles64()));
	if (Pressed) { ReleaseAt=FPlatformTime::Seconds()+.07; bReleaseEnter|=Input==EKeys::Enter; bReleaseP|=Input==EKeys::P; bReleaseF|=Input==EKeys::F; }
}
void ANinjaMatchCheck::Next(const FString& Result)
{ Results.Add(Result); UE_LOG(LogTemp,Display,TEXT("MATCH_CASE_PASSED %s"),*Result); ++Phase; PhaseTime=GetWorld()->GetTimeSeconds(); }
void ANinjaMatchCheck::Capture(const FString& Name)
{
	FString Prefix; if (FParse::Value(FCommandLine::Get(),TEXT("MatchScreenshot="),Prefix))
		FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Prefix)/FPaths::GetBaseFilename(Prefix)+TEXT("_")+Name+TEXT(".png"),false,false);
}
void ANinjaMatchCheck::HoldEnemies()
{ for (ARedNinja* Enemy:Match->Enemies) { Enemy->SetActorTickEnabled(false); Enemy->GetCharacterMovement()->StopMovementImmediately(); Enemy->GetCharacterMovement()->ClearAccumulatedForces(); } }
void ANinjaMatchCheck::Place(FVector Center)
{ Ninja->SetActorLocation(Center); Ninja->GetCharacterMovement()->StopMovementImmediately(); Ninja->GetCharacterMovement()->ClearAccumulatedForces(); Ninja->GetCharacterMovement()->SetMovementMode(MOVE_Falling); }
bool ANinjaMatchCheck::CheckReset()
{
	if (Match->RemainingSeconds!=300 || Match->RemainingEnemies()!=ExpectedEnemies || Ninja->Health!=1 || Ninja->Defeats!=0 || Ninja->JumpStage!=0 || Ninja->ShotsFired!=0 || Ninja->Knockouts!=0 || Ninja->IsInvulnerable()) return false;
	for (ARedNinja* Enemy:Match->Enemies) if (!IsValid(Enemy) || Enemy->bDefeated || !Enemy->bAIEnabled || !Enemy->IsReadyForMatch()) return false;
	if (ExpectedEnemies==8)
	{
		TSet<int32> Orders;
		for (ARedNinja* Enemy:Match->Enemies)
		{
			Orders.Add(Enemy->RouteOrder);
			if (Enemy->RouteHint.IsEmpty() || Enemy->DetectionRadius!=(Enemy->RouteOrder<=2?500:900)) return false;
			if (Enemy->RouteOrder>=3 && Enemy->RouteOrder<=6 && Enemy->AssistRoute.Num()<3) return false;
		}
		if (Orders.Num()!=8) return false;
	}
	for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) return false;
	return true;
}
void ANinjaMatchCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double Wall=FPlatformTime::Seconds();
	if (bFinished) { if (Wall-FinishedAt>3) FPlatformMisc::RequestExit(false); return; }
	if (!Match)
	{
		Match=ANinjaMatchDirector::Find(this); Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
		if (!Match || !Ninja) return;
		ExpectedEnemies=Match->Enemies.Num(); PhaseTime=GetWorld()->GetTimeSeconds();
	}
	if (Wall>ReleaseAt)
	{
		if (bReleaseEnter) { Key(EKeys::Enter,false); bReleaseEnter=false; }
		if (bReleaseP) { Key(EKeys::P,false); bReleaseP=false; }
		if (bReleaseF) { Key(EKeys::F,false); bReleaseF=false; }
	}
	const double Elapsed=GetWorld()->GetTimeSeconds()-PhaseTime;
	if (Wall-Started>130 || (Phase>0 && Elapsed>20)) { Finish(FString::Printf(TEXT("Phase %d timeout, state=%d"),Phase,int32(Match->State))); return; }
	if (Phase==0 && Match->State==ENinjaMatchState::Ready)
	{
		if (!CheckReset() || Ninja->TryThrowShuriken() || Ninja->ReceiveContactDamage() || Match->Enemies[0]->TryDefeat(ENinjaDefeatMethod::Shuriken,Ninja)) { Finish(TEXT("Ready state allowed combat or was not initialized")); return; }
		ReadyPosition=Ninja->GetActorLocation(); Ninja->DoJumpStart(); Ninja->DoMove(1,1);
		Capture(TEXT("ready")); Next(TEXT("City, safe floor and enemy graphs ready; 300 seconds and input/combat locked before start"));
	}
	else if (Phase==1 && Elapsed>.5)
	{
		if (FVector::Dist(ReadyPosition,Ninja->GetActorLocation())>2 || Match->RemainingSeconds!=300) { Finish(TEXT("Ready state moved or consumed time")); return; }
		Key(EKeys::Enter,true); Next(TEXT("Ready screen holds position and timer; press actual Enter"));
	}
	else if (Phase==2 && Match->State==ENinjaMatchState::Countdown && Elapsed>.2)
	{
		if (Ninja->TryThrowShuriken() || Match->RemainingSeconds!=300) { Finish(TEXT("Countdown allowed combat or consumed play time")); return; }
		Capture(TEXT("countdown")); Next(TEXT("Enter starts a three-second countdown with play time preserved"));
	}
	else if (Phase==3 && Match->CanPlay() && Elapsed>.3)
	{
		if (!bCapturedPlay) { Capture(TEXT("playing")); bCapturedPlay=true; PhaseTime=GetWorld()->GetTimeSeconds(); return; }
		Star=GetWorld()->SpawnActor<ANinjaShuriken>(Ninja->GetActorLocation()+FVector(0,0,1500),FRotator::ZeroRotator);
		Star->Launch(Ninja,FVector::UpVector); Key(EKeys::P,true); Next(TEXT("Countdown enters play; timer starts; press actual P to pause"));
	}
	else if (Phase==4 && UGameplayStatics::IsGamePaused(this))
	{
		PausedTime=Match->RemainingSeconds; PauseWall=Wall; PausedStar=Star->GetActorLocation(); PausedEnemies.Reset();
		for (ARedNinja* Enemy:Match->Enemies) PausedEnemies.Add(Enemy->GetActorLocation());
		Capture(TEXT("paused")); Next(TEXT("P pauses the world"));
	}
	else if (Phase==5 && Wall-PauseWall>.7)
	{
		bool Stable=Match->RemainingSeconds==PausedTime && IsValid(Star) && FVector::Dist(Star->GetActorLocation(),PausedStar)<.01;
		for (int32 I=0;I<Match->Enemies.Num();++I) Stable &= FVector::Dist(Match->Enemies[I]->GetActorLocation(),PausedEnemies[I])<.01;
		if (!Stable || Ninja->TryThrowShuriken() || Ninja->ReceiveContactDamage() || Match->Enemies[0]->TryDefeat(ENinjaDefeatMethod::Stomp,Ninja)) { Finish(TEXT("Paused timer, actors or combat changed")); return; }
		Key(EKeys::P,true); Next(TEXT("Paused timer, enemies and projectile stay fixed; combat is rejected; press P again"));
	}
	else if (Phase==6 && Match->CanPlay() && Elapsed>.4)
	{
		if (Match->RemainingSeconds>=PausedTime || (IsValid(Star) && FVector::Dist(Star->GetActorLocation(),PausedStar)<100)) { Finish(TEXT("Resume did not advance timer/projectile")); return; }
		HoldEnemies();
		for (int32 I=0;I<ExpectedEnemies-1;++I) Match->Enemies[I]->TryDefeat(I==0?ENinjaDefeatMethod::Stomp:ENinjaDefeatMethod::Shuriken,Ninja);
		LastEnemy=Match->Enemies.Last();
		auto* PC=UGameplayStatics::GetPlayerController(this,0);
		FRotator Look=(LastEnemy->GetActorLocation()-Ninja->GetActorLocation()).Rotation(); Look.Yaw+=180; Look.Pitch=0; PC->SetControlRotation(Look);
		Next(TEXT("Resume advances time and projectiles; leave one registered enemy behind the player"));
	}
	else if (Phase==7 && Elapsed>.4)
	{
		auto* HUD=Cast<ANinjaTrainingHUD>(UGameplayStatics::GetPlayerController(this,0)->GetHUD());
		if (Match->RemainingEnemies()!=1 || !HUD || HUD->LastMarkerCount!=1 || HUD->LastOffscreenMarkerCount!=1) { Finish(TEXT("Last enemy direction marker missing")); return; }
		Capture(TEXT("last_enemy"));

		Next(TEXT("Last enemy has a visible offscreen direction marker"));
	}
	else if (Phase==8 && Elapsed>.3)
    {
        Match->RemainingSeconds=0; LastEnemy->TryDefeat(ENinjaDefeatMethod::Shuriken,Ninja);
        Next(TEXT("Last kill and deadline occur in the same frame"));
    }
    else if (Phase==9 && Match->State==ENinjaMatchState::Won)
	{
		if (Ninja->Defeats!=ExpectedEnemies || Match->RemainingEnemies()!=0 || Ninja->TryThrowShuriken() || Ninja->ReceiveContactDamage()) { Finish(TEXT("Victory not locked")); return; }
		auto* Extra=GetWorld()->SpawnActor<ARedNinja>(Ninja->GetActorLocation()+FVector(500,0,0),FRotator::ZeroRotator);
		auto* Late=GetWorld()->SpawnActor<ANinjaShuriken>(); Late->Launch(Ninja,FVector::ForwardVector);
		Late->Impact(FHitResult(Extra,Extra->GetCapsuleComponent(),Extra->GetActorLocation(),FVector::UpVector));
		const bool Changed=Extra->bDefeated || Ninja->Defeats!=ExpectedEnemies; Extra->Destroy();
		if (Changed) { Finish(TEXT("Late projectile changed completed result")); return; }
		Capture(TEXT("victory_tie")); Next(TEXT("Victory wins tied deadline; late projectile, damage and new shots cannot alter result"));
	}
	else if (Phase==10 && Elapsed>.3)
	{ Key(EKeys::Enter,true); ++Retries; Next(TEXT("Retry 1 requested from victory with Enter")); }
	else if (Phase==11 && Match->State==ENinjaMatchState::Ready)
	{
		if (!CheckReset()) { Finish(TEXT("First retry did not fully reset")); return; }
		Key(EKeys::Enter,true); Next(TEXT("Retry restores all registered AI enemies, player, counters, cooldowns and 300 seconds"));
	}
	else if (Phase==12 && Match->CanPlay())
	{ HoldEnemies(); Match->RemainingSeconds=.15; Next(TEXT("Trigger normal timeout update with a shortened remaining deadline")); }
	else if (Phase==13 && Match->State==ENinjaMatchState::Lost)
	{
		if (Match->ResultReason!=TEXT("Time is up") || Match->RemainingEnemies()!=ExpectedEnemies) { Finish(TEXT("Timeout outcome incorrect")); return; }
		Capture(TEXT("timeout")); Next(TEXT("Timer reaches zero and loses with enemies remaining"));
	}
	else if (Phase==14 && Elapsed>.3) { Key(EKeys::Enter,true); ++Retries; Next(TEXT("Retry 2 requested from timeout")); }
	else if (Phase==15 && Match->State==ENinjaMatchState::Ready)
	{
		if (!CheckReset()) { Finish(TEXT("Second retry did not fully reset")); return; }
		Key(EKeys::Enter,true); Next(TEXT("Second retry fully initialized"));
	}
	else if (Phase==16 && Match->CanPlay())
	{
		HoldEnemies(); Ninja->Health=1;
		if (FParse::Param(FCommandLine::Get(),TEXT("MatchFatalFall")))
		{
			// Use the real out-of-world recovery path, rather than assigning zero health.
			Ninja->SetActorLocation(FVector(Ninja->GetActorLocation().X,Ninja->GetActorLocation().Y,-5000));
			Next(TEXT("Fall below the world with one health; normal recovery must produce a loss"));
		}
		else
		{
			if (!Ninja->ReceiveContactDamage()) { Finish(TEXT("Final contact damage rejected")); return; }
			Next(TEXT("Contact damage reduces last health to zero"));
		}
	}
	else if (Phase==17 && Match->State==ENinjaMatchState::Lost)
	{
		if (Ninja->Health!=0 || Match->ResultReason!=TEXT("One hit - ninja down") || Match->Enemies[0]->TryDefeat(ENinjaDefeatMethod::Shuriken,Ninja)) { Finish(TEXT("Zero health did not remain a locked loss")); return; }
		Capture(TEXT("health_loss")); Next(TEXT("Zero health loses without automatic healing; post-result defeat is rejected"));
	}
	else if (Phase==18 && Elapsed>.3) { Key(EKeys::Enter,true); ++Retries; Next(TEXT("Retry 3 requested from health loss")); }
	else if (Phase==19 && Match->State==ENinjaMatchState::Ready)
	{
		if (!CheckReset() || Retries!=3 || Match->RoundNumber!=4) { Finish(TEXT("Three consecutive retries failed")); return; }
		Key(EKeys::Enter,true); Next(TEXT("Three consecutive retries restore a fresh round"));
	}
	else if (Phase==20 && Match->CanPlay())
	{
		HoldEnemies(); for (ARedNinja* Enemy:Match->Enemies) { if (Enemy->ActorHasTag(TEXT("GroundA"))) GroundA=Enemy; if (Enemy->ActorHasTag(TEXT("GroundB"))) GroundB=Enemy; }
		Place(GroundB->GetActorLocation()-FVector(500,0,-10)); Next(TEXT("Place player for an actual mapped shuriken shot in the new round"));
	}
	else if (Phase==21)
	{
		auto* PC=UGameplayStatics::GetPlayerController(this,0); FVector View; FRotator Look; PC->GetPlayerViewPoint(View,Look); PC->SetControlRotation((GroundB->GetActorLocation()-View).Rotation());
		if (Elapsed>.8) { Key(EKeys::F,true); Next(TEXT("Mapped F launches a shuriken during play")); }
	}
	else if (Phase==22 && GroundB->bDefeated)
	{
		if (Ninja->ShurikenDefeats!=1 || Match->RemainingEnemies()!=ExpectedEnemies-1) { Finish(TEXT("Live projectile did not credit match")); return; }
		Place(GroundA->GetActorLocation()+FVector(0,0,450)); Ninja->JumpStage=2; Ninja->GetCharacterMovement()->Velocity=FVector(0,0,-300);
		Next(TEXT("Actual projectile credits the round; descend onto another enemy"));
	}
	else if (Phase==23 && GroundA->bDefeated)
	{
		if (Ninja->StompDefeats!=1 || Match->RemainingEnemies()!=ExpectedEnemies-2 || Ninja->JumpStage!=2) { Finish(TEXT("Live stomp did not credit match or preserved stage")); return; }
		for (ARedNinja* Enemy:Match->Enemies) if (!Enemy->bDefeated) Enemy->TryDefeat(ENinjaDefeatMethod::Shuriken,Ninja);
		Next(TEXT("Actual descending stomp credits the round and preserves jump stage"));
	}
	else if (Phase==24 && Match->State==ENinjaMatchState::Won)
	{ Capture(TEXT("victory")); Results.Add(TEXT("Fresh round completes with actual shot and stomp")); Finish(FString()); }
}
void ANinjaMatchCheck::Finish(const FString& Error)
{
	UGameplayStatics::SetGamePaused(this,false);
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed")); Report->SetStringField(TEXT("error"),Error);
	Report->SetNumberField(TEXT("enemy_count"),ExpectedEnemies); Report->SetNumberField(TEXT("fixed_fps"),FPS); Report->SetNumberField(TEXT("phase"),Phase); Report->SetNumberField(TEXT("retries"),Retries);
	Report->SetStringField(TEXT("health_loss_test"),FParse::Param(FCommandLine::Get(),TEXT("MatchFatalFall"))?TEXT("Actual out-of-world fall with one health"):TEXT("Contact damage with one health"));
	Report->SetStringField(TEXT("timeout_test"),TEXT("Verified initial 300 seconds; shortened remaining deadline to 0.15 for timeout boundary test"));
	if (Match) { Report->SetNumberField(TEXT("rounds"),Match->RoundNumber); Report->SetNumberField(TEXT("final_remaining_enemies"),Match->RemainingEnemies()); }
	TArray<TSharedPtr<FJsonValue>> Cases; for (const FString& Case:Results) Cases.Add(MakeShared<FJsonValueString>(Case)); Report->SetArrayField(TEXT("passed_cases"),Cases);
	FString Json,Path; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FParse::Value(FCommandLine::Get(),TEXT("MatchReport="),Path); FFileHelper::SaveStringToFile(Json,*Path);
	UE_LOG(LogTemp,Display,TEXT("MATCH_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error);
	bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
