#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityTextureCheck.generated.h"

/** Opt-in visual/material inspection. Never changes the saved map at runtime. */
UCLASS()
class IKEBUKURONINJACITY_API ACityTextureCheck : public AActor
{
	GENERATED_BODY()
public:
	ACityTextureCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void SetView(int32 Index);
	void Finish(const FString& Error);
	UPROPERTY() TObjectPtr<class ACameraActor> Camera;
	UPROPERTY() TObjectPtr<class ACityPlayArea> Area;
	UPROPERTY() TObjectPtr<class ANinjaCharacter> Ninja;
	bool bBaseline=false,bFinished=false,bNextView=false;
	int32 ViewIndex=0,Frames=0,MaxMeshes=0,TextureCount=0;
	double Started=0,ViewStarted=0,FinishedAt=0,FrameSum=0,FrameMax=0;
	uint64 TextureBytes=0;
	TArray<FString> Captures;
};
