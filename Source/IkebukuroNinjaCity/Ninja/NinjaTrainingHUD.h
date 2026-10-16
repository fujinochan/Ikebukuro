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
};
