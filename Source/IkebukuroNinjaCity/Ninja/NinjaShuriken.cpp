#include "NinjaShuriken.h"
#include "NinjaCharacter.h"
#include "RedNinja.h"
#include "NinjaMatchDirector.h"
#include "NinjaFeedback.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
ANinjaShuriken::ANinjaShuriken()
{
	PrimaryActorTick.bCanEverTick=true;
	Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision")); SetRootComponent(Collision);
	Collision->InitSphereRadius(8); Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionObjectType(ECC_GameTraceChannel1);
	Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel1,ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);
	Collision->OnComponentBeginOverlap.AddDynamic(this,&ANinjaShuriken::HostileOverlap);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Collision->SetCanEverAffectNavigation(false);
	Blades=CreateDefaultSubobject<USceneComponent>(TEXT("Blades")); Blades->SetupAttachment(Collision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (int32 I=0; I<4; ++I)
	{
		auto* Blade=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Blade%d"),I));
		Blade->SetupAttachment(Blades); Blade->SetStaticMesh(Cube.Object);
		Blade->SetRelativeScale3D(FVector(.025,.26,.06)); Blade->SetRelativeRotation(FRotator(0,0,I*45));
		Blade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Blade->SetCastShadow(false); Blade->SetCanEverAffectNavigation(false);
	}
	Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement")); Movement->SetUpdatedComponent(Collision);
	Movement->InitialSpeed=5000; Movement->MaxSpeed=5000; Movement->ProjectileGravityScale=0;
	Movement->bShouldBounce=false; Movement->bSweepCollision=true;
	Movement->OnProjectileStop.AddDynamic(this,&ANinjaShuriken::Impact); InitialLifeSpan=2;
}
void ANinjaShuriken::BeginPlay()
{
	Super::BeginPlay(); Origin=GetActorLocation();
	auto* Steel=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Ninja/Materials/M_NinjaSteel.M_NinjaSteel"));
	TArray<UStaticMeshComponent*> Parts; GetComponents(Parts);
	for (auto* Part:Parts) if (Steel) Part->SetMaterial(0,Steel);
}
void ANinjaShuriken::Launch(ANinjaCharacter* Ninja,FVector Direction,float Speed)
{
	Shooter=Ninja; SetOwner(Ninja); SetInstigator(Ninja); Collision->IgnoreActorWhenMoving(Ninja,true);
	Movement->MaxSpeed=Speed; Movement->Velocity=Direction.GetSafeNormal()*Speed; SetActorRotation(Direction.Rotation());
}
void ANinjaShuriken::LaunchEnemy(ARedNinja* Enemy,FVector Direction,float Speed)
{
	bHostile=true; SetOwner(Enemy); SetInstigator(Enemy); Collision->IgnoreActorWhenMoving(Enemy,true);
	// Overlap pawns so red teammates cannot block or be hurt by their own volleys.
	Collision->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	MaximumRange=12000; SetLifeSpan(MaximumRange/FMath::Max(Speed,1.f)+.25f);
	Movement->MaxSpeed=Speed; Movement->Velocity=Direction.GetSafeNormal()*Speed; SetActorRotation(Direction.Rotation());
	Blades->SetRelativeScale3D(FVector(1.8f));
	if (auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Ninja/Materials/M_NinjaBurst.M_NinjaBurst")))
	{
		auto* Red=UMaterialInstanceDynamic::Create(Base,this); Red->SetVectorParameterValue(TEXT("Color"),FLinearColor(4,.1,.005));
		TArray<UStaticMeshComponent*> Parts; GetComponents(Parts); for (auto* Part:Parts) Part->SetMaterial(0,Red);
	}
}
void ANinjaShuriken::HostileOverlap(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&)
{
	if (!bHostile || bResolved || !Cast<ANinjaCharacter>(Other)) return;
	bResolved=true;
	if (ANinjaMatchDirector::AllowsGameplay(this)) Cast<ANinjaCharacter>(Other)->ReceiveContactDamage();
	Destroy();
}
void ANinjaShuriken::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); Blades->AddLocalRotation(FRotator(0,0,1440*DeltaSeconds));
	if (FVector::DistSquared(Origin,GetActorLocation())>=FMath::Square(MaximumRange)) Destroy();
}
void ANinjaShuriken::Impact(const FHitResult& Hit)
{
	if (bResolved) return;
	bResolved=true;
	if (!ANinjaMatchDirector::AllowsGameplay(this)) { Destroy(); return; }
	if (bHostile) ANinjaFeedback::Emit(this,Hit.ImpactPoint,TEXT("Impact"));
	else if (auto* Enemy=Cast<ARedNinja>(Hit.GetActor())) Enemy->TryDefeat(ENinjaDefeatMethod::Shuriken,Shooter.Get());
	else if (Shooter.IsValid()) { ++Shooter->ShurikenWorldHits; ANinjaFeedback::Emit(this,Hit.ImpactPoint,TEXT("Impact")); }
	Destroy();
}
