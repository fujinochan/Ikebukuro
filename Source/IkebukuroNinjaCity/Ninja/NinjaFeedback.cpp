#include "NinjaFeedback.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

int32 ANinjaFeedback::EmittedCount=0;
ANinjaFeedback::ANinjaFeedback()
{
	PrimaryActorTick.bCanEverTick=true; InitialLifeSpan=.32f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (int32 I=0;I<8;++I)
	{
		auto* Ray=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Ray%d"),I));
		Ray->SetupAttachment(RootComponent); Ray->SetStaticMesh(Cube.Object);
		Ray->SetRelativeScale3D(FVector(.02));
		Ray->SetCollisionEnabled(ECollisionEnabled::NoCollision); Ray->SetCastShadow(false);
		Ray->SetCanEverAffectNavigation(false); Rays.Add(Ray);
	}
}
void ANinjaFeedback::Emit(const UObject* Context,FVector Location,const FString& Cue)
{
	if (!Context || !Context->GetWorld() || Context->GetWorld()->IsPaused()) return;
	++EmittedCount;
	if (auto* Sound=LoadObject<USoundWave>(nullptr,*(TEXT("/Game/Ninja/Audio/S_")+Cue+TEXT(".S_")+Cue)))
		UGameplayStatics::PlaySoundAtLocation(Context,Sound,Location,.32f);
	auto* Burst=Context->GetWorld()->SpawnActor<ANinjaFeedback>(Location,FRotator::ZeroRotator);
	if (!Burst) return;
	const FLinearColor Color=Cue==TEXT("Damage")?FLinearColor(3,.1,.03):Cue==TEXT("Hit") || Cue==TEXT("Stomp")?FLinearColor(3,1.5,.1):FLinearColor(.05,2,3);
	auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Ninja/Materials/M_NinjaBurst.M_NinjaBurst"));
	if (Material) for (UStaticMeshComponent* Ray:Burst->Rays)
	{
		auto* Dynamic=UMaterialInstanceDynamic::Create(Material,Burst);
		Dynamic->SetVectorParameterValue(TEXT("Color"),Color); Ray->SetMaterial(0,Dynamic);
	}
}
void ANinjaFeedback::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); Age+=DeltaSeconds;
	const float T=FMath::Clamp(Age/.32f,0.f,1.f);
	for (int32 I=0;I<Rays.Num();++I)
	{
		const float A=I*PI/4; const FVector D(FMath::Cos(A),FMath::Sin(A),.2);
		Rays[I]->SetRelativeLocation(D*(18+T*65)); Rays[I]->SetRelativeRotation(D.Rotation());
		Rays[I]->SetRelativeScale3D(FVector(.20,.035,.035)*(1-T));
	}
}
