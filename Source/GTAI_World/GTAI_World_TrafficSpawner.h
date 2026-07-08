// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GTAI_World_TrafficSpawner.generated.h"

class UGTAI_WorldStateManager;

namespace GTAI::World
{
	/** Phase of a single intersection signal. */
	enum class ESignalPhase : uint8
	{
		NorthSouthGreen,
		NorthSouthYellow,
		EastWestGreen,
		EastWestYellow,
		AllRed,        // clearance
	};

	/** A baked parking / idle slot anchor for traffic vehicles. */
	struct GTAI_WORLD_API FParkingSlot
	{
		FVector Location = FVector::ZeroVector;
		FRotator Heading = FRotator::ZeroRotator;
		bool bOccupied = false;
		int32 CellIndex = INDEX_NONE;
	};

	/**
	 * Deterministic traffic spawner.
	 * Spawn budget per World Partition cell = BaseDensity * TimeOfDayMult * DistrictMult * WeatherMult,
	 * clamped to a hard cap. Arrivals are a seeded Poisson process over (cellID, timeSlot) so the
	 * city is reproducible and never cross-budget. NO LLM, no per-frame RNG churn.
	 */
	class GTAI_WORLD_API FTrafficSpawnModel
	{
	public:
		/** Returns how many vehicles should be alive in this cell right now. */
		int32 ComputeBudget(int32 CellIndex, float TimeOfDay01, float WeatherMult,
		                    FRandomStream& Rng) const;

		/** Next arrival delay (sec) drawn from the cell's Poisson rate. */
		float NextArrivalDelay(int32 CellIndex, FRandomStream& Rng) const;

		DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSpawnRequest, int32 /*CellIndex*/, int32 /*Count*/, bool /*bParked*/);
		FOnSpawnRequest OnSpawnRequest;

	private:
		float BaseDensity = 12.f;     // vehicles per active cell at peak
		float PoissonLambda = 0.4f;   // arrivals/sec at peak
	};

	/** DataTable row: per-district spawn tuning. */
	struct GTAI_WORLD_API FTrafficDensityRow : public FTableRowBase
	{
		GENERATED_BODY()
		UPROPERTY(EditAnywhere) float BaseDensity = 12.f;
		UPROPERTY(EditAnywhere) float DistrictMult = 1.f;
		UPROPERTY(EditAnywhere) float PeakHour = 0.5f; // time-of-day [0,1] of max density
	};
}

UCLASS()
class GTAI_WORLD_API UGTAI_WorldTrafficSpawner : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UGTAI_WorldStateManager* InWorld);
	void SimTick(float SimDt, float TimeOfDay01, float WeatherMult);

	/** Route AI: pick next graph edge from current node (event-driven at intersections). */
	int32 PickNextRouteNode(int32 CurrentNode, FRandomStream& Rng) const;

	/** Signal controller: advance all intersection state machines. */
	void UpdateSignals(float SimDt);

	/** Preempt an intersection axis for an emergency/police unit. */
	void RequestSignalPreempt(int32 IntersectionId, bool bNorthSouth);

	UPROPERTY(EditDefaultsOnly, Category = "Traffic")
	TSoftObjectPtr<UDataTable> DensityTable;

	UPROPERTY(EditDefaultsOnly, Category = "Traffic|Budgets")
	int32 HardVehicleCap = 50;

protected:
	GTAI::World::FTrafficSpawnModel SpawnModel;
	TWeakObjectPtr<UGTAI_WorldStateManager> World;

	/** Active intersection phases. Key = intersection graph id. */
	TMap<int32, GTAI::World::ESignalPhase> SignalPhases;
	TMap<int32, float> SignalTimers;
	TSet<int32> PreemptedIntersections;

	/** Pedestrian crosswalk yield test: returns true if a ped is in the crosswalk box. */
	bool IsCrosswalkBlocked(int32 IntersectionId) const;
};
