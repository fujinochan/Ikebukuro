#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityGroundColorCheck.generated.h"

UCLASS()
class IKEBUKURONINJACITY_API ACityGroundColorCheck : public AActor
{
	GENERATED_BODY()
public:
	ACityGroundColorCheck();
	virtual void Tick(float DeltaSeconds) override;
private:
	void Finish(const FString& Error);
	void Capture(const TCHAR* Name);
	UPROPERTY() TObjectPtr<class ANinjaMatchDirector> Match;
	UPROPERTY() TObjectPtr<class ACameraActor> Camera;
	double Started=0,PhaseAt=0;
	int32 Phase=0;
};
