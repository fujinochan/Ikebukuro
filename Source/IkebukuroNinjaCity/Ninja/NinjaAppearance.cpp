#include "NinjaAppearance.h"
#include "NinjaAppearanceCheck.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void FNinjaAppearance::Apply(ACharacter* Character, bool bRed)
{
    if (!IsValid(Character)) return;
    auto* Mesh=Character->GetMesh();
    auto* Cloth=LoadObject<UMaterialInterface>(nullptr,bRed?TEXT("/Game/Ninja/Materials/M_NinjaRed.M_NinjaRed"):TEXT("/Game/Ninja/Materials/M_NinjaBlack.M_NinjaBlack"));
    auto* Mask=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Ninja/Materials/M_NinjaMask.M_NinjaMask"));
    auto* Skin=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Ninja/Materials/M_NinjaSkin.M_NinjaSkin"));
    auto* Wrap=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Ninja/Materials/M_NinjaWrap.M_NinjaWrap"));
    if (!Mesh || !Cloth || !Mask || !Skin || !Wrap || !Mesh->DoesSocketExist(TEXT("head"))) return;
    for (int32 I=0; I<Mesh->GetNumMaterials(); ++I) Mesh->SetMaterial(I,Cloth);
    for (auto* Part:Character->GetComponentsByTag(UStaticMeshComponent::StaticClass(),TEXT("NinjaCostume"))) Part->DestroyComponent();
    const FVector Forward=Character->GetActorForwardVector(), Right=Character->GetActorRightVector(), Up=FVector::UpVector;
    const FQuat Facing=Character->GetActorQuat();
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto Add=[&](FName Name,UStaticMesh* Shape,FVector Position,FVector Size,UMaterialInterface* Material,FName Bone,FQuat Rotation)
    {
        if (!Shape || !Mesh->DoesSocketExist(Bone)) return;
        auto* Part=NewObject<UStaticMeshComponent>(Character,Name);
        Character->AddInstanceComponent(Part); Part->ComponentTags.Add(TEXT("NinjaCostume"));
        Part->SetStaticMesh(Shape); Part->SetMaterial(0,Material);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false); Part->SetCastShadow(false);
        Part->RegisterComponent();
        Part->AttachToComponent(Mesh,FAttachmentTransformRules::KeepWorldTransform,Bone);
        // Keep a bone-relative placement: the existing animation drives every garment.
        Part->SetWorldTransform(FTransform(Rotation,Position,Size/100.f));
    };
    auto Piece=[&](const TCHAR* Name,UStaticMesh* Shape,FVector Position,FVector Size,UMaterialInterface* Material,const TCHAR* Bone=TEXT("head"))
    { Add(FName(Name),Shape,Position,Size,Material,FName(Bone),Facing); };
    auto Limb=[&](FName Name,const TCHAR* From,const TCHAR* To,float Width,float LengthScale,UMaterialInterface* Material,UStaticMesh* Shape)
    {
        if (!Mesh->DoesSocketExist(From) || !Mesh->DoesSocketExist(To)) return;
        const FVector A=Mesh->GetSocketLocation(From), B=Mesh->GetSocketLocation(To), Direction=B-A;
        Add(Name,Shape,(A+B)*.5,FVector(Width,Width,Direction.Size()*LengthScale),Material,FName(From),FRotationMatrix::MakeFromZ(Direction).ToQuat());
    };
    const FVector Head=Mesh->GetSocketLocation(TEXT("head"))+Up*9;
    Piece(TEXT("NinjaHood"),Sphere,Head,FVector(36,35,40),Cloth);
    Piece(TEXT("NinjaEyeOpening"),Sphere,Head+Forward*16+Up*3,FVector(10,28,12),Skin);
    Piece(TEXT("NinjaFaceWrap"),Sphere,Head+Forward*14-Up*8,FVector(17,32,21),Cloth);
    Piece(TEXT("NinjaForeheadBand"),Cube,Head+Forward*16+Up*12,FVector(4,27,5),Wrap);
    for (int32 Side:{-1,1})
    {
        const FVector Eye=Head+Forward*21+Right*(Side*6)+Up*3;
        Piece(Side<0?TEXT("NinjaEyeLeft"):TEXT("NinjaEyeRight"),Sphere,Eye,FVector(2.4,4,4.8),Mask);
    }
    Piece(TEXT("NinjaHoodKnot"),Sphere,Head-Forward*18+Right*5-Up*6,FVector(13,15,13),Cloth);
    for (int32 Side:{-1,1})
        Add(Side<0?TEXT("NinjaHoodTailLeft"):TEXT("NinjaHoodTailRight"),Cube,Head-Forward*35+Right*(Side*9)-Up*7,FVector(37,10,3),Cloth,TEXT("head"),Facing*FRotator(Side*10.f,Side*15.f,Side*8.f).Quaternion());
    const FVector Pelvis=Mesh->GetSocketLocation(TEXT("pelvis")), Neck=Mesh->GetSocketLocation(TEXT("neck_01"));
    Piece(TEXT("NinjaTunic"),Sphere,FMath::Lerp(Pelvis,Neck,.52f),FVector(45,57,(Neck-Pelvis).Size()+8),Cloth,TEXT("spine_03"));
    Piece(TEXT("NinjaTunicHem"),Sphere,Pelvis+Up*4,FVector(49,59,34),Cloth,TEXT("pelvis"));
    for (int32 Side:{-1,1})
    {
        const FVector Top=Neck+Forward*21+Right*(Side*15)-Up*8, Bottom=Pelvis+Forward*24-Right*(Side*12)+Up*10;
        Add(Side<0?TEXT("NinjaLapelLeft"):TEXT("NinjaLapelRight"),Cube,(Top+Bottom)*.5,FVector(3,4,(Top-Bottom).Size()),Wrap,TEXT("spine_03"),FRotationMatrix::MakeFromZ(Top-Bottom).ToQuat());
    }
    Piece(TEXT("NinjaBelt"),Cylinder,Pelvis+Up*10,FVector(51,61,11),Mask,TEXT("pelvis"));
    Piece(TEXT("NinjaBeltKnot"),Sphere,Pelvis+Forward*27+Up*10,FVector(13,16,12),Wrap,TEXT("pelvis"));
    for (int32 Side:{-1,1})
        Add(Side<0?TEXT("NinjaBeltTailLeft"):TEXT("NinjaBeltTailRight"),Cube,Pelvis+Forward*27+Right*(Side*7)-Up*4,FVector(3,7,25),Mask,TEXT("pelvis"),FQuat(Forward,FMath::DegreesToRadians(Side*18.f))*Facing);
    for (const TCHAR* Side:{TEXT("l"),TEXT("r")})
    {
        const FString S(Side), Upper=TEXT("upperarm_")+S, Lower=TEXT("lowerarm_")+S, Hand=TEXT("hand_")+S;
        const FString Thigh=TEXT("thigh_")+S, Calf=TEXT("calf_")+S, Foot=TEXT("foot_")+S;
        Limb(FName(*(TEXT("NinjaSleeve_")+S)),*Upper,*Lower,25,1.25f,Cloth,Sphere);
        Limb(FName(*(TEXT("NinjaForearm_")+S)),*Lower,*Hand,17,1.05f,Mask,Sphere);
        const FVector Wrist=Mesh->GetSocketLocation(*Hand), ArmDirection=(Wrist-Mesh->GetSocketLocation(*Lower)).GetSafeNormal();
        Add(FName(*(TEXT("NinjaGlove_")+S)),Sphere,Wrist+ArmDirection*7,FVector(15,15,21),Mask,FName(*Hand),FRotationMatrix::MakeFromZ(ArmDirection).ToQuat());
        Limb(FName(*(TEXT("NinjaTrousers_")+S)),*Thigh,*Calf,31,1.16f,Cloth,Sphere);
        Limb(FName(*(TEXT("NinjaShin_")+S)),*Calf,*Foot,19,.96f,Mask,Sphere);
        for (int32 Band=0; Band<2; ++Band)
        {
            const FVector A=Mesh->GetSocketLocation(*Calf), B=Mesh->GetSocketLocation(*Foot);
            Add(FName(*FString::Printf(TEXT("NinjaShinTie_%s_%d"),Side,Band)),Cylinder,FMath::Lerp(A,B,.30f+Band*.32f),FVector(20,20,3),Wrap,FName(*Calf),FRotationMatrix::MakeFromZ(B-A).ToQuat());
        }
        const FString Ball=TEXT("ball_")+S;
        const FVector Ankle=Mesh->GetSocketLocation(*Foot), Toe=Mesh->DoesSocketExist(*Ball)?Mesh->GetSocketLocation(*Ball):Ankle+Forward*15;
        Piece(Side[0]=='l'?TEXT("NinjaBootLeft"):TEXT("NinjaBootRight"),Sphere,FMath::Lerp(Ankle,Toe,.65f)+Forward*4-Up*2,FVector(39,23,17),Mask,*Foot);
    }
    if (!bRed && FParse::Param(FCommandLine::Get(),TEXT("NinjaAppearanceCheck"))) Character->GetWorld()->SpawnActor<ANinjaAppearanceCheck>();
}
