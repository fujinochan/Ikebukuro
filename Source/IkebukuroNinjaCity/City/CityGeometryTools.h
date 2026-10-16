#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CityGeometryTools.generated.h"

class UStaticMesh;
class ACesiumGeoreference;

/** Editor baking of local, self-contained city geometry; no network access. */
UCLASS()
class IKEBUKURONINJACITY_API UCityGeometryTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="City|Editor")
	static UStaticMesh* BakeLayer(const FString& DatasetRelativePath, const FString& AssetPath,
		ACesiumGeoreference* Georeference, FVector2D Minimum, FVector2D Maximum);
	UFUNCTION(BlueprintCallable, Category="City|Editor")
	static UStaticMesh* BakeLayerWithoutFeatures(const FString& DatasetRelativePath, const FString& AssetPath,
		ACesiumGeoreference* Georeference, FVector2D Minimum, FVector2D Maximum, const TArray<FString>& ExcludedFeatureIds);
	UFUNCTION(BlueprintCallable, Category="City|Editor")
	static UStaticMesh* BuildSurfaceMesh(const FString& AssetPath, const TArray<FVector>& Vertices, const TArray<int32>& Indices);
	UFUNCTION(BlueprintCallable, Category="City|Editor")
	static FVector SurfacePoint(UObject* WorldContext, FVector2D Location);
	UFUNCTION(BlueprintCallable, Category="City|Editor")
	static bool IsRouteClear(UObject* WorldContext, FVector Start, FVector End);
	/** Query saved mesh triangles directly while editor physics is not initialized. */
	UFUNCTION(BlueprintCallable, Category="City|Editor")
	static FVector SurfaceOnMesh(UStaticMesh* Mesh, FVector2D Location);
};
