#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaEncounterCheck.generated.h"
class ANinjaCharacter;
class ARedNinja;
class ANinjaPatrolArea;
UCLASS()
class IKEBUKURONINJACITY_API ANinjaEncounterCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaEncounterCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void PlacePlayer(FVector Center);
	void Hold(ARedNinja* Enemy,bool Enabled);
	void Next(const FString& Result);
	void Finish(const FString& Error);
	UPROPERTY() TObjectPtr<ANinjaCharacter> Ninja;
	UPROPERTY() TObjectPtr<ARedNinja> GroundA;
	UPROPERTY() TObjectPtr<ARedNinja> GroundB;
	UPROPERTY() TObjectPtr<ARedNinja> Roof;
	UPROPERTY() TObjectPtr<ANinjaPatrolArea> GroundDomain;
	UPROPERTY() TObjectPtr<ANinjaPatrolArea> RoofDomain;
	UPROPERTY() TSubclassOf<ARedNinja> EnemyClass;
	int32 Phase=0,FPS=60;
	bool bCity=false,bFinished=false,bStarted=false,bBounce=false;
	double PhaseTime=0,RealStart=0,FinishedAt=0;
	FVector StartGround,StartRoof,FarPlayer,RoofGoal;
	FVector PreviousGround,PreviousB,PreviousRoof,GroundHome,RoofHome;
	double GroundDistance=0,GroundBDistance=0,RoofDistance=0,PressTime=0;
	int32 ForcedRoofRecoveries=0,MaxJumpStage=0,OrdinaryRecoveries=0;
	bool bSpaceHeld=false,bOverview=false,bOverviewSaved=false;
	TArray<FString> Results;
};
