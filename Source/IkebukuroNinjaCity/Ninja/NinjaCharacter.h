#pragma once

#include "CoreMinimal.h"
#include "IkebukuroNinjaCityCharacter.h"
#include "NinjaCharacter.generated.h"

class UInputMappingContext;
class ARedNinja;
enum class ENinjaDefeatMethod : uint8;

UCLASS()
class IKEBUKURONINJACITY_API ANinjaCharacter : public AIkebukuroNinjaCityCharacter
{
	GENERATED_BODY()
public:
	ANinjaCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void DoJumpStart() override;
	virtual void DoJumpEnd() override;
	virtual void DoMove(float Right,float Forward) override;
	virtual void DoLook(float Yaw,float Pitch) override;
	void ResetForMatch(FVector Foot,FRotator Rotation);

	UFUNCTION(BlueprintCallable, Category="Ninja|Input")
	void DoSprintStart();
	UFUNCTION(BlueprintCallable, Category="Ninja|Input")
	void DoSprintEnd();
	UFUNCTION(BlueprintPure, Category="Ninja|Jump")
	bool CanChainJump() const;
	UFUNCTION(BlueprintPure, Category="Ninja|Jump")
	float GetSecondsToJumpApex() const;
	UFUNCTION(BlueprintCallable,Category="Ninja|Combat") void DoThrow();
	bool TryThrowShuriken();
	bool TryStomp(ARedNinja* Enemy,const FHitResult& Hit);
	bool ReceiveContactDamage();
	void RegisterDefeat(ENinjaDefeatMethod Method);
	void SetSafeSurface(FVector Surface);
	void RecoverToSurface(FVector Surface,bool bFallPenalty);
	UFUNCTION() void OnNinjaHit(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
	UPROPERTY(EditDefaultsOnly,Category="Ninja|Input") TObjectPtr<UInputAction> ThrowAction;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Combat") int32 Health=3;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Combat") int32 Defeats=0;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Combat") int32 StompDefeats=0;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Combat") int32 ShurikenDefeats=0;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Combat") int32 ShotsFired=0;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Combat") int32 ShurikenWorldHits=0;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Combat") int32 Knockouts=0;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja|Combat") FString CombatFeedback=TEXT("F / LMB: shuriken  |  Stomp red ninjas from above");
	bool IsInvulnerable() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ninja|Movement")
	float WalkSpeed = 700;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ninja|Movement")
	float RunSpeed = 1400;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ninja|Camera")
	FVector CameraShoulderOffset=FVector(0,90,60);
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Ninja|Camera")
	bool bAutoAdjustCamera=true;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Ninja|Camera",meta=(ClampMin="0"))
	float CameraFollowSpeed=3.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Ninja|Camera",meta=(ClampMin="0",Units="deg/s"))
	float CameraMaxYawSpeed=100.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Ninja|Camera",meta=(ClampMin="0",Units="s"))
	float CameraManualGraceSeconds=1.2f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ninja|Jump")
	FVector JumpSpeeds = FVector(1100, 1700, 2300);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ninja|Jump", meta=(ClampMin="0.01", Units="s"))
	float TimingHalfWindow = 0.15f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Ninja|Jump")
	int32 JumpStage = 0;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Ninja|Movement")
	bool bIsSprinting = false;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Ninja|Jump")
	FString JumpFeedback = TEXT("SPACE to jump");
	UPROPERTY(EditDefaultsOnly, Category="Ninja|Input")
	TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(EditDefaultsOnly, Category="Ninja|Input")
	TObjectPtr<UInputMappingContext> SprintMappingContext;

private:
	void UpdateAutoCamera(float DeltaSeconds);
	void EndMoveInput();
	FVector2D CameraMoveInput=FVector2D::ZeroVector;
	float MovementReferenceYaw=0;
	bool bHasMovementReference=false;
	double ManualCameraUntil=0;
	double AimCameraUntil=0;
	bool bJumpButtonHeld = false;
	double PredictedApexTime = 0;
	double NextThrowTime=0;
	double InvulnerableUntil=0;
	FVector SafeSurface=FVector::ZeroVector;
	FVector PreviousFeet=FVector::ZeroVector;
	float PreviousVerticalVelocity=0;
	bool bCityHandlesRecovery=false;
};
