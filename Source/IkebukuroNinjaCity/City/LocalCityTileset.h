#pragma once

#include "CoreMinimal.h"
#include "Cesium3DTileset.h"
// A Windows API macro exposed by the native dependency conflicts with UE's
// TScriptInterface::GetObject in generated unity translation units.
#if PLATFORM_WINDOWS && defined(GetObject)
#undef GetObject
#endif
// Native dependency Windows macros must not leak into subsequent unity sources
// (RHI uses std::numeric_limits::max in the Shipping translation unit).
#if PLATFORM_WINDOWS && defined(max)
#undef max
#endif
#if PLATFORM_WINDOWS && defined(min)
#undef min
#endif
#if PLATFORM_WINDOWS && defined(DrawText)
#undef DrawText
#endif
#include "LocalCityTileset.generated.h"

/** Local-only city source with a portable path and an opt-in runtime smoke check. */
UCLASS()
class IKEBUKURONINJACITY_API ALocalCityTileset : public ACesium3DTileset
{
	GENERATED_BODY()

public:
	ALocalCityTileset();
	virtual void PostLoad() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Local City")
	FString DatasetRelativePath = TEXT("CityData/ToshimaLOD1/tileset.json");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Local City")
	bool bCreateTilePhysics = true;

	UFUNCTION(CallInEditor, BlueprintCallable, Category="Local City")
	void ConfigureLocalSource();

private:
	int32 CountRoofHits() const;
	void WriteSmokeReport(bool bPassed);
	bool bSmokeTest = false;
	bool bChangedView = false;
	bool bReported = false;
	double SmokeStart = 0;
	double ViewChangedAt = 0;
	double ReportedAt = 0;
	int32 FirstRoofHits = 0;
	int32 LastMeshCount = 0;
	FVector TestRoofPoint = FVector::ZeroVector;
	UPROPERTY()
	TObjectPtr<class ACharacter> TestCharacter;
	FString ResolvedDatasetPath;
	UPROPERTY()
	TObjectPtr<class ACameraActor> SmokeCamera;
};
