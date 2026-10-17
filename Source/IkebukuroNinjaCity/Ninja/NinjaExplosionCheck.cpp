#include "NinjaExplosionCheck.h"
#include "NinjaExplosion.h"
#include "NinjaMatchDirector.h"
#include "NinjaCharacter.h"
#include "RedNinja.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ANinjaExplosionCheck::ANinjaExplosionCheck()
{ PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bTickEvenWhenPaused=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork; }
void ANinjaExplosionCheck::Next(const FString& Case)
{ Cases.Add(Case); UE_LOG(LogTemp,Display,TEXT("EXPLOSION_CASE %s"),*Case); ++Phase; PhaseAge=0; }
void ANinjaExplosionCheck::View(FVector Center)
{
	if (!Camera) Camera=GetWorld()->SpawnActor<ACameraActor>();
	const FVector Eye=Center+FVector(-850,-850,420);
	Camera->SetActorLocation(Eye); Camera->SetActorRotation((Center-Eye).Rotation());
	UGameplayStatics::GetPlayerController(this,0)->SetViewTarget(Camera);
}
void ANinjaExplosionCheck::Capture(const FString& Suffix)
{
	FString Prefix; if (FParse::Value(FCommandLine::Get(),TEXT("ExplosionScreenshot="),Prefix))
		FScreenshotRequest::RequestScreenshot(Prefix+TEXT("_")+Suffix+TEXT(".png"),false,false);
}
void ANinjaExplosionCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double Wall=FPlatformTime::Seconds();
	if (!Started) Started=Wall;
	if (bFinished) { if (Wall-FinishedAt>2) FPlatformMisc::RequestExit(false); return; }
	if (Wall-Started>100) { Finish(FString::Printf(TEXT("Timeout in phase %d"),Phase)); return; }
	PhaseAge+=DeltaSeconds;
	if (!Match) { Match=ANinjaMatchDirector::Find(this); Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)); if (!Match || !Ninja) return; }
	if (Phase==0 && Match->State==ENinjaMatchState::Ready)
	{
		for (ARedNinja* E:Match->Enemies) { E->SetActorTickEnabled(false); E->GetCharacterMovement()->StopMovementImmediately(); }
		EnemyBase=ANinjaExplosion::EnemyBursts; PlayerBase=ANinjaExplosion::PlayerBursts;
		Match->Confirm(); Next(TEXT("Start a prepared round with enemies held stationary for effects inspection"));
	}
	else if (Phase==1 && Match->CanPlay())
	{
		Target=Match->Enemies[0]; Target->SetActorLocation(Ninja->GetActorLocation()+FVector(650,0,0)); View(Target->GetActorLocation());
		if (!Target->TryDefeat(ENinjaDefeatMethod::Shuriken,Ninja) || ANinjaExplosion::EnemyBursts!=EnemyBase+1 ||
			Target->TryDefeat(ENinjaDefeatMethod::Stomp,Ninja) || ANinjaExplosion::EnemyBursts!=EnemyBase+1) { Finish(TEXT("Enemy defeat did not emit exactly one explosion")); return; }
		Next(TEXT("Shuriken defeat emits one explosion; duplicate defeat is rejected"));
	}
	else if (Phase==2 && PhaseAge>.54f)
	{
		int32 Components=0;
		for (TActorIterator<ANinjaExplosion> It(GetWorld());It;++It)
		{
			if (!It->GetActorScale3D().Equals(FVector(3.f),.001f)) { Finish(TEXT("Explosion is not triple-sized")); return; }
			TArray<UStaticMeshComponent*> Meshes; It->GetComponents(Meshes); Components+=Meshes.Num();
			for (auto* Mesh:Meshes) if (Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision || Mesh->IsSimulatingPhysics()) { Finish(TEXT("Cosmetic explosion gained collision or physics")); return; }
		}
		if (Components!=69 || Ninja->Health!=3 || Match->RemainingEnemies()!=Match->Enemies.Num()-1) { Finish(TEXT("Explosion geometry or gameplay invariants failed")); return; }
		Capture(TEXT("enemy")); Next(TEXT("Large fireball, sparks, smoke and ring exist without collision or splash damage"));
	}
	else if (Phase==3)
	{
		if (!bLongLived && PhaseAge>1.6f)
		{
			int32 Alive=0; for (TActorIterator<ANinjaExplosion> It(GetWorld());It;++It) ++Alive;
			if (Alive!=1) { Finish(TEXT("Triple-duration effect vanished at the old lifetime")); return; }
			bLongLived=true; Cases.Add(TEXT("Triple-sized explosion remains alive beyond the original 1.45-second lifetime"));
		}
		if (PhaseAge<4.5f) return;
		for (TActorIterator<ANinjaExplosion> It(GetWorld());It;++It) { Finish(TEXT("Explosion outlived its lifetime")); return; }
		if (!Match->Enemies[1]->TryDefeat(ENinjaDefeatMethod::Stomp,Ninja) || ANinjaExplosion::EnemyBursts!=EnemyBase+2) { Finish(TEXT("Stomp did not explode")); return; }
		if (!Ninja->ReceiveContactDamage() || Ninja->Health!=2 || ANinjaExplosion::PlayerBursts!=PlayerBase) { Finish(TEXT("Nonlethal hit exploded the player")); return; }
		Next(TEXT("Expired effect is removed; stomp also explodes; a nonlethal hit does not"));
	}
	else if (Phase==4 && PhaseAge>1.1f)
	{
		Ninja->Health=1; View(Ninja->GetActorLocation());
		if (!Ninja->ReceiveContactDamage() || Ninja->Health!=0 || !Ninja->IsHidden() || ANinjaExplosion::PlayerBursts!=PlayerBase+1 || Ninja->ReceiveContactDamage()) { Finish(TEXT("Player knockout did not explode exactly once and hide the ninja")); return; }
		Next(TEXT("Lethal damage emits one player explosion and hides the defeated character"));
	}
	else if (Phase==5 && PhaseAge>.54f)
	{
		if (Match->State!=ENinjaMatchState::Lost || Match->ResultAge>=2.1f) { Finish(TEXT("Result did not resolve immediately with a visible explosion interval")); return; }
		Capture(TEXT("player")); Next(TEXT("Game loss resolves immediately while the result panel leaves the blast visible"));
	}
	else if (Phase==6 && PhaseAge>.15f)
	{
		Match->Confirm();
		for (TActorIterator<ANinjaExplosion> It(GetWorld());It;++It) { Finish(TEXT("Retry retained an explosion")); return; }
		if (Ninja->IsHidden() || Ninja->Health!=3) { Finish(TEXT("Retry left player hidden or dead")); return; }
		Next(TEXT("Immediate retry removes active explosions and restores player visibility and health"));
	}
	else if (Phase==7 && Match->State==ENinjaMatchState::Ready)
	{
		for (ARedNinja* E:Match->Enemies) E->SetActorTickEnabled(false);
		Match->Confirm(); Next(TEXT("Retry prepares the existing enemy roster"));
	}
	else if (Phase==8 && Match->CanPlay())
	{
		Ninja->Health=1; const FVector Feet=Ninja->GetActorLocation()-FVector(0,0,Ninja->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Ninja->RecoverToSurface(Feet,true);
		if (Ninja->Health!=0 || ANinjaExplosion::PlayerBursts!=PlayerBase+2) { Finish(TEXT("Fatal fall did not emit a player explosion")); return; }
		Next(TEXT("Fatal fall penalty also emits a knockout explosion")); Finish(FString());
	}
}
void ANinjaExplosionCheck::Finish(const FString& Error)
{
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed")); Report->SetStringField(TEXT("error"),Error);
	Report->SetNumberField(TEXT("enemy_bursts"),ANinjaExplosion::EnemyBursts-EnemyBase); Report->SetNumberField(TEXT("player_bursts"),ANinjaExplosion::PlayerBursts-PlayerBase);
	TArray<TSharedPtr<FJsonValue>> Rows; for (const auto& Case:Cases) Rows.Add(MakeShared<FJsonValueString>(Case)); Report->SetArrayField(TEXT("passed_cases"),Rows);
	FString Json,Path; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json)); FParse::Value(FCommandLine::Get(),TEXT("ExplosionReport="),Path); FFileHelper::SaveStringToFile(Json,*Path);
	UE_LOG(LogTemp,Display,TEXT("EXPLOSION_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error); bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
