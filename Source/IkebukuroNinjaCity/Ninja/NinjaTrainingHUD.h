#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NinjaTrainingHUD.generated.h"

UCLASS()
class IKEBUKURONINJACITY_API ANinjaTrainingHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
	int32 LastMarkerCount=0;
	int32 LastOffscreenMarkerCount=0;
private:
	void DrawMatchHUD(class ANinjaMatchDirector* Match);
};
