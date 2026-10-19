#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaAppearanceCheck.generated.h"

/** Opt-in costume contact sheet using the actual animated game characters. */
UCLASS()
class IKEBUKURONINJACITY_API ANinjaAppearanceCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaAppearanceCheck();
	virtual void Tick(float DeltaSeconds) override;
private:
	void Capture(const TCHAR* View);
	void Finish(const FString& Error);
	UPROPERTY() TObjectPtr<class ANinjaCharacter> Ninja;
	UPROPERTY() TObjectPtr<class ARedNinja> Enemy;
	UPROPERTY() TObjectPtr<class ACameraActor> Camera;
	int32 Phase=0;
	double Started=0, PhaseAt=0;
};
