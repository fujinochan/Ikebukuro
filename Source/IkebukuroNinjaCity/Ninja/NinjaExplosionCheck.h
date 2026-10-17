#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaExplosionCheck.generated.h"

UCLASS()
class IKEBUKURONINJACITY_API ANinjaExplosionCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaExplosionCheck();
	virtual void Tick(float DeltaSeconds) override;
private:
	void Finish(const FString& Error);
	void Next(const FString& Case);
	void Capture(const FString& Suffix);
	void View(FVector Center);
	UPROPERTY() TObjectPtr<class ANinjaMatchDirector> Match;
	UPROPERTY() TObjectPtr<class ANinjaCharacter> Ninja;
	UPROPERTY() TObjectPtr<class ACameraActor> Camera;
	UPROPERTY() TObjectPtr<class ARedNinja> Target;
	TArray<FString> Cases;
	int32 Phase=0, EnemyBase=0, PlayerBase=0;
	float PhaseAge=0;
	double Started=0, FinishedAt=0;
	bool bFinished=false;
	bool bLongLived=false;
};
