#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaCameraCheck.generated.h"

/** Opt-in integration test. No effect on normal maps. */
UCLASS()
class IKEBUKURONINJACITY_API ANinjaCameraCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaCameraCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void Next(const FString& Case);
	void Finish(const FString& Error);
	void Key(FKey Input,bool bPressed);
	void Stop();
	UPROPERTY() TObjectPtr<class ANinjaCharacter> Ninja;
	UPROPERTY() TObjectPtr<class ANinjaMatchDirector> Match;
	UPROPERTY() TObjectPtr<class APlayerController> PC;
	TArray<FString> Cases;
	int32 Phase=0, FPS=60;
	float PhaseTime=0, SavedYaw=0, SavedPitch=0, PreviousYaw=0, PeakYawRate=0;
	FVector SegmentStart=FVector::ZeroVector;
	double Started=0, FinishedAt=0, PauseWall=0;
	bool bFinished=false;
};
