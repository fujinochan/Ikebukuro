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
	UPROPERTY() int32 RouteOrder=0;
	UPROPERTY() FString RouteHint;
	UPROPERTY() TArray<FVector> AssistRoute;
	UPROPERTY() float DetectionRadius=1000;
	UPROPERTY() float LeashRadius=1800;
	UPROPERTY() bool bRanged=true;
	UPROPERTY() float RangedRadius=10000;
	UPROPERTY() float VolleyInterval=2;
	UPROPERTY() float ProjectileSpeed=2500;
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
	float ResultAge=0;
	int32 RemainingEnemies() const { return Enemies.Num()-Credited.Num(); }
	bool UsesRoundMusic() const { return bMusicRound; }
	class UMediaPlayer* GetRoundMusicPlayer() const { return RoundMusic; }
	class UMediaSoundComponent* GetRoundMusicSound() const { return MusicSound; }
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") float RoundDurationSeconds=300;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") FString MusicError;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") ENinjaMatchState State=ENinjaMatchState::Loading;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") float RemainingSeconds=300;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") float CountdownSeconds=3;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") FString ResultReason;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Match") int32 RoundNumber=1;
	UPROPERTY() TArray<TObjectPtr<ARedNinja>> Enemies;
private:
	bool IsPrepared() const;
	void Finish(bool bWon,const FString& Reason);
	void OpenRoundMusic();
	void StopRoundMusic();
	void StartRound();
	UFUNCTION() void MusicEnded();
	UFUNCTION() void MusicOpenFailed(FString Url);
	UPROPERTY() TObjectPtr<class UMediaPlayer> RoundMusic;
	UPROPERTY() TObjectPtr<class UFileMediaSource> MusicSource;
	UPROPERTY() TObjectPtr<class UMediaSoundComponent> MusicSound;
	bool bMusicRound=false,bMusicReady=false,bMusicStarted=false,bSongEnded=false;
	bool bActive=true;
	float PreparedSeconds=0;
	FVector StartFoot;
	FRotator StartRotation;
	UPROPERTY() TObjectPtr<ANinjaCharacter> Ninja;
	UPROPERTY() TArray<FNinjaMatchSpawn> Spawns;
	TSet<TWeakObjectPtr<ARedNinja>> Credited;
};
