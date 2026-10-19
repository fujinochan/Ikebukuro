#include "CityGroundColorTools.h"
#if WITH_EDITOR
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "CesiumGeoreference.h"
#if PLATFORM_WINDOWS
#undef min
#undef max
#undef GetObject
#undef DrawText
#endif
#endif

bool UCityGroundColorTools::GenerateGroundColors(const FString& ManifestRelativePath, ACesiumGeoreference* Geo)
{
#if WITH_EDITOR
	const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const FString ManifestPath=FPaths::ConvertRelativePathToFull(Root/ManifestRelativePath);
	if (!Geo || !FPaths::IsUnderDirectory(ManifestPath,Root)) return false;
	FString Text; TSharedPtr<FJsonObject> Manifest;
	if (!FFileHelper::LoadFileToString(Text,*ManifestPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Manifest)) return false;
	const auto& Low=Manifest->GetArrayField(TEXT("world_min_cm")); const auto& High=Manifest->GetArrayField(TEXT("world_max_cm"));
	const FVector2D Minimum(Low[0]->AsNumber(),Low[1]->AsNumber()), Maximum(High[0]->AsNumber(),High[1]->AsNumber()), Span=Maximum-Minimum;
	const int32 Zoom=Manifest->GetIntegerField(TEXT("zoom"));
	TMap<FIntPoint,FImage> Tiles;
	for (const auto& Row:Manifest->GetArrayField(TEXT("tiles")))
	{
		const auto& Tile=Row->AsObject(); const FString Path=FPaths::ConvertRelativePathToFull(Root/Tile->GetStringField(TEXT("file")));
		FImage Image;
		if (!FPaths::IsUnderDirectory(Path,Root/TEXT("Downloads/GSI")) || !FImageUtils::LoadImage(*Path,Image) || Image.SizeX!=256 || Image.SizeY!=256) return false;
		Image.ChangeFormat(ERawImageFormat::BGRA8,EGammaSpace::sRGB);
		Tiles.Add(FIntPoint(Tile->GetIntegerField(TEXT("x")),Tile->GetIntegerField(TEXT("y"))),MoveTemp(Image));
	}
	constexpr int32 Grid=256, Size=1024;
	TArray<FColor> Coarse; Coarse.SetNum(Grid*Grid);
	TArray<uint8> Classes; Classes.SetNumZeroed(Grid*Grid);
	TArray<bool> Roads; Roads.Init(false,Grid*Grid);
	const FColor Palette[]={FColor(120,119,109),FColor(68,76,82),FColor(73,99,57),FColor(147,130,103),FColor(147,145,135)};
	FImage Photo(Size,Size,ERawImageFormat::BGRA8,EGammaSpace::sRGB);
	auto* PhotoPixels=reinterpret_cast<FColor*>(Photo.RawData.GetData());
	bool Missing=false;
	auto Sample=[&](FVector2D P)->FColor
	{
		const FVector LL=Geo->TransformUnrealPositionToLongitudeLatitudeHeight(FVector(P,0));
		const double Phi=FMath::DegreesToRadians(LL.Y), Scale=double(1<<Zoom)*256;
		const int32 PX=FMath::FloorToInt((LL.X+180)/360*Scale);
		const double T=FMath::Tan(Phi);
		const int32 PY=FMath::FloorToInt((1-FMath::Loge(T+FMath::Sqrt(T*T+1))/PI)*.5*Scale);
		const FImage* Tile=Tiles.Find(FIntPoint(PX/256,PY/256));
		if (!Tile) { Missing=true; return FColor::Magenta; }
		return reinterpret_cast<const FColor*>(Tile->RawData.GetData())[(PY%256)*256+PX%256];
	};
	// The photo is resampled through the actual saved ESU georeference, not a
	// guessed UV orientation. +X is east and +Y is south, matching image rows.
	for (int32 Y=0;Y<Size;++Y) for (int32 X=0;X<Size;++X)
		PhotoPixels[Y*Size+X]=Sample(Minimum+Span*FVector2D((X+.5)/Size,(Y+.5)/Size));
	if (Missing) return false;
	for (int32 Y=0;Y<Grid;++Y) for (int32 X=0;X<Grid;++X)
	{
		int32 R=0,G=0,B=0;
		for (int32 J=0;J<4;++J) for (int32 I=0;I<4;++I)
		{ const FColor C=PhotoPixels[(Y*4+J)*Size+X*4+I]; R+=C.R; G+=C.G; B+=C.B; }
		R/=16; G/=16; B/=16;
		const int32 Bright=(R+G+B)/3; uint8 Class=0;
		if (G>R*1.10 && G>B*1.03 && G-R>5) Class=2;
		else if (R>G*1.10 && G>B*1.12 && Bright>100) Class=3;
		else if (Bright>150) Class=4;
		else if (Bright<85) Class=1;
		Classes[Y*Grid+X]=Class;
	}
	// The saved road mesh supplies a continuous pavement mask. This prevents
	// building shadows or roadside tree crowns from becoming patches on roads.
	auto* RoadMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Ninja/City/SM_RoadsCollision.SM_RoadsCollision"));
	if (!RoadMesh || !RoadMesh->GetRenderData() || RoadMesh->GetRenderData()->LODResources.IsEmpty()) return false;
	const auto& LOD=RoadMesh->GetRenderData()->LODResources[0];
	const auto& Positions=LOD.VertexBuffers.PositionVertexBuffer;
	for (int32 I=0;I+2<LOD.IndexBuffer.GetNumIndices();I+=3)
	{
		FVector2D P[3];
		for (int32 J=0;J<3;++J) { const FVector V(Positions.VertexPosition(LOD.IndexBuffer.GetIndex(I+J))); P[J]=(FVector2D(V.X,V.Y)-Minimum)/Span*Grid; }
		const double D=(P[1].Y-P[2].Y)*(P[0].X-P[2].X)+(P[2].X-P[1].X)*(P[0].Y-P[2].Y);
		if (FMath::Abs(D)<1.e-6) continue;
		const int32 X0=FMath::Clamp(FMath::FloorToInt(FMath::Min3(P[0].X,P[1].X,P[2].X)),0,Grid-1), X1=FMath::Clamp(FMath::CeilToInt(FMath::Max3(P[0].X,P[1].X,P[2].X)),0,Grid-1);
		const int32 Y0=FMath::Clamp(FMath::FloorToInt(FMath::Min3(P[0].Y,P[1].Y,P[2].Y)),0,Grid-1), Y1=FMath::Clamp(FMath::CeilToInt(FMath::Max3(P[0].Y,P[1].Y,P[2].Y)),0,Grid-1);
		for (int32 Y=Y0;Y<=Y1;++Y) for (int32 X=X0;X<=X1;++X)
		{
			const double U=((P[1].Y-P[2].Y)*(X+.5-P[2].X)+(P[2].X-P[1].X)*(Y+.5-P[2].Y))/D;
			const double V=((P[2].Y-P[0].Y)*(X+.5-P[2].X)+(P[0].X-P[2].X)*(Y+.5-P[2].Y))/D;
			if (U>=0 && V>=0 && U+V<=1) Roads[Y*Grid+X]=true;
		}
	}
	int32 Counts[5]={0,0,0,0,0}, RoadCells=0;
	for (int32 Y=0;Y<Grid;++Y) for (int32 X=0;X<Grid;++X)
	{
		uint8 Class=Classes[Y*Grid+X];
		if (Roads[Y*Grid+X]) { Class=1; ++RoadCells; }
		else
		{
			int32 Votes[5]={0,0,0,0,0};
			for (int32 J=-1;J<=1;++J) for (int32 I=-1;I<=1;++I) ++Votes[Classes[FMath::Clamp(Y+J,0,Grid-1)*Grid+FMath::Clamp(X+I,0,Grid-1)]];
			for (uint8 C=0;C<5;++C) if (Votes[C]>Votes[Class]) Class=C;
		}
		++Counts[Class]; Coarse[Y*Grid+X]=Palette[Class];
	}
	FImage Colors(Size,Size,ERawImageFormat::BGRA8,EGammaSpace::sRGB);
	auto* Output=reinterpret_cast<FColor*>(Colors.RawData.GetData());
	for (int32 Y=0;Y<Size;++Y) for (int32 X=0;X<Size;++X)
	{
		const float GX=(X+.5f)/4-.5f,GY=(Y+.5f)/4-.5f; const int32 IX=FMath::FloorToInt(GX),IY=FMath::FloorToInt(GY);
		auto At=[&](int32 A,int32 B) { return Coarse[FMath::Clamp(B,0,Grid-1)*Grid+FMath::Clamp(A,0,Grid-1)].ReinterpretAsLinear(); };
		const FLinearColor C=FMath::Lerp(FMath::Lerp(At(IX,IY),At(IX+1,IY),GX-IX),FMath::Lerp(At(IX,IY+1),At(IX+1,IY+1),GX-IX),GY-IY);
		Output[Y*Size+X]=C.ToFColor(false);
	}
	const FString OutputPath=Root/TEXT("Content/Ninja/City/Textures/T_CityGroundColors.png");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath),true);
	if (!FImageUtils::SaveImageByExtension(*OutputPath,Colors) || !FImageUtils::SaveImageByExtension(*(Root/TEXT("Saved/Validation/ground-photo-aligned.png")),Photo)) return false;
	auto Report=MakeShared<FJsonObject>(); Report->SetStringField(TEXT("status"),TEXT("passed"));
	Report->SetStringField(TEXT("source"),TEXT("GSI seamlessphoto; coarse color inference plus saved PLATEAU road mesh mask"));
	Report->SetNumberField(TEXT("tiles"),Tiles.Num()); Report->SetNumberField(TEXT("resolution"),Size); Report->SetNumberField(TEXT("classification_grid"),Grid);
	Report->SetNumberField(TEXT("road_mask_cells"),RoadCells); Report->SetNumberField(TEXT("road_triangles"),LOD.IndexBuffer.GetNumIndices()/3);
	TArray<TSharedPtr<FJsonValue>> Histogram; for (int32 Count:Counts) Histogram.Add(MakeShared<FJsonValueNumber>(Count)); Report->SetArrayField(TEXT("class_histogram_neutral_road_green_earth_light"),Histogram);
	Report->SetBoolField(TEXT("all_photo_samples_covered"),!Missing);
	FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
	return FFileHelper::SaveStringToFile(Json,*(Root/TEXT("Saved/Validation/ground-colors-generation.json")));
#else
	return false;
#endif
}
