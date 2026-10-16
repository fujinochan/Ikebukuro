#include "NinjaAppearance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

void FNinjaAppearance::Apply(ACharacter* Character, bool bRed)
{
	auto* Mesh=Character->GetMesh();
	auto* Cloth=LoadObject<UMaterialInterface>(nullptr,bRed?TEXT("/Game/Ninja/Materials/M_NinjaRed.M_NinjaRed"):TEXT("/Game/Ninja/Materials/M_NinjaBlack.M_NinjaBlack"));
	auto* Mask=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Ninja/Materials/M_NinjaMask.M_NinjaMask"));
	auto* Eyes=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Ninja/Materials/M_NinjaEyes.M_NinjaEyes"));
	if (!Cloth || !Mask || !Eyes || !Mesh->DoesSocketExist(TEXT("head"))) return;
	for (int32 I=0; I<Mesh->GetNumMaterials(); ++I) Mesh->SetMaterial(I,Cloth);
	const FVector Forward=Character->GetActorForwardVector(), Right=Character->GetActorRightVector();
	const FVector Head=Mesh->GetSocketLocation(TEXT("head"))+FVector(0,0,9);
	auto Add=[&](const TCHAR* Name,const TCHAR* Shape,FVector Position,FVector Scale,UMaterialInterface* Material,const TCHAR* Bone=TEXT("head"))
	{
		auto* Part=NewObject<UStaticMeshComponent>(Character,FName(Name));
		Character->AddInstanceComponent(Part);
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Shape)); Part->SetMaterial(0,Material);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetCanEverAffectNavigation(false);
		Part->RegisterComponent();
		Part->AttachToComponent(Mesh,FAttachmentTransformRules::KeepWorldTransform,Bone);
		Part->SetWorldLocation(Position); Part->SetWorldRotation(Character->GetActorRotation()); Part->SetWorldScale3D(Scale);
	};
	Add(TEXT("NinjaHood"),TEXT("/Engine/BasicShapes/Sphere.Sphere"),Head,FVector(.27,.26,.30),Cloth);
	Add(TEXT("NinjaFaceWrap"),TEXT("/Engine/BasicShapes/Cube.Cube"),Head+Forward*12-FVector(0,0,5),FVector(.08,.24,.14),Mask);
	for (int32 Side:{-1,1}) Add(Side<0?TEXT("NinjaEyeLeft"):TEXT("NinjaEyeRight"),TEXT("/Engine/BasicShapes/Cube.Cube"),Head+Forward*14.5+Right*(Side*5)+FVector(0,0,4),FVector(.025,.06,.025),Eyes);
	if (Mesh->DoesSocketExist(TEXT("pelvis"))) Add(TEXT("NinjaBelt"),TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),Mesh->GetSocketLocation(TEXT("pelvis")),FVector(.36,.29,.07),Mask,TEXT("pelvis"));
}
