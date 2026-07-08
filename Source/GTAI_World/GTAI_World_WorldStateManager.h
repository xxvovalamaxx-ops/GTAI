// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_World_WorldStateManager.generated.h"

class UGTAI_WorldWantedSystem;
class UGTAI_WorldTrafficSpawner;
class UGTAI_WorldEconomySystem;
class UGTAI_WorldFactionSystem;
class UGTAI_WorldReactivitySystem;

namespace GTAI::World
{
	/** A single spatial simulation cell, aligned to a World Partition grid cell. */
	struct GTAI_WORLD_API FWorldCell
	{
		FIntVector2 GridCoord = FIntVector2::ZeroValue;
		FVector Center = FVector::ZeroVector;

		/** [0,1] Recent-crime heat; drives police dispatch origin + response delay. */
		float Heat = 0.f;
		/** [0,1] per-faction influence; index = faction id. Diffused each tick. */
		TArray<float> FactionInfluence;
		/** Normalized property / real-estate value for the cell. */
		float PropertyValue = 1.f;
		/** Bitmask of which gameplay systems consider this cell "active" (streamed in). */
		uint8 bActive : 1 = 0;
	};

	/** Lightweight event sent on the world event bus (no payload heap alloc). */
	struct GTAI_WORLD_API FWorldEvent
	{
		enum class EType : uint8
		{
			CrimeCommitted,
			WantedChanged,
			FactionWarStarted,
			BusinessClosed,
			MarketShock,
			TerritoryShift,
		};
		EType Type = EType::CrimeCommitted;
		int32 CellIndex = INDEX_NONE;
		int32 FactionA = INDEX_NONE;
		int32 FactionB = INDEX_NONE;
		float Magnitude = 0.f;
	};
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAI_OnWorldEvent, const GTAI::World::FWorldEvent&, Event);

/**
 * UGTAI_WorldStateManager
 *
 * Central singleton (UE GameInstance Subsystem) tracking all world systems.
 * - Owns the shared spatial fields (heat / faction influence / property) aligned to
 *   World Partition grid cells so every system reads/writes one source of truth.
 * - Registers and ticks every world system at a fixed simulation rate (decoupled from render).
 * - Provides a lightweight event bus so systems react without hard coupling.
 *
 * Designer tuning lives in DataTables/CurveTables; this class is deterministic C++ only.
 */
UCLASS()
class GTAI_WORLD_API UGTAI_WorldStateManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// --- Subsystem lifecycle ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Fixed-rate simulation tick. Called by the world game mode / a dedicated sim ticker. */
	void SimTick(float SimDt);

	// --- Accessors (singleton pattern via subsystem) ---
	static UGTAI_WorldStateManager* Get(const UObject* WorldContext);

	// --- Spatial grid ---
	/** Map a world location to its cell index (O(1) for our uniform grid). */
	int32 LocationToCellIndex(const FVector& WorldLocation) const;
	GTAI::World::FWorldCell& GetCell(int32 CellIndex);
	const GTAI::World::FWorldCell& GetCell(int32 CellIndex) const;

	/** Influence-weighted selection of a source cell (e.g. police precinct spawn). */
	int32 SelectInfluenceWeightedCell(int32 FactionId, const FVector& NearLocation,
	                                  FRandomStream& Rng) const;

	// --- Event bus ---
	/** Broadcast a world event to all listeners (Economy listens for MarketShock, etc.). */
	void BroadcastEvent(const GTAI::World::FWorldEvent& Event);
	UPROPERTY(BlueprintAssignable, Category = "World")
	FGTAI_OnWorldEvent OnWorldEvent;

	// --- System registry (set during Initialize / by owning systems) ---
	UPROPERTY()
	TObjectPtr<UGTAI_WorldWantedSystem> WantedSystem = nullptr;
	UPROPERTY()
	TObjectPtr<UGTAI_WorldTrafficSpawner> TrafficSpawner = nullptr;
	UPROPERTY()
	TObjectPtr<UGTAI_WorldEconomySystem> EconomySystem = nullptr;
	UPROPERTY()
	TObjectPtr<UGTAI_WorldFactionSystem> FactionSystem = nullptr;
	UPROPERTY()
	TObjectPtr<UGTAI_WorldReactivitySystem> ReactivitySystem = nullptr;

protected:
	/** All live cells. Indexed by a hash of GridCoord. */
	TArray<GTAI::World::FWorldCell> Cells;

	/** Decay heat and diffuse faction influence. Cheap box-blur over the grid. */
	void UpdateSpatialFields(float SimDt);

	/** Hard performance ceiling guard: never let pooled actors exceed budget. */
	void EnforceActorBudgets() const;

	UPROPERTY(EditDefaultsOnly, Category = "World|Sim")
	float HeatDecayPerSec = 0.02f;

	UPROPERTY(EditDefaultsOnly, Category = "World|Sim")
	float InfluenceDiffusionRate = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "World|Budgets")
	int32 MaxActiveNPCs = 200;

	UPROPERTY(EditDefaultsOnly, Category = "World|Budgets")
	int32 MaxActiveVehicles = 50;
};
