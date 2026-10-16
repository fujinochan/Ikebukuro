#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaCombatCheck.generated.h"
class ANinjaCharacter;
class ARedNinja;
class ANinjaShuriken;
UCLASS()
class IKEBUKURONINJACITY_API ANinjaCombatCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaCombatCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void StartCase();
	void TickCity();
	void AimAt(FVector Target);
	void Pass(const FString& Name);
	void Finish(const FString& Error);
	void Place(FVector Position,FVector Velocity=FVector::ZeroVector);
	ARedNinja* EnemyAt(FVector Position);
	ANinjaShuriken* StarAt(FVector Position,float Speed);
	void Key(FKey Key,bool Pressed);
	UPROPERTY() TObjectPtr<ANinjaCharacter> Ninja;
	UPROPERTY() TObjectPtr<ARedNinja> Enemy;
	UPROPERTY() TObjectPtr<ARedNinja> Rear;
	UPROPERTY() TObjectPtr<ANinjaShuriken> Star;
	UPROPERTY() TArray<TObjectPtr<AActor>> Temporary;
	TArray<FString> Results;
	int32 Case=0,Phase=0,FPS=60,CountBefore=0,ShotsBefore=0,HealthBefore=0;
	double StartTime=0,PhaseTime=0,RealStart=0,FinishedAt=0;
	bool bFinished=false,bSawBounce=false;
	bool bCityMode=false;
	double CityGroundError=0;
};
