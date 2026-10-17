#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaMatchDirector.generated.h"
class ANinjaCharacter;
class ARedNinja;
class ANinjaPatrolArea;

UENUM(BlueprintType)
enum class ENinjaMatchState : uint8 { Loading, Ready, Countdown, Playing, Won, Lost };

USTRUCT()
struct FNinjaMatchSpawn
{
	GENERATED_BODY()
	UPROPERTY() TSubclassOf<ARedNinja> Class;
	UPROPERTY() FTransform Transform;
	UPROPERTY() TObjectPtr<ANinjaPatrolArea> Area;
	UPROPERTY() TArray<FName> Tags;
	UPROPERTY() bool bAI=false;
};

/** Owns one local round. Practice maps without this actor retain their old rules. */
UCLASS()
class IKEBUKURONINJACITY_API ANinjaMatchDirector : public AActor
{
	GENERATED_BODY()
public:
	ANinjaMatchDirector();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	static ANinjaMatchDirector* Find(const UObject* Context);
	static bool AllowsGameplay(const UObject* Context);
	bool CanPlay() const;
	bool CanDefeat(ARedNinja* Enemy) const;
	void CreditDefeat(ARedNinja* Enemy);
	void Confirm();
	void TogglePause();
	void Retry();
	int32 RemainingEnemies() const { return Enemies.Num()-Credited.Num(); }
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") ENinjaMatchState State=ENinjaMatchState::Loading;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") float RemainingSeconds=300;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") float CountdownSeconds=3;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") FString ResultReason;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") int32 RoundNumber=1;
	UPROPERTY() TArray<TObjectPtr<ARedNinja>> Enemies;
private:
	bool IsPrepared() const;
	void Finish(bool bWon,const FString& Reason);
	bool bActive=true;
	float PreparedSeconds=0;
	FVector StartFoot;
	FRotator StartRotation;
	UPROPERTY() TObjectPtr<ANinjaCharacter> Ninja;
	UPROPERTY() TArray<FNinjaMatchSpawn> Spawns;
	TSet<TWeakObjectPtr<ARedNinja>> Credited;
};
