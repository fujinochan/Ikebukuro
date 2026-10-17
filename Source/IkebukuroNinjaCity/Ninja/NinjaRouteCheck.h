#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaRouteCheck.generated.h"
UCLASS()
class IKEBUKURONINJACITY_API ANinjaRouteCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaRouteCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void Finish(const FString& Error);
	void PlaceAtEntry();
	UPROPERTY() TObjectPtr<class ANinjaMatchDirector> Match;
	UPROPERTY() TObjectPtr<class ANinjaCharacter> Ninja;
	UPROPERTY() TArray<TObjectPtr<class ARedNinja>> Roofs;
	TArray<FVector> InitialPositions;
	TArray<double> PatrolDistances;
	TArray<FString> Cases;
	FVector ShortcutTarget;
	int32 Phase=0,RoofIndex=0,StepIndex=1,Landings=0,FPS=60;
	bool bJumped=false,bFinished=false,bTripleReached=false;
	double Started=0,PhaseTime=0,StepTime=0,FinishedAt=0;
	double StepBudget=8;
};
