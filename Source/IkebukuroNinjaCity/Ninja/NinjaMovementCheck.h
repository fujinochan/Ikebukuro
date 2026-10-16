#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaMovementCheck.generated.h"

class ANinjaCharacter;

/** Only runs with -NinjaMovementCheck. Drives mapped keys through PlayerController. */
UCLASS()
class IKEBUKURONINJACITY_API ANinjaMovementCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaMovementCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void Key(const FKey& Key, bool bPressed);
	void Space();
	void PrepareCase();
	void Finish(const FString& Error = FString());
	UPROPERTY()
	TObjectPtr<ANinjaCharacter> Ninja;
	int32 Phase = 0;
	int32 Case = 0;
	int32 MaxStage = 0;
	int32 FPS = 60;
	double PhaseStart = 0;
	double RealStart = 0;
	double PressTime = 0;
	double ThirdTime = 0;
	double DoneAt = 0;
	float BaseZ = 0;
	float PeakZ = 0;
	float MaxX = 0;
	float WalkMeasured = 0;
	float RunMeasured = 0;
	bool bSpaceDown = false;
	bool bBadPressSent = false;
	bool bRejected = false;
	bool bScreenshot = false;
	TArray<TSharedPtr<class FJsonValue>> Results;
};
