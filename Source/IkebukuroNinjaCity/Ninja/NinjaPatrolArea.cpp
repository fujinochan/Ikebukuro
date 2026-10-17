#include "NinjaPatrolArea.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"

ANinjaPatrolArea::ANinjaPatrolArea() { PrimaryActorTick.bCanEverTick=false; SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root"))); }
bool ANinjaPatrolArea::RawSample(FVector Location,FVector& Foot) const
{
	const FVector Center=GetActorLocation();
	const FVector2D Extent=GetWalkingHalfExtent();
	if (FMath::Abs(Location.X-Center.X)>Extent.X-50 || FMath::Abs(Location.Y-Center.Y)>Extent.Y-50) return false;
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByObjectType(Hit,FVector(Location.X,Location.Y,Center.Z+HeightTolerance),
		FVector(Location.X,Location.Y,Center.Z-HeightTolerance),FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionQueryParams(SCENE_QUERY_STAT(NinjaFloor),true))) return false;
	if (Hit.ImpactNormal.Z<.85) return false;
	Foot=Hit.ImpactPoint; return true;
}
bool ANinjaPatrolArea::Sample(FVector Location,FVector& Foot) const
{
	if (!RawSample(Location,Foot)) return false;
	for (FVector Offset:{FVector(50,0,0),FVector(-50,0,0),FVector(0,50,0),FVector(0,-50,0)})
	{
		FVector Edge;
		if (!RawSample(Location+Offset,Edge) || FMath::Abs(Edge.Z-Foot.Z)>35) return false;
	}
	return !GetWorld()->OverlapAnyTestByObjectType(Foot+FVector(0,0,94),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeCapsule(43,88),FCollisionQueryParams(SCENE_QUERY_STAT(NinjaHeadroom),true));
}
bool ANinjaPatrolArea::ClearSegment(FVector Start,FVector End) const
{
	const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Start,End)/50));
	FVector Previous=Start;
	for (int32 I=0; I<=Steps; ++I)
	{
		FVector Foot;
		if (!Sample(FMath::Lerp(Start,End,double(I)/Steps),Foot) || FMath::Abs(Foot.Z-Previous.Z)>35) return false;
		Previous=Foot;
	}
	FHitResult Hit;
	return !GetWorld()->SweepSingleByObjectType(Hit,Start+FVector(0,0,127),End+FVector(0,0,127),FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(43,88),FCollisionQueryParams(SCENE_QUERY_STAT(NinjaWalkCorridor),true));
}
bool ANinjaPatrolArea::BuildGraph()
{
	Nodes.Reset(); Edges.Reset(); bReady=false;
	const FVector2D Extent=GetWalkingHalfExtent();
	const int32 NX=FMath::Clamp(FMath::FloorToInt(2*Extent.X/GridSpacing),1,40), NY=FMath::Clamp(FMath::FloorToInt(2*Extent.Y/GridSpacing),1,40);
	TMap<FIntPoint,int32> Lookup;
	for (int32 X=0; X<=NX; ++X) for (int32 Y=0; Y<=NY; ++Y)
	{
		FVector Foot;
		if (Sample(GetActorLocation()+FVector(-Extent.X+X*(2*Extent.X/NX),-Extent.Y+Y*(2*Extent.Y/NY),0),Foot))
		{ Lookup.Add(FIntPoint(X,Y),Nodes.Num()); Nodes.Add(Foot); }
	}
	Edges.SetNum(Nodes.Num());
	for (const auto& Pair:Lookup) for (FIntPoint Offset:{FIntPoint(1,0),FIntPoint(0,1)})
		if (const int32* Other=Lookup.Find(Pair.Key+Offset); Other && ClearSegment(Nodes[Pair.Value],Nodes[*Other]))
		{ Edges[Pair.Value].Add(*Other); Edges[*Other].Add(Pair.Value); }
	ValidNodes=Nodes.Num(); bReady=ValidNodes>=3; return bReady;
}
int32 ANinjaPatrolArea::Nearest(FVector Point) const
{
	int32 Best=INDEX_NONE; double Distance=FMath::Square(220.);
	for (int32 I=0; I<Nodes.Num(); ++I)
	{
		const double D=FVector::DistSquared(Point,Nodes[I]);
		if (D<Distance) { Best=I; Distance=D; }
	}
	return Best;
}
bool ANinjaPatrolArea::FindRoute(FVector Start,FVector End,TArray<FVector>& Route) const
{
	Route.Reset(); FVector Foot;
	if (!bReady || !Sample(End,Foot) || FMath::Abs(Foot.Z-End.Z)>100) return false;
	const int32 From=Nearest(Start), To=Nearest(Foot);
	if (From==INDEX_NONE || To==INDEX_NONE || !ClearSegment(Start,Nodes[From]) || !ClearSegment(Nodes[To],Foot)) return false;
	TArray<int32> Parent; Parent.Init(INDEX_NONE,Nodes.Num()); Parent[From]=From;
	TArray<int32> Queue; Queue.Add(From);
	for (int32 Q=0; Q<Queue.Num() && Parent[To]==INDEX_NONE; ++Q)
		for (int32 Next:Edges[Queue[Q]]) if (Parent[Next]==INDEX_NONE && ClearSegment(Nodes[Queue[Q]],Nodes[Next]))
		{ Parent[Next]=Queue[Q]; Queue.Add(Next); }
	if (Parent[To]==INDEX_NONE) return false;
	for (int32 I=To;;I=Parent[I]) { Route.Insert(Nodes[I],0); if (I==From) break; }
	if (Route.Num()>1 && ClearSegment(Start,Route[1])) Route.RemoveAt(0);
	Route.Add(Foot); return true;
}
FVector ANinjaPatrolArea::PatrolGoal(FVector Home,int32 Index) const
{
	TArray<FVector> Candidates;
	for (const FVector& Point:Nodes)
	{
		const FVector Delta=Point-Home;
		const double Distance=Delta.SizeSquared2D();
		if (Distance<150*150 || Distance>FMath::Square(PatrolTravelRadius) || (Index%2==0 ? Delta.X<=0 : Delta.X>=0)) continue;
		Candidates.Add(Point);
	}
	Candidates.Sort([Home](const FVector& A,const FVector& B){return FVector::DistSquared2D(A,Home)>FVector::DistSquared2D(B,Home);});
	for (const FVector& Point:Candidates)
	{
		TArray<FVector> Route; if (FindRoute(Home,Point,Route)) return Point;
	}
	return Home;
}
