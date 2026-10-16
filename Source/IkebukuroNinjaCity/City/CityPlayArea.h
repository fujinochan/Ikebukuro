#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityPlayArea.generated.h"

class ANinjaCharacter;

/** Fixed play bounds, safe recovery, and an opt-in city integration check. */
UCLASS()
class IKEBUKURONINJACITY_API ACityPlayArea : public AActor
{
	GENERATED_BODY()
public:
	ACityPlayArea();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City")
	FVector2D PlayMinimum = FVector2D(-8000,-23000);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City")
	FVector2D PlayMaximum = FVector2D(32000,17000);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City")
	FVector SafeStreet = FVector(12000,-5000,900);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City")
	FVector StreetRunEnd = FVector(15000,-5000,900);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City")
	TArray<FVector> RoofTargets;
private:
	void PlaceAt(FVector Surface, bool bRecovery);
	void Key(const FKey& Input, bool bPressed);
	void Finish(const FString& Error);
	TArray<double> ProbeGround() const;
	UPROPERTY()
	TObjectPtr<ANinjaCharacter> Ninja;
	bool bPreviousR = false;
	bool bCheck = false;
	bool bSpaceDown = false;
	bool bFinished = false;
	int32 RecoveryCount = 0;
	int32 Phase = 0;
	int32 MaxJumpStage = 0;
	int32 RoofIndex = 0;
	int32 CameraIndex = 0;
	int32 CameraObstructions = 0;
	int32 ExpectedRecovery = 0;
	float RunDistance = 0;
	bool bStableCollision = false;
	double VisualCollisionErrorCm = -1;
	double PhaseTime = 0;
	double PressTime = 0;
	double RealStart = 0;
	double FinishedAt = 0;
	TArray<double> GroundHeights;
};
