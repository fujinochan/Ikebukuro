#include "NinjaMatchDirector.h"
#include "NinjaExplosion.h"
#include "NinjaCharacter.h"
#include "RedNinja.h"
#include "NinjaShuriken.h"
#include "City/LocalCityTileset.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "FileMediaSource.h"
#include "NinjaMusicCheck.h"
#include "NinjaExplosionCheck.h"
#include "NinjaCameraCheck.h"
#include "NinjaPerformanceCheck.h"

ANinjaMatchDirector::ANinjaMatchDirector()
{
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true;
	// Resolve outcomes after physics/projectiles so the last kill wins a tied deadline.
	PrimaryActorTick.TickGroup=TG_PostUpdateWork;
	MusicSound=CreateDefaultSubobject<UMediaSoundComponent>(TEXT("RoundMusicSound"));
	SetRootComponent(MusicSound); MusicSound->SetAutoActivate(false);
	MusicSound->bAllowSpatialization=false; MusicSound->bIsUISound=false;
	MusicSound->PrimaryComponentTick.bTickEvenWhenPaused=true;
}
ANinjaMatchDirector* ANinjaMatchDirector::Find(const UObject* Context)
{
	if (!Context || !Context->GetWorld()) return nullptr;
	for (TActorIterator<ANinjaMatchDirector> It(Context->GetWorld());It;++It) if (It->bActive) return *It;
	return nullptr;
}
bool ANinjaMatchDirector::AllowsGameplay(const UObject* Context)
{
	const auto* Match=Find(Context); return !Match || Match->CanPlay();
}
bool ANinjaMatchDirector::CanPlay() const { return State==ENinjaMatchState::Playing && !UGameplayStatics::IsGamePaused(this); }
void ANinjaMatchDirector::BeginPlay()
{
	Super::BeginPlay();
	for (const TCHAR* Flag:{TEXT("CityAreaCheck"),TEXT("CityTextureCheck"),TEXT("NinjaEncounterCheck"),TEXT("NinjaCombatCheck"),TEXT("NinjaCityCombatCheck")})
		if (FParse::Param(FCommandLine::Get(),Flag)) bActive=false;
	SetActorTickEnabled(bActive); if (!bActive) return;
	bMusicRound=(GetWorld()->GetMapName().Contains(TEXT("L_Ikebukuro")) || FParse::Param(FCommandLine::Get(),TEXT("NinjaMusicCheck"))) &&
		!FParse::Param(FCommandLine::Get(),TEXT("NinjaLegacyTimerCheck"));
	if (bMusicRound) OpenRoundMusic();
	Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
	for (TActorIterator<ARedNinja> It(GetWorld());It;++It)
	{
		FNinjaMatchSpawn Spawn; Spawn.Class=It->GetClass(); Spawn.Transform=It->GetActorTransform();
		Spawn.Area=It->PatrolArea; Spawn.Tags=It->Tags; Spawn.bAI=It->bAIEnabled;
		Spawn.RouteOrder=It->RouteOrder; Spawn.RouteHint=It->RouteHint; Spawn.AssistRoute=It->AssistRoute;
		Spawn.DetectionRadius=It->DetectionRadius; Spawn.LeashRadius=It->LeashRadius;
		Spawns.Add(Spawn); Enemies.Add(*It);
	}
	if (auto* PC=UGameplayStatics::GetPlayerController(this,0))
	{
		EnableInput(PC);
		InputComponent->BindKey(EKeys::Enter,IE_Pressed,this,&ANinjaMatchDirector::Confirm).bExecuteWhenPaused=true;
		InputComponent->BindKey(EKeys::P,IE_Pressed,this,&ANinjaMatchDirector::TogglePause).bExecuteWhenPaused=true;
		InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&ANinjaMatchDirector::TogglePause).bExecuteWhenPaused=true;
	}
	if (FParse::Param(FCommandLine::Get(),TEXT("NinjaMusicCheck"))) GetWorld()->SpawnActor<ANinjaMusicCheck>();
	if (FParse::Param(FCommandLine::Get(),TEXT("NinjaExplosionCheck"))) GetWorld()->SpawnActor<ANinjaExplosionCheck>();
	if (FParse::Param(FCommandLine::Get(),TEXT("NinjaCameraCheck"))) GetWorld()->SpawnActor<ANinjaCameraCheck>();
	if (FParse::Param(FCommandLine::Get(),TEXT("NinjaPerformanceCheck"))) GetWorld()->SpawnActor<ANinjaPerformanceCheck>();
}
void ANinjaMatchDirector::StopRoundMusic()
{
	MusicSound->Stop(); MusicSound->SetMediaPlayer(nullptr);
	if (RoundMusic)
	{
		RoundMusic->OnEndReached.RemoveAll(this); RoundMusic->OnMediaOpenFailed.RemoveAll(this);
		RoundMusic->Close(); RoundMusic=nullptr;
	}
	MusicSource=nullptr; bMusicStarted=false;
}
void ANinjaMatchDirector::OpenRoundMusic()
{
	StopRoundMusic(); bMusicReady=false; bSongEnded=false; MusicError.Reset();
	const bool MissingCheck=FParse::Param(FCommandLine::Get(),TEXT("NinjaMusicCheck")) && FParse::Param(FCommandLine::Get(),TEXT("MusicMissingFileCheck"));
	const FString Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/(MissingCheck?TEXT("Movies/__missing_music_check__.m4a"):TEXT("Movies/Ikebukuro Ninja City.m4a")));
	if (!IFileManager::Get().FileExists(*Path)) { MusicOpenFailed(Path); return; }
	RoundMusic=NewObject<UMediaPlayer>(this); RoundMusic->PlayOnOpen=false;
	RoundMusic->SetDesiredPlayerName(TEXT("WmfMedia")); RoundMusic->SetLooping(false);
	RoundMusic->OnEndReached.AddDynamic(this,&ANinjaMatchDirector::MusicEnded);
	RoundMusic->OnMediaOpenFailed.AddDynamic(this,&ANinjaMatchDirector::MusicOpenFailed);
	MusicSource=NewObject<UFileMediaSource>(this); MusicSource->PrecacheFile=true; MusicSource->SetFilePath(Path);
	MusicSound->SetMediaPlayer(RoundMusic); MusicSound->SetVolumeMultiplier(.35f);
	if (!RoundMusic->OpenSource(MusicSource)) MusicOpenFailed(Path);
}
void ANinjaMatchDirector::MusicOpenFailed(FString Url)
{
	MusicError=TEXT("Music unavailable. Press ENTER to retry.");
	UE_LOG(LogTemp,Error,TEXT("ROUND_MUSIC_OPEN_FAILED %s"),*Url);
}
void ANinjaMatchDirector::MusicEnded()
{
	if (State==ENinjaMatchState::Playing && bMusicStarted) bSongEnded=true;
}
void ANinjaMatchDirector::StartRound()
{
	State=ENinjaMatchState::Playing;
	if (!bMusicRound) return;
	if (!RoundMusic || !RoundMusic->Play()) { Finish(false,TEXT("Music could not start")); return; }
	bMusicStarted=true; MusicSound->UpdatePlayer(); MusicSound->Start();
}
bool ANinjaMatchDirector::IsPrepared() const
{
	if (bMusicRound && (!bMusicReady || !MusicError.IsEmpty())) return false;
	if (!Ninja || !Ninja->GetCharacterMovement()->IsMovingOnGround() || Enemies.IsEmpty()) return false;
	for (ARedNinja* Enemy:Enemies) if (!IsValid(Enemy) || Enemy->GetCharacterMovement()->IsFalling() || !Enemy->IsReadyForMatch()) return false;
	// The isolated NullRHI route check validates saved collision without a renderer.
	// Normal play and the rendered match suite still wait for visible city tiles.
	const bool CollisionOnly=FParse::Param(FCommandLine::Get(),TEXT("NinjaRouteCheck")) && FParse::Param(FCommandLine::Get(),TEXT("NullRHI"));
	for (TActorIterator<ALocalCityTileset> It(GetWorld());!CollisionOnly && It;++It)
	{
		if (It->GetLoadProgress()<99) return false;
		TArray<UStaticMeshComponent*> Meshes; It->GetComponents(Meshes);
		if (!Meshes.ContainsByPredicate([](auto* Mesh){return Mesh->GetStaticMesh() && Mesh->IsVisible();})) return false;
	}
	FHitResult Hit; const FVector Feet=Ninja->GetActorLocation()-FVector(0,0,Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return GetWorld()->LineTraceSingleByObjectType(Hit,Feet+FVector(0,0,10),Feet-FVector(0,0,30),FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(MatchStartFloor),true));
}
void ANinjaMatchDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (UGameplayStatics::IsGamePaused(this)) return;
	if (State==ENinjaMatchState::Won || State==ENinjaMatchState::Lost) ResultAge+=DeltaSeconds;
	if (!Ninja) Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
	if (State==ENinjaMatchState::Loading)
	{
		if (bMusicRound && !bMusicReady && RoundMusic && RoundMusic->IsReady() && RoundMusic->GetDuration().GetTotalSeconds()>0)
		{
			RoundDurationSeconds=float(RoundMusic->GetDuration().GetTotalSeconds());
			RemainingSeconds=RoundDurationSeconds; bMusicReady=true;
			UE_LOG(LogTemp,Display,TEXT("ROUND_MUSIC_READY %.3f seconds"),RoundDurationSeconds);
		}
		PreparedSeconds=IsPrepared()?PreparedSeconds+DeltaSeconds:0;
		if (PreparedSeconds>=.5)
		{
			StartFoot=Ninja->GetActorLocation()-FVector(0,0,Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			if (RoundNumber==1) StartRotation=Ninja->GetController()->GetControlRotation();
			State=ENinjaMatchState::Ready;
		}
	}
	else if (State==ENinjaMatchState::Countdown)
	{
		CountdownSeconds=FMath::Max(0.f,CountdownSeconds-DeltaSeconds);
		if (CountdownSeconds<=0) StartRound();
	}
	else if (State==ENinjaMatchState::Playing)
	{
		if (bMusicRound && RoundMusic) RemainingSeconds=FMath::Clamp(RoundDurationSeconds-float(RoundMusic->GetTime().GetTotalSeconds()),0.f,RoundDurationSeconds);
		else RemainingSeconds=FMath::Max(0.f,RemainingSeconds-DeltaSeconds);
		if (RemainingEnemies()==0) Finish(true,TEXT("All red ninjas defeated"));
		else if (Ninja->Health<=0) Finish(false,TEXT("No health remaining"));
		else if (bMusicRound && (!RoundMusic || RoundMusic->HasError() || !MusicError.IsEmpty())) Finish(false,TEXT("Music playback failed"));
		else if (bMusicRound && bSongEnded) { RemainingSeconds=0; Finish(false,TEXT("The song has ended")); }
		else if (!bMusicRound && RemainingSeconds<=0) Finish(false,TEXT("Time is up"));
	}
}
bool ANinjaMatchDirector::CanDefeat(ARedNinja* Enemy) const { return CanPlay() && Enemies.Contains(Enemy) && !Credited.Contains(Enemy); }
void ANinjaMatchDirector::CreditDefeat(ARedNinja* Enemy) { if (CanDefeat(Enemy)) Credited.Add(Enemy); }
void ANinjaMatchDirector::Confirm()
{
	if (State==ENinjaMatchState::Loading && bMusicRound && !MusicError.IsEmpty()) { OpenRoundMusic(); return; }
	if (State==ENinjaMatchState::Ready) { CountdownSeconds=3; State=ENinjaMatchState::Countdown; }
	else if (State==ENinjaMatchState::Won || State==ENinjaMatchState::Lost) Retry();
}
void ANinjaMatchDirector::TogglePause()
{
	if (State==ENinjaMatchState::Playing || State==ENinjaMatchState::Countdown)
	{
		const bool Paused=UGameplayStatics::IsGamePaused(this);
		if (!Paused && bMusicStarted && RoundMusic) RoundMusic->Pause();
		UGameplayStatics::SetGamePaused(this,!Paused);
		if (Paused && bMusicStarted && RoundMusic) RoundMusic->Play();
	}
}
void ANinjaMatchDirector::Finish(bool bWon,const FString& Reason)
{
	State=bWon?ENinjaMatchState::Won:ENinjaMatchState::Lost; ResultReason=Reason; ResultAge=0;
	StopRoundMusic();
	Ninja->GetCharacterMovement()->StopMovementImmediately(); Ninja->GetCharacterMovement()->ClearAccumulatedForces(); Ninja->GetCharacterMovement()->DisableMovement();
	Ninja->DoJumpEnd(); Ninja->DoSprintEnd();
	for (ARedNinja* Enemy:Enemies) if (IsValid(Enemy))
	{ Enemy->GetCharacterMovement()->StopMovementImmediately(); Enemy->GetCharacterMovement()->ClearAccumulatedForces(); Enemy->GetCharacterMovement()->DisableMovement(); }
	for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) It->Destroy();
}
void ANinjaMatchDirector::Retry()
{
	if (State!=ENinjaMatchState::Won && State!=ENinjaMatchState::Lost) return;
	UGameplayStatics::SetGamePaused(this,false); State=ENinjaMatchState::Loading;
	ANinjaExplosion::Clear(GetWorld());
	for (TActorIterator<ANinjaShuriken> It(GetWorld());It;++It) It->Destroy();
	for (ARedNinja* Enemy:Enemies) if (IsValid(Enemy)) Enemy->Destroy();
	Enemies.Reset(); Credited.Reset(); RemainingSeconds=RoundDurationSeconds; CountdownSeconds=3; PreparedSeconds=0; ResultReason.Reset(); ++RoundNumber;
	if (bMusicRound) OpenRoundMusic();
	Ninja->ResetForMatch(StartFoot,StartRotation);
	for (const auto& Spawn:Spawns)
	{
		auto* Enemy=GetWorld()->SpawnActorDeferred<ARedNinja>(Spawn.Class,Spawn.Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Enemy->PatrolArea=Spawn.Area; Enemy->Tags=Spawn.Tags; Enemy->bAIEnabled=Spawn.bAI;
		Enemy->RouteOrder=Spawn.RouteOrder; Enemy->RouteHint=Spawn.RouteHint; Enemy->AssistRoute=Spawn.AssistRoute;
		Enemy->DetectionRadius=Spawn.DetectionRadius; Enemy->LeashRadius=Spawn.LeashRadius;
		Enemies.Add(Enemy); Enemy->FinishSpawning(Spawn.Transform);
	}
}
void ANinjaMatchDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bActive) UGameplayStatics::SetGamePaused(this,false);
	StopRoundMusic();
	Super::EndPlay(Reason);
}
