#include "NinjaExplosion.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

int32 ANinjaExplosion::EnemyBursts=0;
int32 ANinjaExplosion::PlayerBursts=0;
ANinjaExplosion::ANinjaExplosion()
{
	PrimaryActorTick.bCanEverTick=true; InitialLifeSpan=1.45f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// One flash, ten fireballs, ten smoke puffs, 24 sparks, and a 24-piece shock ring.
	for (int32 I=0; I<69; ++I)
	{
		auto* Piece=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Blast%d"),I));
		Piece->SetupAttachment(RootComponent); Piece->SetStaticMesh(I<21?Sphere.Object:Cube.Object);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision); Piece->SetGenerateOverlapEvents(false);
		Piece->SetCastShadow(false); Piece->SetCanEverAffectNavigation(false);
		Piece->SetRelativeScale3D(FVector(.001)); Pieces.Add(Piece);
	}
}
void ANinjaExplosion::BeginPlay()
{
	Super::BeginPlay();
	auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Ninja/Materials/M_NinjaBurst.M_NinjaBurst"));
	for (UStaticMeshComponent* Piece:Pieces)
	{
		auto* Dynamic=Material?UMaterialInstanceDynamic::Create(Material,this):nullptr;
		Materials.Add(Dynamic); if (Dynamic) Piece->SetMaterial(0,Dynamic);
	}
	Tick(0);
}
void ANinjaExplosion::Emit(const UObject* Context,FVector Location,bool bPlayer)
{
	if (!Context || !Context->GetWorld() || Context->GetWorld()->IsPaused()) return;
	auto* Blast=Context->GetWorld()->SpawnActor<ANinjaExplosion>(Location,FRotator::ZeroRotator);
	if (!Blast) return;
	if (bPlayer) ++PlayerBursts; else ++EnemyBursts;
	if (auto* Sound=LoadObject<USoundWave>(nullptr,TEXT("/Game/Ninja/Audio/S_Explosion.S_Explosion")))
		UGameplayStatics::PlaySoundAtLocation(Context,Sound,Location,.55f);
}
void ANinjaExplosion::Clear(UWorld* World)
{
	if (World) for (TActorIterator<ANinjaExplosion> It(World); It; ++It) It->Destroy();
}
void ANinjaExplosion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds); Age+=DeltaSeconds;
	for (int32 I=0; I<Pieces.Num(); ++I)
	{
		FVector Position=FVector::ZeroVector, Scale; FLinearColor Color;
		if (I==0)
		{
			const float T=FMath::Clamp(Age/.22f,0.f,1.f);
			Scale=FVector(5.5f*FMath::Sin(T*PI)); Color=FLinearColor(4,1,.08);
		}
		else if (I<=10)
		{
			const float T=FMath::Clamp(Age/.7f,0.f,1.f), A=I*2.39996f;
			const FVector D(FMath::Cos(A),FMath::Sin(A),.25f+(I%3)*.25f);
			Position=D*(30+T*220); Scale=FVector((1.7f+I%3*.25f)*FMath::Sin(PI*T));
			Color=FMath::Lerp(FLinearColor(3,.38,.012),FLinearColor(.4,.015,.001),T)*(.65f+.18f*(I%3));
		}
		else if (I<=20)
		{
			const float T=FMath::Clamp((Age-.15f)/1.25f,0.f,1.f), A=I*2.39996f;
			Position=FVector(FMath::Cos(A)*T*210,FMath::Sin(A)*T*210,50+T*(200+(I%3)*40));
			Scale=FVector((1.4f+T*1.4f)*FMath::Sin(PI*T));
			Color=FLinearColor(.045,.055,.07)*(1-T*.65f);
		}
		else if (I<45)
		{
			const float T=FMath::Clamp(Age/.9f,0.f,1.f), A=I*2.39996f;
			const FVector D(FMath::Cos(A),FMath::Sin(A),.25f+(I%5)*.18f);
			Position=D*(40+T*480); Position.Z-=T*T*170;
			Pieces[I]->SetRelativeRotation(D.Rotation());
			Scale=FVector(.7,.055,.055)*(1-T); Color=FLinearColor(5,1.4,.06)*(1-T);
		}
		else
		{
			const float T=FMath::Clamp(Age/.55f,0.f,1.f), A=(I-45)*2*PI/24;
			Position=FVector(FMath::Cos(A),FMath::Sin(A),0)*(35+T*430); Position.Z=-65;
			Pieces[I]->SetRelativeRotation(FRotator(0,FMath::RadiansToDegrees(A)+90,0));
			Scale=FVector(.15f+T*1.05f,.10f*(1-T),.07f*(1-T)); Color=FLinearColor(4,1.1,.07);
		}
		Pieces[I]->SetVisibility(Scale.GetAbsMax()>.01f);
		Pieces[I]->SetRelativeLocation(Position); Pieces[I]->SetRelativeScale3D(Scale);
		if (Materials.IsValidIndex(I) && Materials[I]) Materials[I]->SetVectorParameterValue(TEXT("Color"),Color);
	}
}
