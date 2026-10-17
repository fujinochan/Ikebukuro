#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaMatchCheck.generated.h"
UCLASS()
class IKEBUKURONINJACITY_API ANinjaMatchCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaMatchCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void Key(const FKey& Key,bool Pressed);
	void Next(const FString& Result);
	void Capture(const FString& Name);
	void Finish(const FString& Error);
	bool CheckReset();
	void HoldEnemies();
	void Place(FVector Center);
	UPROPERTY() TObjectPtr<class ANinjaMatchDirector> Match;
	UPROPERTY() TObjectPtr<class ANinjaCharacter> Ninja;
	UPROPERTY() TObjectPtr<class ANinjaShuriken> Star;
	UPROPERTY() TObjectPtr<class ARedNinja> LastEnemy;
	UPROPERTY() TObjectPtr<class ARedNinja> GroundA;
	UPROPERTY() TObjectPtr<class ARedNinja> GroundB;
	TArray<FVector> PausedEnemies;
	FVector PausedStar,ReadyPosition;
	float PausedTime=0;
	int32 Phase=0,FPS=60,Retries=0,ExpectedEnemies=0;
	double PhaseTime=0,Started=0,FinishedAt=0,PauseWall=0,ReleaseAt=0;
	bool bFinished=false,bReleaseEnter=false,bReleaseP=false,bReleaseF=false,bCapturedPlay=false;
	TArray<FString> Results;
};
