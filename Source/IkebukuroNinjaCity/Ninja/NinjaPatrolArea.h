#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaPatrolArea.generated.h"

/** Small walking graph built against saved WorldStatic collision, never visual tiles. */
UCLASS()
class IKEBUKURONINJACITY_API ANinjaPatrolArea : public AActor
{
	GENERATED_BODY()
public:
	ANinjaPatrolArea();
	bool BuildGraph();
	bool Sample(FVector Location,FVector& Foot) const;
	bool ClearSegment(FVector Start,FVector End) const;
	bool FindRoute(FVector Start,FVector End,TArray<FVector>& Route) const;
	int32 Nearest(FVector Point) const;
	FVector PatrolGoal(FVector Home,int32 Index) const;
	FVector2D GetWalkingHalfExtent() const { return HalfExtent*PatrolExtentScale; }
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Patrol") FVector2D HalfExtent=FVector2D(1800,600);
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Patrol",meta=(ClampMin="1")) float PatrolExtentScale=2;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Patrol") float PatrolTravelRadius=800;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Patrol") float GridSpacing=150;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Patrol") float HeightTolerance=100;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Patrol") bool bReady=false;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Patrol") int32 ValidNodes=0;
	TArray<FVector> Nodes;
private:
	bool RawSample(FVector Location,FVector& Foot) const;
	TArray<TArray<int32>> Edges;
};
