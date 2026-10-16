#include "LocalCityTileset.h"
#include "CesiumGeoreference.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ALocalCityTileset::ALocalCityTileset()
{
	SetTilesetSource(ETilesetSource::FromUrl);
	MaximumScreenSpaceError = 32;
	// This small proof prioritizes retaining physics behind the camera.
	EnableFrustumCulling = false;
	EnableFogCulling = false;
	BodyInstance.SetCollisionProfileName(TEXT("BlockAll"));
}

void ALocalCityTileset::ConfigureLocalSource()
{
	const FString ContentRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	ResolvedDatasetPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ContentRoot, DatasetRelativePath));
	FPaths::NormalizeFilename(ResolvedDatasetPath);
	if (!FPaths::IsUnderDirectory(ResolvedDatasetPath, ContentRoot))
	{
		UE_LOG(LogTemp, Error, TEXT("LocalCity: dataset path escapes Content"));
		return;
	}
	SetTilesetSource(ETilesetSource::FromUrl);
	SetUrl(TEXT("file:///") + ResolvedDatasetPath.Replace(TEXT(" "), TEXT("%20")));
	SetCreatePhysicsMeshes(bCreateTilePhysics);
}

void ALocalCityTileset::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ConfigureLocalSource();
}

void ALocalCityTileset::BeginPlay()
{
	ConfigureLocalSource();
	Super::BeginPlay();
	UE_LOG(LogTemp, Display, TEXT("LocalCity: source=%s exists=%d"), *ResolvedDatasetPath,
		IFileManager::Get().FileExists(*ResolvedDatasetPath));
	bSmokeTest = FParse::Param(FCommandLine::Get(), TEXT("CitySmokeTest"));
	SmokeStart = FPlatformTime::Seconds();
	if (bSmokeTest)
	{
		const FVector Location(25000, 35000, 45000);
		SmokeCamera = GetWorld()->SpawnActor<ACameraActor>(Location, (-Location).Rotation());
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetViewTarget(SmokeCamera);
		}
	}
}

int32 ALocalCityTileset::CountRoofHits() const
{
	int32 Hits = 0;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CityRoofCheck), true);
	for (int32 X = -3; X <= 3; ++X)
	{
		for (int32 Y = -3; Y <= 3; ++Y)
		{
			FHitResult Hit;
			const FVector Start(X * 4000., Y * 4000., 40000.);
			const FVector End(X * 4000., Y * 4000., -10000.);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.GetActor() == this)
			{
				++Hits;
			}
		}
	}
	return Hits;
}

void ALocalCityTileset::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bSmokeTest) return;
	const double Now = FPlatformTime::Seconds();
	if (bReported)
	{
		if (Now - ReportedAt > 3) FPlatformMisc::RequestExit(false);
		return;
	}
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents(Meshes);
	LastMeshCount = 0;
	for (const UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetStaticMesh() && Mesh->IsVisible()) ++LastMeshCount;
	}
	if (!bChangedView && Now - SmokeStart > 12 && LastMeshCount > 0 && GetLoadProgress() >= 99)
	{
		FirstRoofHits = CountRoofHits();
		if (FirstRoofHits > 0)
		{
			TestCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);
			for (int32 X = -3; X <= 3 && TestCharacter; ++X)
			{
				FHitResult Hit;
				const FVector Start(X * 4000., 0, 40000.);
				if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0, 0, 50000), ECC_Visibility,
					FCollisionQueryParams(SCENE_QUERY_STAT(CityLandingCheck), true)) && Hit.GetActor() == this)
				{
					TestRoofPoint = Hit.ImpactPoint;
					TestCharacter->SetActorLocation(TestRoofPoint + FVector(0, 0, 300));
					TestCharacter->GetCharacterMovement()->StopMovementImmediately();
					TestCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
					break;
				}
			}
			SmokeCamera->SetActorRotation(SmokeCamera->GetActorRotation() + FRotator(0, 180, 0));
			bChangedView = true;
			ViewChangedAt = Now;
		}
	}
	if (bChangedView && Now - ViewChangedAt > 6)
	{
		WriteSmokeReport(CountRoofHits() >= FirstRoofHits && LastMeshCount > 0);
	}
	else if (Now - SmokeStart > 120)
	{
		WriteSmokeReport(false);
	}
}

void ALocalCityTileset::WriteSmokeReport(bool bPassed)
{
	const bool bLandedOnRoof = TestCharacter && !TestCharacter->GetCharacterMovement()->IsFalling() &&
		FMath::Abs(TestCharacter->GetActorLocation().Z - TestCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			- TestRoofPoint.Z) < 10;
	bPassed = bPassed && bLandedOnRoof;
	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), bPassed ? TEXT("passed") : TEXT("failed"));
	Result->SetStringField(TEXT("dataset"), ResolvedDatasetPath);
	Result->SetBoolField(TEXT("dataset_exists"), IFileManager::Get().FileExists(*ResolvedDatasetPath));
	Result->SetNumberField(TEXT("visible_mesh_components"), LastMeshCount);
	Result->SetNumberField(TEXT("load_progress"), GetLoadProgress());
	Result->SetNumberField(TEXT("roof_hits_before_camera_turn"), FirstRoofHits);
	Result->SetNumberField(TEXT("roof_hits_after_camera_turn"), CountRoofHits());
	Result->SetBoolField(TEXT("camera_turned_180_degrees"), bChangedView);
	Result->SetBoolField(TEXT("frustum_culling_enabled"), EnableFrustumCulling);
	Result->SetBoolField(TEXT("character_landed_on_city_roof"), bLandedOnRoof);
	if (ACesiumGeoreference* Geo = GetGeoreference().Get())
	{
		const FVector Origin = Geo->GetOriginLongitudeLatitudeHeight();
		const FVector Base = Geo->TransformLongitudeLatitudeHeightPositionToUnreal(Origin);
		const FVector OneMetreUp = Geo->TransformLongitudeLatitudeHeightPositionToUnreal(Origin + FVector(0, 0, 1));
		Result->SetNumberField(TEXT("one_metre_in_unreal_cm"), (OneMetreUp - Base).Length());
		Result->SetNumberField(TEXT("origin_longitude"), Origin.X);
		Result->SetNumberField(TEXT("origin_latitude"), Origin.Y);
		Result->SetNumberField(TEXT("origin_height_m"), Origin.Z);
	}
	FString Path;
	if (!FParse::Value(FCommandLine::Get(), TEXT("CitySmokeReport="), Path))
		Path = FPaths::ProjectSavedDir() / TEXT("Validation/step2-city-runtime.json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FString Json;
	const auto Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Result, Writer);
	FFileHelper::SaveStringToFile(Json, *Path);
	if (SmokeCamera)
	{
		const FVector Location = SmokeCamera->GetActorLocation();
		SmokeCamera->SetActorRotation((-Location).Rotation());
	}
	FString Screenshot;
	if (FParse::Value(FCommandLine::Get(), TEXT("CitySmokeScreenshot="), Screenshot))
		FScreenshotRequest::RequestScreenshot(Screenshot, false, false);
	UE_LOG(LogTemp, Display, TEXT("CITY_SMOKE_%s report=%s"), bPassed ? TEXT("PASSED") : TEXT("FAILED"), *Path);
	bReported = true;
	ReportedAt = FPlatformTime::Seconds();
}
