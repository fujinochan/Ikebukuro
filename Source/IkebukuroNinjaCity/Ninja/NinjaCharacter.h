#pragma once

#include "CoreMinimal.h"
#include "IkebukuroNinjaCityCharacter.h"
#include "NinjaCharacter.generated.h"

class UInputMappingContext;

UCLASS()
class IKEBUKURONINJACITY_API ANinjaCharacter : public AIkebukuroNinjaCityCharacter
{
	GENERATED_BODY()
public:
	ANinjaCharacter();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void DoJumpStart() override;
	virtual void DoJumpEnd() override;

	UFUNCTION(BlueprintCallable, Category="Ninja|Input")
	void DoSprintStart();
	UFUNCTION(BlueprintCallable, Category="Ninja|Input")
	void DoSprintEnd();
	UFUNCTION(BlueprintPure, Category="Ninja|Jump")
	bool CanChainJump() const;
	UFUNCTION(BlueprintPure, Category="Ninja|Jump")
	float GetSecondsToJumpApex() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ninja|Movement")
	float WalkSpeed = 700;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ninja|Movement")
	float RunSpeed = 1400;
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
	bool bJumpButtonHeld = false;
	double PredictedApexTime = 0;
};
