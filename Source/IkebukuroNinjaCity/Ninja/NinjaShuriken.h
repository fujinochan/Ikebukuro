#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaShuriken.generated.h"
class USphereComponent;
class UProjectileMovementComponent;
class ANinjaCharacter;
UCLASS()
class IKEBUKURONINJACITY_API ANinjaShuriken : public AActor
{
	GENERATED_BODY()
public:
	ANinjaShuriken();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void Launch(ANinjaCharacter* Ninja,FVector Direction,float Speed=5000);
	void LaunchEnemy(class ARedNinja* Enemy,FVector Direction,float Speed=2500);
	bool IsHostile() const { return bHostile; }
	UFUNCTION() void Impact(const FHitResult& Hit);
	UFUNCTION() void HostileOverlap(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex,bool bFromSweep,const FHitResult& Hit);
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Blades;
	UPROPERTY(EditDefaultsOnly,Category="Shuriken") float MaximumRange=6000;
private:
	TWeakObjectPtr<ANinjaCharacter> Shooter;
	FVector Origin;
	bool bResolved=false;
	bool bHostile=false;
};
