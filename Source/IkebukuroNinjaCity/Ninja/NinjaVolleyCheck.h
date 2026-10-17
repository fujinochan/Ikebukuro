#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaVolleyCheck.generated.h"
UCLASS()
class IKEBUKURONINJACITY_API ANinjaVolleyCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaVolleyCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void Next(const FString& Case);
	void Finish(const FString& Error);
	void ClearStars();
	void PlacePlayer(float Distance);
	void MakeWall(float Distance);
	void Capture(const FString& Name);
	class ANinjaShuriken* FireTestStar();
	UPROPERTY() TObjectPtr<class ANinjaMatchDirector> Match;
	UPROPERTY() TObjectPtr<class ANinjaCharacter> Ninja;
	UPROPERTY() TObjectPtr<class ARedNinja> Enemy;
	UPROPERTY() TObjectPtr<class ARedNinja> Teammate;
	UPROPERTY() TObjectPtr<class ANinjaShuriken> Star;
	UPROPERTY() TObjectPtr<class AStaticMeshActor> Wall;
	UPROPERTY() TObjectPtr<class ACameraActor> Camera;
	TArray<FString> Cases;
	int32 Phase=0, FPS=60, VolleyBase=0, PlayerBurstBase=0;
	float PhaseTime=0;
	double Started=0, FinishedAt=0, FirstVolley=0, PauseWall=0;
	FVector PausedPosition=FVector::ZeroVector;
	bool bFinished=false;
};
