#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaExplosion.generated.h"

/** Cosmetic only: no collision, damage, force, or dependency on a defeated actor. */
UCLASS()
class IKEBUKURONINJACITY_API ANinjaExplosion : public AActor
{
	GENERATED_BODY()
public:
	ANinjaExplosion();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	static void Emit(const UObject* Context, FVector Location, bool bPlayer=false);
	static void Clear(UWorld* World);
	static int32 EnemyBursts;
	static int32 PlayerBursts;
	float GetAge() const { return Age; }
private:
	UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Pieces;
	UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> Materials;
	float Age=0;
};
