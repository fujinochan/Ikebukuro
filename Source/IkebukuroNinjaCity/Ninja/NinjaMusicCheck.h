#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaMusicCheck.generated.h"

/** Opt-in integration check of the real M4A decoder, audio mixer and round lifecycle. */
UCLASS()
class IKEBUKURONINJACITY_API ANinjaMusicCheck : public AActor
{
	GENERATED_BODY()
public:
	ANinjaMusicCheck();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
private:
	void Finish(const FString& Error);
	void HoldEnemies();
	void Next(const FString& Case);
	UFUNCTION() void DefeatAtSongEnd();
	UPROPERTY() TObjectPtr<class ANinjaMatchDirector> Match;
	UPROPERTY() TObjectPtr<class ANinjaCharacter> Ninja;
	int32 Phase=0;
	double Started=0,PhaseWall=0,PausedPosition=0,FinishedAt=0,FullTrackStarted=0;
	float PausedRemaining=0,PeakEnvelope=0;
	double CompletedTrackWallSeconds=0;
	bool bFinished=false,bCountdownChecked=false,bFullTrack=false,bEndCaptured=false,bDeadlineTieDefeated=false;
	TArray<FString> Cases;
};
