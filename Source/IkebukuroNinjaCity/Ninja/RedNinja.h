#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RedNinja.generated.h"
class ANinjaCharacter;
class ANinjaPatrolArea;
UENUM(BlueprintType)
enum class ENinjaBrainState : uint8 { Waiting, Patrol, Chase, Attack, Return, Blocked, Defeated };
UENUM(BlueprintType)
enum class ENinjaDefeatMethod : uint8 { Stomp, Shuriken };
UCLASS()
class IKEBUKURONINJACITY_API ARedNinja : public ACharacter
{
	GENERATED_BODY()
public:
	ARedNinja();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	bool TryDefeat(ENinjaDefeatMethod Method, ANinjaCharacter* Ninja);
	bool TryFireVolley(ANinjaCharacter* Player);
	bool IsReadyForMatch() const { return !bAIEnabled || bInitialized; }
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja") bool bDefeated=false;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja") ENinjaDefeatMethod DefeatMethod=ENinjaDefeatMethod::Stomp;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|AI") TObjectPtr<ANinjaPatrolArea> PatrolArea;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|AI") bool bAIEnabled=false;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|AI") float DetectionRadius=1000;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|AI") float LeashRadius=1800;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|Ranged") bool bRangedAttackEnabled=true;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|Ranged") float RangedAttackRadius=10000;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|Ranged") float VolleyInterval=2;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|Ranged") float ProjectileSpeed=2500;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Ranged") int32 VolleysFired=0;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Ranged") double LastVolleyTime=-1;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|AI") ENinjaBrainState BrainState=ENinjaBrainState::Waiting;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|AI") int32 SafetyRecoveries=0;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|AI") int32 SuccessfulAttacks=0;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|Route") int32 RouteOrder=0;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|Route") FString RouteHint;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja|Route") TArray<FVector> AssistRoute;
	FVector HomeFoot;
	int32 VisitedStates=0;
private:
	void SetState(ENinjaBrainState State);
	bool CanSee(ANinjaCharacter* Player) const;
	void ReturnSafely();
	bool bInitialized=false,bReturning=false;
	int32 PatrolIndex=0,RouteIndex=0;
	double NextRouteTime=0,NextAttackTime=0,ReacquireTime=0;
	double NextVolleyTime=0;
	TArray<FVector> Route;
	FVector PatrolTarget;
};
