#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CityGroundColorTools.generated.h"
class ACesiumGeoreference;

/** Offline editor processing of the approved local photo tiles. */
UCLASS()
class IKEBUKURONINJACITY_API UCityGroundColorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="City|Editor")
	static bool GenerateGroundColors(const FString& ManifestRelativePath, ACesiumGeoreference* Georeference);
};
