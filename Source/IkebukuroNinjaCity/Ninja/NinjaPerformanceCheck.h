#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaPerformanceCheck.generated.h"
UCLASS()
class IKEBUKURONINJACITY_API ANinjaPerformanceCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaPerformanceCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void EnterScene();
	void RecordScene();
	void Finish(const FString& Error);
	UPROPERTY() TObjectPtr<class ANinjaMatchDirector> Match;
	UPROPERTY() TObjectPtr<class ANinjaCharacter> Ninja;
	UPROPERTY() TObjectPtr<class APlayerController> PC;
	TArray<TSharedPtr<class FJsonValue>> Scenes;
	TArray<double> Frames, GPU, Render;
	int32 Scene=-1;
	double Started=0, SceneStarted=0, Previous=0, BlastAt=-10, FinishedAt=0;
	FVector StartPoint=FVector::ZeroVector;
	bool bFinished=false;
	bool bAdvanceScene=false;
};
