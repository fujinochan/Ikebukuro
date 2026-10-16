#include "CityTextureCheck.h"
#include "CityPlayArea.h"
#include "LocalCityTileset.h"
#include "Ninja/NinjaCharacter.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"

ACityTextureCheck::ACityTextureCheck() { PrimaryActorTick.bCanEverTick=true; }
void ACityTextureCheck::BeginPlay()
{
	Super::BeginPlay(); SetActorTickEnabled(FParse::Param(FCommandLine::Get(),TEXT("CityTextureCheck")));
	if (!IsActorTickEnabled()) return;
	bBaseline=FParse::Param(FCommandLine::Get(),TEXT("TextureBaseline")); Started=ViewStarted=FPlatformTime::Seconds();
	for (TActorIterator<ACityPlayArea> It(GetWorld());It;++It) Area=*It;
	Ninja=Cast<ANinjaCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
	Camera=GetWorld()->SpawnActor<ACameraActor>();
}
void ACityTextureCheck::SetView(int32 Index)
{
	auto* PC=UGameplayStatics::GetPlayerController(this,0);
	if (Index==1)
	{
		const FVector Center((Area->PlayMinimum.X+Area->PlayMaximum.X)/2,(Area->PlayMinimum.Y+Area->PlayMaximum.Y)/2,1000);
		const FVector View=Center+FVector(23000,28000,35000);
		Camera->SetActorLocationAndRotation(View,(Center-View).Rotation()); PC->SetViewTarget(Camera);
	}
	else
	{
		FVector Foot=Area->SafeStreet;
		if (Index==2 && !Area->RoofTargets.IsEmpty()) Foot=Area->RoofTargets[0];
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit,Foot+FVector(0,0,10000),Foot-FVector(0,0,10000),FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(TextureViewFoot),true))) Foot=Hit.ImpactPoint;
		Ninja->SetActorLocation(Foot+FVector(0,0,110)); Ninja->GetCharacterMovement()->StopMovementImmediately();
		Ninja->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		PC->SetViewTarget(Ninja); PC->SetControlRotation(Index==0?FRotator(-5,0,0):FRotator(-10,60,0));
	}
	ViewStarted=FPlatformTime::Seconds();
}
void ACityTextureCheck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); const double Now=FPlatformTime::Seconds();
	if (bFinished) { if (Now-FinishedAt>3) FPlatformMisc::RequestExit(false); return; }
	if (!Area || !Ninja) { Finish(TEXT("Play area or player missing")); return; }
	if (Now-Started>120) { Finish(TEXT("Texture loading timed out")); return; }
	if (Frames==0) SetView(0);
	if (bNextView) { SetView(ViewIndex); bNextView=false; }
	++Frames; FrameSum+=DeltaSeconds*1000; FrameMax=FMath::Max(FrameMax,double(DeltaSeconds*1000));
	bool Ready=true; int32 Meshes=0; TSet<UTexture*> Textures;
	for (TActorIterator<ALocalCityTileset> It(GetWorld());It;++It)
	{
		Ready &= It->GetLoadProgress()>=99;
		if (!It->DatasetRelativePath.Contains(TEXT("Buildings"))) continue;
		TArray<UStaticMeshComponent*> Components; It->GetComponents(Components);
		for (auto* Component:Components) if (Component->IsVisible() && Component->GetStaticMesh())
		{
			++Meshes;
		}
	}
	// Cesium v2.30 creates these transient textures when decoding local tiles.
	// Count actual image objects rather than relying on parent material parameter
	// enumeration, which does not expose the runtime bindings in this UE version.
	for (TObjectIterator<UTexture2D> It; It; ++It)
		if (It->GetName().StartsWith(TEXT("CesiumRuntimeTexture")) && !It->HasAnyFlags(RF_ClassDefaultObject)) Textures.Add(*It);
	MaxMeshes=FMath::Max(MaxMeshes,Meshes); TextureCount=FMath::Max(TextureCount,Textures.Num());
	uint64 Bytes=0; for (auto* Texture:Textures) Bytes+=Texture->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
	TextureBytes=FMath::Max(TextureBytes,Bytes);
	if (Frames%300==0) UE_LOG(LogTemp,Display,TEXT("TEXTURE_PROGRESS view=%d ready=%d meshes=%d textures=%d"),ViewIndex,Ready,Meshes,Textures.Num());
	if (Now-ViewStarted<6 || !Ready || Meshes==0) return;
	FString Prefix;
	if (FParse::Value(FCommandLine::Get(),TEXT("TextureScreenshot="),Prefix))
	{
		const FString Suffix=ViewIndex==0?TEXT("_street.png"):ViewIndex==1?TEXT("_overview.png"):TEXT("_roof.png");
		const FString Path=FPaths::GetPath(Prefix)/FPaths::GetBaseFilename(Prefix)+Suffix;
		Captures.Add(Path); FScreenshotRequest::RequestScreenshot(Path,false,false);
	}
	if (++ViewIndex<3) bNextView=true; else Finish(!bBaseline && TextureCount==0?TEXT("No Cesium runtime image textures loaded"):FString());
}
void ACityTextureCheck::Finish(const FString& Error)
{
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),Error.IsEmpty()?TEXT("passed"):TEXT("failed"));
	Report->SetStringField(TEXT("error"),Error); Report->SetBoolField(TEXT("baseline_lod1"),bBaseline);
	Report->SetNumberField(TEXT("max_loaded_building_meshes"),MaxMeshes); Report->SetNumberField(TEXT("loaded_cesium_runtime_textures"),TextureCount);
	Report->SetNumberField(TEXT("texture_object_reported_bytes"),double(TextureBytes));
	Report->SetStringField(TEXT("texture_memory_scope"),TEXT("UObject resource accounting only; excludes Cesium custom GPU image allocations. This is not VRAM usage."));
	Report->SetNumberField(TEXT("process_physical_memory_bytes"),double(FPlatformMemory::GetStats().UsedPhysical));
	Report->SetNumberField(TEXT("sampled_frame_ms_average"),Frames?FrameSum/Frames:0); Report->SetNumberField(TEXT("sampled_frame_ms_max"),FrameMax);
	Report->SetStringField(TEXT("performance_scope"),TEXT("Editor game helper, 1280x720, capped 60fps, includes streaming; not a 1080p performance acceptance test"));
	TArray<TSharedPtr<FJsonValue>> Images; for (const FString& Path:Captures) Images.Add(MakeShared<FJsonValueString>(Path)); Report->SetArrayField(TEXT("screenshots"),Images);
	TArray<TSharedPtr<FJsonValue>> Roofs;
	if (Area) for (FVector Point:Area->RoofTargets)
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit,Point+FVector(0,0,20000),Point-FVector(0,0,20000),FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(TextureRoofProbe),true)))
		{
			auto Item=MakeShared<FJsonObject>(); Item->SetNumberField(TEXT("x"),Hit.ImpactPoint.X); Item->SetNumberField(TEXT("y"),Hit.ImpactPoint.Y); Item->SetNumberField(TEXT("z"),Hit.ImpactPoint.Z); Roofs.Add(MakeShared<FJsonValueObject>(Item));
		}
	}
	Report->SetArrayField(TEXT("roof_surfaces"),Roofs);
	FString Json,Path; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
	FParse::Value(FCommandLine::Get(),TEXT("TextureReport="),Path); FFileHelper::SaveStringToFile(Json,*Path);
	UE_LOG(LogTemp,Display,TEXT("CITY_TEXTURE_%s %s"),Error.IsEmpty()?TEXT("PASSED"):TEXT("FAILED"),*Error);
	bFinished=true; FinishedAt=FPlatformTime::Seconds();
}
