#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RedNinja.generated.h"
class ANinjaCharacter;
UENUM(BlueprintType)
enum class ENinjaDefeatMethod : uint8 { Stomp, Shuriken };
UCLASS()
class IKEBUKURONINJACITY_API ARedNinja : public ACharacter
{
	GENERATED_BODY()
public:
	ARedNinja();
	virtual void BeginPlay() override;
	bool TryDefeat(ENinjaDefeatMethod Method, ANinjaCharacter* Ninja);
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja") bool bDefeated=false;
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,Category="Ninja") ENinjaDefeatMethod DefeatMethod=ENinjaDefeatMethod::Stomp;
};
