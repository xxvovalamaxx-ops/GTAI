// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

#include "GTAI_WorldPartitionSetup.generated.h"

class UWorldPartitionRuntimeSpatialHash;

// --------------------------------------------------------------------------
// Namespace: shared data types for the OSM -> World Partition pipeline.
// --------------------------------------------------------------------------
namespace GTAI::City
{
	/** Per-building OSM record (mirrors Tools/CityGenerator/osm_to_json.py). */
	struct GTAI_WORLD_API FOSMBuilding
	{
		FString Id;
		FString Name;
		FString Use;            // commercial / residential / industrial / civic
		float   HeightM = 0.f;  // estimated real-world height
		float   HeightUE = 0.f; // = HeightM * ue_scale_cm_per_m
		int32   Floors = 0;
		// Footprint ring in local tangent-plane metres [east, north] relative
		// to the JSON `anchor`. (UE X=east, Y=north, Z=up.)
		TArray<FVector2D> FootprintLocalM;
	};

	/** A street polyline + traffic metadata. */
	struct GTAI_WORLD_API FOSMStreet
	{
		FString Id;
		FString Name;
		FString Highway;        // primary / secondary / residential / ...
		float   WidthM = 0.f;
		int32   Lanes = 0;
		bool    bOneWay = false;
		TArray<FVector2D> PolylineLocalM;
	};

	/** A traffic signal node from OSM (highway=traffic_signals). */
	struct GTAI_WORLD_API FOSMTrafficSignal
	{
		FString Id;
		FVector2D LocationLocalM = FVector2D::ZeroVector;
		// Phase the importer should give this node, derived from adjacent
		// street orientation (see ComputeSignalPhase).
		uint8    SuggestedPhase = 0;
	};

	/** A district polygon + zoning class for height enforcement. */
	struct GTAI_WORLD_API FOSMDistrict
	{
		FString Name;
		FString ZoningClass;    // R8 / C6 / M1 / PARK / ... (mapped from OSM tags)
		TArray<TArray<FVector2D>> PolygonsLocalM; // outer + holes
	};

	/** One streaming cell: the unit of World Partition streaming + mesh bake. */
	struct GTAI_WORLD_API FCityCell
	{
		FIntPoint GridCoord = FIntPoint::NoneValue; // (col, row) in the WP grid
		FBox2D    Bounds;                            // world X/Y extent (cm)
		TArray<FOSMBuilding>        Buildings;
		TArray<FOSMStreet>          Streets;
		TArray<FOSMTrafficSignal>   Signals;
		FString    DataLayerName;                   // gameplay / vista / water
		bool       bIsAlwaysLoaded = false;         // central cell
		TObjectPtr<UDataLayerAsset> DataLayer;      // resolved at bake time
	};

	/** Parsed top-level city document. */
	struct GTAI_WORLD_API FOSMCityData
	{
		FString   Attribution;
		double    AnchorLat = 0.0;
		double    AnchorLon = 0.0;
		float     UEScaleCmPerM = 100.f;
		FBox2D    BoundsLocalM;       // overall extent in metres
		TArray<FOSMBuilding>      Buildings;
		TArray<FOSMStreet>        Streets;
		TArray<FOSMTrafficSignal> Signals;
		TArray<FOSMDistrict>      Districts;
		int32     StatsBuildings = 0;
		int32     StatsStreets = 0;
	};
}

// --------------------------------------------------------------------------
// UGTAI_WorldPartitionSetup
//
// Editor-time actor that reads the OSM city JSON produced by
// Tools/CityGenerator/osm_to_json.py and:
//   1. Splits the world into World Partition streaming cells of a fixed size.
//   2. Buckets every building / street / signal into the cell that contains
//      its centroid.
//   3. Assigns a Data Layer per cell (gameplay vs vista by distance from centre).
//   4. Bakes one merged Nanite-enabled StaticMesh per cell (footprint extrusion)
//      so the city is both streamed AND geometric.
//
// This is the "split into streaming cells" stage of the ATLAS pipeline.
// It runs in-editor (editor module only) so the resulting .umap + OFPA actors
// cook into the packaged build.
// --------------------------------------------------------------------------
UCLASS(NotBlueprintable, meta = (ToolMenu = "Atlas|WorldPartition"))
class GTAI_WORLD_API AGTAI_WorldPartitionSetup : public AActor
{
	GENERATED_BODY()

public:
	AGTAI_WorldPartitionSetup();

	// ---- Configuration (edit in Details panel) -------------------------

	/** Path to the osm_to_json.py output (manhattan_slice.json). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Import")
	FFilePath CityJsonPath;

	/** World Partition cell edge length in METRES (doc says 500m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Import", meta = (ClampMin = "50", ClampMax = "4096"))
	float CellSizeM = 500.f;

	/** Cells within this ring of the centre are 'gameplay', outside are 'vista'. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Import", meta = (ClampMin = "0", ClampMax = "32"))
	int32 GameplayRadiusCells = 2;

	/** Enable Nanite on every baked per-cell mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Mesh")
	bool bEnableNanite = true;

	/** Merge all buildings of a cell into one mesh (fewer draw calls). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Mesh")
	bool bMergeBuildingsPerCell = true;

	/** Material applied to extruded building masses (overridden by FORGE later). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Atlas|Mesh")
	TSoftObjectPtr<UMaterialInterface> BuildingMassMaterial;

	// ---- Actions --------------------------------------------------------

	/** Load JSON, split into cells, assign data layers. No baking. */
	UFUNCTION(CallInEditor, Category = "Atlas|Actions")
	void ImportAndPartition();

	/** Bake geometry (Nanite meshes) for every cell after ImportAndPartition. */
	UFUNCTION(CallInEditor, Category = "Atlas|Actions")
	void BakeCellMeshes();

	/** Convenience: import + bake in one call. */
	UFUNCTION(CallInEditor, Category = "Atlas|Actions")
	void BuildCity();

	// ---- Queries --------------------------------------------------------

	/** Number of cells produced by the last partition. */
	UFUNCTION(BlueprintPure, Category = "Atlas|Query")
	int32 GetCellCount() const { return Cells.Num(); }

	/** World Partition grid origin (cm) — bottom-left corner of cell (0,0). */
	const FVector2D& GetGridOriginCm() const { return GridOriginCm; }

	// ---- Runtime helpers (used by traffic/world-state systems) ----------

	/** Map a world X/Y position (cm) to the streaming cell that owns it. */
	UFUNCTION(BlueprintPure, Category = "Atlas|Query")
	FIntPoint WorldToCell(FVector WorldPosCm) const;

	/** Centroid of a cell in world cm. */
	FVector2D CellCentroidCm(const FIntPoint& Cell) const;

protected:
	virtual void BeginPlay() override;

private:
	// --- JSON parsing (mirrors osm_to_json.py schema) ---
	bool LoadCityData(const FString& Path, GTAI::City::FOSMCityData& Out) const;
	bool ParseBuildings(const TSharedPtr<FJsonObject>& Root, GTAI::City::FOSMCityData& Out) const;
	bool ParseStreets(const TSharedPtr<FJsonObject>& Root, GTAI::City::FOSMCityData& Out) const;
	bool ParseSignals(const TSharedPtr<FJsonObject>& Root, GTAI::City::FOSMCityData& Out) const;
	bool ParseDistricts(const TSharedPtr<FJsonObject>& Root, GTAI::City::FOSMCityData& Out) const;

	// --- Partitioning ---
	void ComputeGrid(const GTAI::City::FOSMCityData& City);
	void AssignToCells(const GTAI::City::FOSMCityData& City);
	void AssignDataLayers();

	// --- Baking ---
	bool BakeCell(const GTAI::City::FCityCell& Cell);
	UStaticMesh* BuildMergedBuildingMesh(const GTAI::City::FCityCell& Cell) const;

	// --- Zoning (NYC height law enforcement) ---
	/** Clamp a building height to the district's zoning max (R8=85m, C6=200m, ...). */
	static float ApplyZoningHeight(const GTAI::City::FOSMBuilding& B, const TArray<GTAI::City::FOSMDistrict>& Districts);
	/** Map an OSM district/landuse tag to a NYC-style zoning class. */
	static FString MapZoningClass(const FString& OsmTag);

	// --- WP plumbing ---
	UWorldPartition* GetWP() const;
	UWorldPartitionRuntimeSpatialHash* GetRuntimeHash() const;

	// --- State ---
	TArray<GTAI::City::FCityCell> Cells;
	FVector2D GridOriginCm = FVector2D::ZeroVector; // cm, world X/Y of cell (0,0) min
	float     CellSizeCm = 50000.f;                 // CellSizeM * 100
	FIntPoint GridMin = FIntPoint::ZeroValue;       // grid coord of cell (0)
	int32     GridCols = 0;
	int32     GridRows = 0;
	float     LoadedScaleCmPerM = 100.f;            // ue_scale from the JSON
};
