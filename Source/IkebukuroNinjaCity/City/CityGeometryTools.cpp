#include "CityGeometryTools.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "CesiumGeoreference.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "PhysicsEngine/BodySetup.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include <CesiumGltf/AccessorUtility.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumGltfReader/GltfReader.h>
#include <glm/mat4x4.hpp>

static TSharedPtr<FJsonObject> ReadCityJson(const FString& Path)
{
	FString Text;
	TSharedPtr<FJsonObject> Object;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object)) return nullptr;
	return Object;
}
#endif

FVector UCityGeometryTools::SurfacePoint(UObject* WorldContext, FVector2D Location)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	FHitResult Hit;
	if (World && World->LineTraceSingleByChannel(Hit, FVector(Location, 50000), FVector(Location, -10000), ECC_Visibility,
		FCollisionQueryParams(SCENE_QUERY_STAT(CitySurfaceSample), true))) return Hit.ImpactPoint;
	return FVector(Location, -1000000);
}

bool UCityGeometryTools::IsRouteClear(UObject* WorldContext, FVector Start, FVector End)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	FHitResult Hit;
	return World && !World->SweepSingleByChannel(Hit, Start + FVector(0,0,110), End + FVector(0,0,110), FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(60), FCollisionQueryParams(SCENE_QUERY_STAT(CityRouteSample), true));
}

UStaticMesh* UCityGeometryTools::BuildSurfaceMesh(const FString& AssetPath, const TArray<FVector>& Vertices, const TArray<int32>& Indices)
{
#if WITH_EDITOR
	if (!AssetPath.StartsWith(TEXT("/Game/Ninja/City/")) || Vertices.Num() == 0 || Indices.Num() % 3 != 0) return nullptr;
	UPackage* Package = CreatePackage(*AssetPath);
	const FString Name = FPackageName::GetLongPackageAssetName(AssetPath);
	UStaticMesh* Mesh = FindObject<UStaticMesh>(Package, *Name);
	if (!Mesh && FPackageName::DoesPackageExist(AssetPath)) Mesh = LoadObject<UStaticMesh>(nullptr, *(AssetPath + TEXT(".") + Name));
	const bool bNew = !Mesh;
	if (!Mesh) Mesh = NewObject<UStaticMesh>(Package, *Name, RF_Public | RF_Standalone);
	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	Attributes.GetVertexInstanceUVs().SetNumChannels(1);
	TArray<FVertexID> IDs;
	IDs.Reserve(Vertices.Num());
	for (const FVector& Position : Vertices)
	{
		const FVertexID ID = Description.CreateVertex();
		Attributes.GetVertexPositions()[ID] = FVector3f(Position);
		IDs.Add(ID);
	}
	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("CitySurface");
	for (int32 I = 0; I < Indices.Num(); I += 3)
	{
		if (!Vertices.IsValidIndex(Indices[I]) || !Vertices.IsValidIndex(Indices[I+1]) || !Vertices.IsValidIndex(Indices[I+2])) return nullptr;
		const FVector Normal = FVector::CrossProduct(Vertices[Indices[I+1]] - Vertices[Indices[I]], Vertices[Indices[I+2]] - Vertices[Indices[I]]).GetSafeNormal();
		if (Normal.IsNearlyZero()) continue;
		TArray<FVertexInstanceID, TInlineAllocator<3>> Instances;
		for (int32 J = 0; J < 3; ++J)
		{
			const int32 Index = Indices[I+J];
			const auto Instance = Description.CreateVertexInstance(IDs[Index]);
			Attributes.GetVertexInstanceNormals()[Instance] = FVector3f(Normal);
			Attributes.GetVertexInstanceUVs().Set(Instance, 0, FVector2f(Vertices[Index].X / 1000, Vertices[Index].Y / 1000));
			Instances.Add(Instance);
		}
		// Input normals use the mathematical cross product. UE StaticMesh physics
		// flips its index winding (GetPhysicsTriMeshData), so store clockwise faces.
		Swap(Instances[1], Instances[2]);
		Description.CreatePolygon(Group, Instances);
	}
	Mesh->GetStaticMaterials().Reset();
	Mesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), TEXT("CitySurface")));
	Mesh->CreateBodySetup();
	Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
	Mesh->GetBodySetup()->bDoubleSidedGeometry = true;
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bAllowCpuAccess = true;
	Params.bBuildSimpleCollision = false;
	if (!Mesh->BuildFromMeshDescriptions({&Description}, Params)) return nullptr;
	Mesh->GetBodySetup()->InvalidatePhysicsData();
	Mesh->GetBodySetup()->CreatePhysicsMeshes();
	Mesh->MarkPackageDirty();
	if (bNew) FAssetRegistryModule::AssetCreated(Mesh);
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	const FString Filename = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	if (!UPackage::SavePackage(Package, Mesh, *Filename, SaveArgs)) return nullptr;
	return Mesh;
#else
	return nullptr;
#endif
}

UStaticMesh* UCityGeometryTools::BakeLayer(const FString& DatasetRelativePath, const FString& AssetPath,
	ACesiumGeoreference* Georeference, FVector2D Minimum, FVector2D Maximum)
{
#if WITH_EDITOR
	if (!Georeference) return nullptr;
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	const FString Path = FPaths::ConvertRelativePathToFull(Root / DatasetRelativePath);
	if (!FPaths::IsUnderDirectory(Path, Root)) return nullptr;
	const auto Tileset = ReadCityJson(Path);
	if (!Tileset) return nullptr;
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	TArray<TSharedPtr<FJsonValue>> Samples;
	int64 SourceTriangles = 0;
	CesiumGltfReader::GltfReader Reader;
	CesiumGltfReader::GltfReaderOptions Options;
	Options.decodeEmbeddedImages = false;
	Options.resolveExternalImages = false;
	bool bError = false;
	for (const auto& TileValue : Tileset->GetObjectField(TEXT("root"))->GetArrayField(TEXT("children")))
	{
		const auto Content = TileValue->AsObject()->GetObjectField(TEXT("content"));
		FString Uri;
		if (!Content->TryGetStringField(TEXT("uri"), Uri)) Uri = Content->GetStringField(TEXT("url"));
		const FString TilePath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(Path) / Uri);
		if (!FPaths::IsUnderDirectory(TilePath, FPaths::GetPath(Path))) return nullptr;
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *TilePath) || Bytes.Num() < 28) return nullptr;
		uint32 Offset = 0;
		if (FMemory::Memcmp(Bytes.GetData(), "b3dm", 4) == 0)
		{
			Offset = 28;
			for (int32 Header = 3; Header < 7; ++Header) { uint32 Length; FMemory::Memcpy(&Length, Bytes.GetData() + Header*4, 4); Offset += Length; }
		}
		if (Offset >= static_cast<uint32>(Bytes.Num())) return nullptr;
		auto Result = Reader.readGltf(std::span<const std::byte>(reinterpret_cast<const std::byte*>(Bytes.GetData()+Offset), Bytes.Num()-Offset), Options);
		if (!Result.model || !Result.errors.empty()) { UE_LOG(LogTemp, Error, TEXT("City bake: decoder failed %s"), *Uri); return nullptr; }
		auto& Model = *Result.model;
		const auto Rtc = CesiumGltfContent::GltfUtilities::applyRtcCenter(Model, glm::dmat4(1));
		const auto RootTransform = CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(Model, Rtc);
		Model.forEachPrimitiveInScene(-1, [&](CesiumGltf::Model& M, CesiumGltf::Node&, CesiumGltf::Mesh&, CesiumGltf::MeshPrimitive& Primitive, const glm::dmat4& Transform)
		{
			if (Primitive.mode != CesiumGltf::MeshPrimitive::Mode::TRIANGLES) { bError = true; return; }
			const auto PositionView = CesiumGltf::getPositionAccessorView(M, Primitive);
			if (std::visit(CesiumGltf::StatusFromAccessor{}, PositionView) != CesiumGltf::AccessorViewStatus::Valid) { bError = true; return; }
			const int64 Count = std::visit(CesiumGltf::CountFromAccessor{}, PositionView);
			const auto IndexView = CesiumGltf::getIndexAccessorView(M, Primitive);
			const int64 IndexCount = Primitive.indices < 0 ? Count : std::visit(CesiumGltf::NumIndicesFromAccessor{}, IndexView);
			const auto Matrix = RootTransform * Transform;
			for (int64 Face = 0; Face < IndexCount/3; ++Face)
			{
				++SourceTriangles;
				const auto FaceIndices = std::visit(CesiumGltf::IndicesForFaceFromAccessor{Face, Count, Primitive.mode}, IndexView);
				FVector P[3];
				for (int32 J = 0; J < 3; ++J)
				{
					const auto Position = std::visit(CesiumGltf::PositionFromAccessor{FaceIndices[J]}, PositionView);
					if (!Position) { bError = true; return; }
					const glm::dvec4 Ecef = Matrix * glm::dvec4(*Position, 1);
					P[J] = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(FVector(Ecef.x, Ecef.y, Ecef.z));
				}
				FBox Bounds(ForceInit); for (const FVector& Point : P) Bounds += Point;
				if (Bounds.Max.X < Minimum.X || Bounds.Min.X > Maximum.X || Bounds.Max.Y < Minimum.Y || Bounds.Min.Y > Maximum.Y) continue;
				// Geographic conversion into UE is a reflection; reverse winding.
				Swap(P[1], P[2]);
				const FVector Cross = FVector::CrossProduct(P[1]-P[0], P[2]-P[0]);
				const double Area = Cross.Length() / 2;
				if (Area < 1) continue;
				const int32 Start = Vertices.Num();
				Vertices.Append(P, 3);
				Indices.Append({Start, Start+1, Start+2});
				if (Cross.GetSafeNormal().Z > 0.8 && Area > 10000)
				{
					const FVector C = (P[0]+P[1]+P[2])/3;
					auto Sample = MakeShared<FJsonObject>();
					Sample->SetNumberField(TEXT("x"), C.X); Sample->SetNumberField(TEXT("y"), C.Y); Sample->SetNumberField(TEXT("z"), C.Z);
					Sample->SetNumberField(TEXT("area_m2"), Area / 10000);
					Samples.Add(MakeShared<FJsonValueObject>(Sample));
				}
			}
		});
		if (bError) return nullptr;
	}
	auto Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("source_triangles"), SourceTriangles);
	Summary->SetNumberField(TEXT("saved_triangles"), Indices.Num()/3);
	Summary->SetArrayField(TEXT("upward_surface_samples"), Samples);
	FString Json;
	FJsonSerializer::Serialize(Summary, TJsonWriterFactory<>::Create(&Json));
	const FString Report = FPaths::ProjectSavedDir() / TEXT("Validation") / (FPackageName::GetLongPackageAssetName(AssetPath)+TEXT(".json"));
	FFileHelper::SaveStringToFile(Json, *Report);
	return BuildSurfaceMesh(AssetPath, Vertices, Indices);
#else
	return nullptr;
#endif
}
