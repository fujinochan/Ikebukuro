#include "RedNinja.h"
#include "NinjaCharacter.h"
#include "NinjaAppearance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
ARedNinja::ARedNinja()
{
	GetCapsuleComponent()->InitCapsuleSize(42,90);
	GetCapsuleComponent()->CanCharacterStepUpOn=ECB_No;
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->MaxWalkSpeed=0;
	PrimaryActorTick.bCanEverTick=false;
}
void ARedNinja::BeginPlay() { Super::BeginPlay(); FNinjaAppearance::Apply(this,true); }
bool ARedNinja::TryDefeat(ENinjaDefeatMethod Method,ANinjaCharacter* Ninja)
{
	if (bDefeated || !IsValid(Ninja)) return false;
	bDefeated=true; DefeatMethod=Method;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement(); Ninja->RegisterDefeat(Method);
	// Retain identity briefly for a subsequent CharacterMovement landing callback.
	SetActorHiddenInGame(true); SetLifeSpan(0.8f); return true;
}
