// Copyright GTAI. All Rights Reserved.

#include "GTAI_WorldPartitionSetup.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"

#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionEditorSpatialHash.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogATLAS_WP, Log, All);

namespace GTAI::City::Zoning
{
	// NYC zoning max building heights (m) used to clamp OSM estimates.
	// Real values are more nuanced (setbacks, special districts), but these
	// are robust game-side caps so the skyline stays plausible.
	//   R8/R9  residential  -> ~85 m  (tallest contextual towers capped)
	//   C5/C6  commercial   -> ~200 m (Midtown core; supertall needs special permit)
	//   M1/M2  manufacturing-> ~40 m
	//   PARK   green         -> 0 (no mass)
	static float MaxHeightM(const FString& Z)
	{
		if (Z.StartsWith(TEXT("R")))  return 85.f;   // residential
		if (Z.StartsWith(TEXT("C")))  return 200.f;  // commercial
		if (Z.StartsWith(TEXT("M")))  return 40.f;   // manufacturing
		if (Z == TEXT("PARK"))        return 0.f;
		return 250.f; // unknown -> permissive
	}
}

AGTAI_WorldPartitionSetup::AGTAI_WorldPartitionSetup()
{
	PrimaryActorTick.bCanEverTick = false;
#if WITH_EDITOR
	bIsEditorOnlyActor = true;
#endif
	CellSizeCm = CellSizeM * 100.f;
}

void AGTAI_WorldPartitionSetup::BeginPlay()
{
	Super::BeginPlay();
}

// ==========================================================================
// JSON PARSING
// ==========================================================================

bool AGTAI_WorldPartitionSetup::LoadCityData(const FString& Path, GTAI::City::FOSMCityData& Out) const
{
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path))
	{
		UE_LOG(LogATLAS_WP, Error, TEXT("City JSON not found: %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogATLAS_WP, Error, TEXT("Failed to parse JSON: %s"), *Path);
		return false;
	}

	// --- metadata ---
	const TSharedPtr<FJsonObject>* Meta = nullptr;
	if (Root->TryGetObjectField(TEXT("metadata"), Meta))
	{
		Out.Attribution = (*Meta)->GetStringField(TEXT("attribution"));
		Out.UEScaleCmPerM = (float)(*Meta)->GetNumberField(TEXT("ue_scale_cm_per_m"));
		const TSharedPtr<FJsonObject>* Anchor = nullptr;
		if ((*Meta)->TryGetObjectField(TEXT("anchor"), Anchor))
		{
			Out.AnchorLat = (*Anchor)->GetNumberField(TEXT("lat"));
			Out.AnchorLon = (*Anchor)->GetNumberField(TEXT("lon"));
		}
		const TSharedPtr<FJsonObject>* Stats = nullptr;
		if ((*Meta)->TryGetObjectField(TEXT("stats"), Stats))
		{
			Out.StatsBuildings = (int32)(*Stats)->GetNumberField(TEXT("buildings"));
			Out.StatsStreets = (int32)(*Stats)->GetNumberField(TEXT("streets"));
		}
	}

	ParseBuildings(Root, Out);
	ParseStreets(Root, Out);
	ParseSignals(Root, Out);
	ParseDistricts(Root, Out);
	UE_LOG(LogATLAS_WP, Log, TEXT("Loaded city: %d buildings, %d streets, %d signals, %d districts (scale=%.1f cm/m)"),
		Out.Buildings.Num(), Out.Streets.Num(), Out.Signals.Num(), Out.Districts.Num(), Out.UEScaleCmPerM);
	return true;
}

bool AGTAI_WorldPartitionSetup::ParseBuildings(const TSharedPtr<FJsonObject>& Root, GTAI::City::FOSMCityData& Out) const
{
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("buildings"), Arr)) return true;

	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject>& B = V->AsObject();
		GTAI::City::FOSMBuilding Rec;
		Rec.Id = B->GetStringField(TEXT("id"));
		Rec.Name = B->GetStringField(TEXT("name"));
		Rec.Use = B->GetStringField(TEXT("use"));
		Rec.HeightM = (float)B->GetNumberField(TEXT("height_m"));
		Rec.HeightUE = (float)B->GetNumberField(TEXT("height_ue"));
		Rec.Floors = (int32)B->GetNumberField(TEXT("floors"));

		const TArray<TSharedPtr<FJsonValue>>* Ring = nullptr;
		if (B->TryGetArrayField(TEXT("footprint_local_m"), Ring))
		{
			for (const TSharedPtr<FJsonValue>& P : *Ring)
			{
				const TArray<TSharedPtr<FJsonValue>>& C = P->AsArray();
				// [east_m, north_m]
				Rec.FootprintLocalM.Add(FVector2D((float)C[0]->AsNumber(), (float)C[1]->AsNumber()));
			}
		}
		Out.Buildings.Add(MoveTemp(Rec));
	}
	return true;
}

bool AGTAI_WorldPartitionSetup::ParseStreets(const TSharedPtr<FJsonObject>& Root, GTAI::City::FOSMCityData& Out) const
{
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("streets"), Arr)) return true;

	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject>& S = V->AsObject();
		GTAI::City::FOSMStreet Rec;
		Rec.Id = S->GetStringField(TEXT("id"));
		Rec.Name = S->GetStringField(TEXT("name"));
		Rec.Highway = S->GetStringField(TEXT("highway"));
		Rec.WidthM = (float)S->GetNumberField(TEXT("width_m"));
		Rec.Lanes = (int32)S->GetNumberField(TEXT("lanes"));
		Rec.bOneWay = S->GetBoolField(TEXT("oneway"));

		const TArray<TSharedPtr<FJsonValue>>* Line = nullptr;
		if (S->TryGetArrayField(TEXT("polyline_local_m"), Line))
		{
			for (const TSharedPtr<FJsonValue>& P : *Line)
			{
				const TArray<TSharedPtr<FJsonValue>>& C = P->AsArray();
				Rec.PolylineLocalM.Add(FVector2D((float)C[0]->AsNumber(), (float)C[1]->AsNumber()));
			}
		}
		Out.Streets.Add(MoveTemp(Rec));
	}
	return true;
}

bool AGTAI_WorldPartitionSetup::ParseSignals(const TSharedPtr<FJsonObject>& Root, GTAI::City::FOSMCityData& Out) const
{
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("signals"), Arr)) return true;

	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject>& Sig = V->AsObject();
		GTAI::City::FOSMTrafficSignal Rec;
		Rec.Id = Sig->GetStringField(TEXT("id"));
		const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
		if (Sig->TryGetArrayField(TEXT("location_local_m"), L))
		{
			Rec.LocationLocalM = FVector2D((float)(*L)[0]->AsNumber(), (float)(*L)[1]->AsNumber());
		}
		Out.Signals.Add(MoveTemp(Rec));
	}
	return true;
}

bool AGTAI_WorldPartitionSetup::ParseDistricts(const TSharedPtr<FJsonObject>& Root, GTAI::City::FOSMCityData& Out) const
{
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Root->TryGetArrayField(TEXT("districts"), Arr)) return true;

	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TSharedPtr<FJsonObject>& D = V->AsObject();
		GTAI::City::FOSMDistrict Rec;
		Rec.Name = D->GetStringField(TEXT("name"));
		const FString OsmTag = D->GetStringField(TEXT("zoning_tag"));
		Rec.ZoningClass = MapZoningClass(OsmTag);
		const TArray<TSharedPtr<FJsonValue>>* Polys = nullptr;
		if (D->TryGetArrayField(TEXT("polygons_local_m"), Polys))
		{
			for (const TSharedPtr<FJsonValue>& Poly : *Polys)
			{
				TArray<FVector2D> Ring;
				for (const TSharedPtr<FJsonValue>& P : Poly->AsArray())
				{
					const TArray<TSharedPtr<FJsonValue>>& C = P->AsArray();
					Ring.Add(FVector2D((float)C[0]->AsNumber(), (float)C[1]->AsNumber()));
				}
				Rec.PolygonsLocalM.Add(MoveTemp(Ring));
			}
		}
		Out.Districts.Add(MoveTemp(Rec));
	}
	return true;
}

// ==========================================================================
// PARTITIONING
// ==========================================================================

void AGTAI_WorldPartitionSetup::ComputeGrid(const GTAI::City::FOSMCityData& City)
{
	// Build a local-metre bounding box from all geometry.
	FBox2D Bounds(ForceInit);
	for (const auto& B : City.Buildings)
		for (const FVector2D& P : B.FootprintLocalM) Bounds += P;
	for (const auto& S : City.Streets)
		for (const FVector2D& P : S.PolylineLocalM) Bounds += P;
	for (const auto& Sig : City.Signals) Bounds += Sig.LocationLocalM;

	// Centre the grid on the city centroid so the central cell is 'gameplay'
	// and always-loaded.
	const FVector2D Centre = Bounds.GetCenter();
	const float CellM = CellSizeM;

	// Grid covers the full extent with the centroid at cell (0,0) centre.
	const float HalfWidthM = (Bounds.Max.X - Centre.X);
	const float HalfHeightM = (Bounds.Max.Y - Centre.Y);
	GridCols = FMath::Max(1, (int32)FMath::CeilToFloat((HalfWidthM * 2.f) / CellM));
	GridRows = FMath::Max(1, (int32)FMath::CeilToFloat((HalfHeightM * 2.f) / CellM));

	// Convert to cm and compute the min corner of cell (0,0).
	// Local metres -> UE cm: scale by ue_scale (cm per metre). X=east, Y=north.
	const float Scale = City.UEScaleCmPerM;
	GridOriginCm = FVector2D(
		Centre.X * Scale - (GridCols * CellM * Scale) * 0.5f,
		Centre.Y * Scale - (GridRows * CellM * Scale) * 0.5f);
	CellSizeCm = CellM * Scale;

	Cells.SetNum(GridCols * GridRows);
	for (int32 r = 0; r < GridRows; ++r)
	{
		for (int32 c = 0; c < GridCols; ++c)
		{
			FCityCell& Cell = Cells[r * GridCols + c];
			Cell.GridCoord = FIntPoint(c, r);
			const FVector2D Min = GridOriginCm + FVector2D(c * CellSizeCm, r * CellSizeCm);
			Cell.Bounds = FBox2D(Min, Min + FVector2D(CellSizeCm, CellSizeCm));
		}
	}
	UE_LOG(LogATLAS_WP, Log, TEXT("Grid: %dx%d cells of %.0f m (%.0f cm), origin=(%.0f,%.0f)"),
		GridCols, GridRows, CellM, CellSizeCm, GridOriginCm.X, GridOriginCm.Y);
}

void AGTAI_WorldPartitionSetup::AssignToCells(const GTAI::City::FOSMCityData& City)
{
	auto CellIndexAt = [&](const FVector2D& LocalM, float Scale) -> int32
	{
		const FVector2D WorldCm = FVector2D(LocalM.X * Scale, LocalM.Y * Scale);
		int32 c = (int32)FMath::FloorToFloat((WorldCm.X - GridOriginCm.X) / CellSizeCm);
		int32 r = (int32)FMath::FloorToFloat((WorldCm.Y - GridOriginCm.Y) / CellSizeCm);
		c = FMath::Clamp(c, 0, GridCols - 1);
		r = FMath::Clamp(r, 0, GridRows - 1);
		return r * GridCols + c;
	};

	const float Scale = City.UEScaleCmPerM;

	for (auto& B : City.Buildings)
	{
		if (B.FootprintLocalM.Num() == 0) continue;
		FVector2D Centroid = FVector2D::ZeroVector;
		for (const FVector2D& P : B.FootprintLocalM) Centroid += P;
		Centroid /= (float)B.FootprintLocalM.Num();
		const int32 Idx = CellIndexAt(Centroid, Scale);
		B.HeightM = FMath::Min(B.HeightM, ApplyZoningHeight(B, City.Districts));
		Cells[Idx].Buildings.Add(B);
	}

	for (auto& S : City.Streets)
	{
		if (S.PolylineLocalM.Num() == 0) continue;
		FVector2D Mid = S.PolylineLocalM[S.PolylineLocalM.Num() / 2];
		const int32 Idx = CellIndexAt(Mid, Scale);
		Cells[Idx].Streets.Add(S);
	}

	for (auto& Sig : City.Signals)
	{
		const int32 Idx = CellIndexAt(Sig.LocationLocalM, Scale);
		Cells[Idx].Signals.Add(Sig);
	}

	int32 TotalB = 0, TotalS = 0, TotalSig = 0;
	for (auto& Cell : Cells) { TotalB += Cell.Buildings.Num(); TotalS += Cell.Streets.Num(); TotalSig += Cell.Signals.Num(); }
	UE_LOG(LogATLAS_WP, Log, TEXT("Bucketed %d buildings, %d streets, %d signals across %d cells"),
		TotalB, TotalS, TotalSig, Cells.Num());
}

void AGTAI_WorldPartitionSetup::AssignDataLayers()
{
	const FIntPoint CentreCell(GridCols / 2, GridRows / 2);
	for (auto& Cell : Cells)
	{
		const int32 Dist = FMath::Max(
			FMath::Abs(Cell.GridCoord.X - CentreCell.X),
			FMath::Abs(Cell.GridCoord.Y - CentreCell.Y));
		Cell.bIsAlwaysLoaded = (Dist == 0); // central cell always resident
		Cell.DataLayerName = (Dist <= GameplayRadiusCells) ? TEXT("ATLAS_Gameplay") : TEXT("ATLAS_Vista");
	}
}

// ==========================================================================
// PUBLIC ACTIONS
// ==========================================================================

void AGTAI_WorldPartitionSetup::ImportAndPartition()
{
	Cells.Empty();
	GTAI::City::FOSMCityData City;
	if (!LoadCityData(CityJsonPath.FilePath, City)) return;
	LoadedScaleCmPerM = City.UEScaleCmPerM;

	ComputeGrid(City);
	AssignToCells(City);
	AssignDataLayers();

	UE_LOG(LogATLAS_WP, Log, TEXT("Partition complete: %d cells. Run BakeCellMeshes() to build geometry."), Cells.Num());
}

#if WITH_EDITOR
void AGTAI_WorldPartitionSetup::BakeCellMeshes()
{
	if (Cells.Num() == 0)
	{
		UE_LOG(LogATLAS_WP, Warning, TEXT("Call ImportAndPartition() first."));
		return;
	}
	int32 Baked = 0;
	for (const auto& Cell : Cells)
	{
		if (Cell.Buildings.Num() == 0) continue;
		if (BakeCell(Cell)) ++Baked;
	}
	UE_LOG(LogATLAS_WP, Log, TEXT("Baked %d/%d non-empty cells into Nanite static meshes."), Baked, Cells.Num());
}

void AGTAI_WorldPartitionSetup::BuildCity()
{
	ImportAndPartition();
	BakeCellMeshes();
}
#else
void AGTAI_WorldPartitionSetup::BakeCellMeshes() {}
void AGTAI_WorldPartitionSetup::BuildCity() { ImportAndPartition(); }
#endif

// ==========================================================================
// BAKING  (editor-only)
// ==========================================================================

#if WITH_EDITOR
bool AGTAI_WorldPartitionSetup::BakeCell(const GTAI::City::FCityCell& Cell)
{
	UStaticMesh* Mesh = BuildMergedBuildingMesh(Cell);
	if (!Mesh) return false;

	FActorSpawnParameters Sp;
	Sp.bDeferConstruction = false;
	AStaticMeshActor* SMA = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform::Identity, Sp);
	if (!SMA) return false;

	SMA->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	SMA->SetActorLabel(FString::Printf(TEXT("ATLAS_Cell_%d_%d"), Cell.GridCoord.X, Cell.GridCoord.Y));

	// --- World Partition: assign this actor to its streaming cell ---
	if (UWorldPartition* WP = GetWP())
	{
		// The actor's world location determines its grid cell. Place at the
		// cell centroid so WP's spatial hash routes it correctly.
		const FVector2D Cen = CellCentroidCm(Cell.GridCoord);
		SMA->SetActorLocation(FVector(Cen.X, Cen.Y, 0.f));
		WP->AddActor(SMA, true); // true = assign to runtime spatial hash cell
	}

	// --- Data Layer assignment (gameplay vs vista) ---
	if (Cell.DataLayer)
	{
		SMA->AddDataLayer(Cell.DataLayer);
	}
	return true;
}

UStaticMesh* AGTAI_WorldPartitionSetup::BuildMergedBuildingMesh(const GTAI::City::FCityCell& Cell) const
{
	// Build one FMeshDescription merging every building footprint extruded
	// to its (zoning-clamped) height. Nanite is enabled on the asset.
	FMeshDescription MeshDesc;
	FStaticMeshAttributes Attributes(MeshDesc);
	Attributes.Register();

	auto AddExtrudedBuilding = [&](const GTAI::City::FOSMBuilding& B)
	{
		const int32 N = B.FootprintLocalM.Num();
		if (N < 3) return;
		const float H = FMath::Max(B.HeightUE, 300.f); // >=3m min height, in cm

		TArray<FVertexID> Bottom, Top;
		Bottom.Reserve(N); Top.Reserve(N);
		for (int32 i = 0; i < N; ++i)
		{
			const FVector2D& P = B.FootprintLocalM[i];
			// local tangent-plane metres -> UE world cm. X=east, Y=north, Z=up.
			const FVector2D WorldCm(P.X * LoadedScaleCmPerM, P.Y * LoadedScaleCmPerM);
			Bottom.Add(MeshDesc.CreateVertex());
			Top.Add(MeshDesc.CreateVertex());
			Attributes.GetVertexPositions()[Bottom[i]] = FVector(WorldCm.X, WorldCm.Y, 0.f);
			Attributes.GetVertexPositions()[Top[i]] = FVector(WorldCm.X, WorldCm.Y, H);
		}
		// Side quads (CCW seen from outside)
		for (int32 i = 0; i < N; ++i)
		{
			const int32 j = (i + 1) % N;
			FPolygonID Poly = MeshDesc.CreatePolygon();
			MeshDesc.CreatePolygonVertex(Poly, Bottom[i]);
			MeshDesc.CreatePolygonVertex(Poly, Bottom[j]);
			MeshDesc.CreatePolygonVertex(Poly, Top[j]);
			MeshDesc.CreatePolygonVertex(Poly, Top[i]);
		}
		// Top cap as a triangle fan around vertex 0
		for (int32 i = 1; i < N - 1; ++i)
		{
			FPolygonID Poly = MeshDesc.CreatePolygon();
			MeshDesc.CreatePolygonVertex(Poly, Top[0]);
			MeshDesc.CreatePolygonVertex(Poly, Top[i]);
			MeshDesc.CreatePolygonVertex(Poly, Top[i + 1]);
		}
		// Bottom cap (reverse winding)
		for (int32 i = 1; i < N - 1; ++i)
		{
			FPolygonID Poly = MeshDesc.CreatePolygon();
			MeshDesc.CreatePolygonVertex(Poly, Bottom[0]);
			MeshDesc.CreatePolygonVertex(Poly, Bottom[i + 1]);
			MeshDesc.CreatePolygonVertex(Poly, Bottom[i]);
		}
	};

	for (const auto& B : Cell.Buildings) AddExtrudedBuilding(B);

	// Create the asset
	const FString PkgName = FString::Printf(TEXT("/Game/City/Meshes/ATLAS_Cell_%d_%d"), Cell.GridCoord.X, Cell.GridCoord.Y);
	UPackage* Pkg = CreatePackage(*PkgName);
	if (!Pkg) return nullptr;
	UStaticMesh* Mesh = NewObject<UStaticMesh>(Pkg, FName(*(TEXT("SM_ATLAS_Cell_") + FString::FromInt(Cell.GridCoord.X) + TEXT("_") + FString::FromInt(Cell.GridCoord.Y))), RF_Public | RF_Standalone);
	Mesh->SetNumSourceModels(1);
	Mesh->CreateMeshDescription(0, MoveTemp(MeshDesc));
	Mesh->CommitMeshDescription(0);

	// Nanite
	FMeshBuildSettings& BS = Mesh->GetSourceModel(0).BuildSettings;
	BS.bBuildAdjacencyBuffer = false;
	BS.bRemoveDegenerates = true;
	Mesh->NaniteSettings.bEnabled = bEnableNanite;

	if (BuildingMassMaterial.IsValid())
		if (UMaterialInterface* Mat = BuildingMassMaterial.LoadSynchronous())
			Mesh->SetMaterial(0, Mat);

	Mesh->Build(true);
	Mesh->MarkPackageDirty();
	return Mesh;
}
#endif

// ==========================================================================
// ZONING
// ==========================================================================

float AGTAI_WorldPartitionSetup::ApplyZoningHeight(const GTAI::City::FOSMBuilding& B, const TArray<GTAI::City::FOSMDistrict>& Districts)
{
	float Cap = 250.f;
	for (const auto& D : Districts)
	{
		// point-in-polygon test against the district's outer ring
		bool Inside = false;
		if (D.PolygonsLocalM.Num() == 0) continue;
		const TArray<FVector2D>& Ring = D.PolygonsLocalM[0];
		if (B.FootprintLocalM.Num() == 0) continue;
		const FVector2D Pt = B.FootprintLocalM[0];
		for (int32 i = 0, j = Ring.Num() - 1; i < Ring.Num(); j = i++)
		{
			if (((Ring[i].Y > Pt.Y) != (Ring[j].Y > Pt.Y)) &&
				(Pt.X < (Ring[j].X - Ring[i].X) * (Pt.Y - Ring[i].Y) / (Ring[j].Y - Ring[i].Y) + Ring[i].X))
				Inside = !Inside;
		}
		if (Inside) { Cap = GTAI::City::Zoning::MaxHeightM(D.ZoningClass); break; }
	}
	return FMath::Min(B.HeightM, Cap);
}

FString AGTAI_WorldPartitionSetup::MapZoningClass(const FString& OsmTag)
{
	// Map raw OSM landuse/place tags to NYC-style zoning buckets.
	if (OsmTag.Contains(TEXT("park")) || OsmTag.Contains(TEXT("forest")) ||
		OsmTag.Contains(TEXT("grass")) || OsmTag.Contains(TEXT("cemetery")))
		return TEXT("PARK");
	if (OsmTag.Contains(TEXT("industrial")) || OsmTag.Contains(TEXT("commercial")) ||
		OsmTag.Contains(TEXT("retail")))
		return TEXT("C6");
	if (OsmTag.Contains(TEXT("residential")))
		return TEXT("R8");
	return TEXT("C6"); // default dense urban commercial/residential mix
}

// ==========================================================================
// WP PLUMBING + RUNTIME QUERIES
// ==========================================================================

UWorldPartition* AGTAI_WorldPartitionSetup::GetWP() const
{
	return GetWorld() ? GetWorld()->GetWorldPartition() : nullptr;
}

UWorldPartitionRuntimeSpatialHash* AGTAI_WorldPartitionSetup::GetRuntimeHash() const
{
#if WITH_EDITOR
	return nullptr; // editor uses editor hash; runtime hash only valid in cooked build
#else
	if (UWorldPartition* WP = GetWP())
		return Cast<UWorldPartitionRuntimeSpatialHash>(WP->RuntimeHash);
	return nullptr;
#endif
}

FIntPoint AGTAI_WorldPartitionSetup::WorldToCell(FVector WorldPosCm) const
{
	int32 c = (int32)FMath::FloorToFloat((WorldPosCm.X - GridOriginCm.X) / CellSizeCm);
	int32 r = (int32)FMath::FloorToFloat((WorldPosCm.Y - GridOriginCm.Y) / CellSizeCm);
	c = FMath::Clamp(c, 0, FMath::Max(0, GridCols - 1));
	r = FMath::Clamp(r, 0, FMath::Max(0, GridRows - 1));
	return FIntPoint(c, r);
}

FVector2D AGTAI_WorldPartitionSetup::CellCentroidCm(const FIntPoint& Cell) const
{
	return GridOriginCm + FVector2D((Cell.X + 0.5f) * CellSizeCm, (Cell.Y + 0.5f) * CellSizeCm);
}
