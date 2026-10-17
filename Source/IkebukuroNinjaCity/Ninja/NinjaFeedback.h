#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaFeedback.generated.h"

/** Tiny, collision-free radial burst; short local sounds require no external assets. */
UCLASS()
class IKEBUKURONINJACITY_API ANinjaFeedback : public AActor
{
	GENERATED_BODY()
public:
	ANinjaFeedback();
	virtual void Tick(float DeltaSeconds) override;
	static void Emit(const UObject* Context,FVector Location,const FString& Cue);
	static int32 EmittedCount;
private:
	UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Rays;
	float Age=0;
};
