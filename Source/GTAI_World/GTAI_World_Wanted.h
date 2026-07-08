// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GTAI_World_Wanted.generated.h"

class UGTAI_WorldStateManager;

namespace GTAI::World
{
	/** Wanted star level, clamped [0,5]. 0 = clean. */
	enum class EWantedLevel : uint8
	{
		Clean   = 0,
		OneStar = 1,
		TwoStar = 2,
		ThreeStar = 3,
		FourStar = 4,
		FiveStar = 5,
	};

	/** Severity buckets for a committed crime (drives deterministic star assignment). */
	enum class ECrimeSeverity : uint8
	{
		Minor,      // punch, petty theft
		Moderate,   // car theft, assault w/ weapon
		Major,      // killing civilian
		Capital,    // killing officer, mass violence
	};

	/** One police dispatch request produced by the deterministic core. */
	struct GTAI_WORLD_API FDispatchRequest
	{
		EWantedLevel Stars = EWantedLevel::Clean;
		FVector Location = FVector::ZeroVector;
		int32 SourceCellIndex = INDEX_NONE;
		int32 PatrolCars = 0;
		int32 Unmarked = 0;
		int32 Helicopters = 0;
		int32 NOOSE = 0;
		bool bRoadblockCapable = false;
	};

	/**
	 * Deterministic core of the wanted system.
	 * Pure functions of (severity, witness, prior stars, heat). No RNG in transitions.
	 * AI police tactics live in the BT/EQS layer (see PoliceTactics in .cpp), not here.
	 */
	class GTAI_WORLD_API FWantedCore
	{
	public:
		EWantedLevel GetStars() const { return CurrentStars; }

		/** Apply a witnessed/unwitnessed crime. Returns the new star level. */
		EWantedLevel CommitCrime(ECrimeSeverity Severity, bool bWitnessed, int32 CellIndex);

		/** Timer/distance based de-escalation. Called by the system tick. */
		void TickDeescalation(float SimDt, bool bAnyCopHasLOS, float DistToNearestCop);

		/** Attempt a bribe. Succeeds only if no cop currently has line of sight. */
		bool TryBribe(float AvailableCash, bool bAnyCopHasLOS, float& OutCost);

		/** Player entered a hidden state (interior / safehouse / no LOS for T_hidden). */
		void SetHidden(bool bHidden) { bIsHidden = bHidden; }

		/** Reset after arrest / bribe success. */
		void Clear() { CurrentStars = EWantedLevel::Clean; CooldownTimer = 0.f; }

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStarsChanged, EWantedLevel /*Old*/, EWantedLevel /*New*/);
		FOnStarsChanged OnStarsChanged;

	private:
		EWantedLevel CurrentStars = EWantedLevel::Clean;
		float CooldownTimer = 0.f;
		bool bIsHidden = false;

		static EWantedLevel SeverityToStars(ECrimeSeverity S);
		static float SeverityToHeat(ECrimeSeverity S);
	};

	/** DataTable row: stars -> unit composition (designer-tuned, no recompile). */
	struct GTAI_WORLD_API FDispatchRow : public FTableRowBase
	{
		GENERATED_BODY()
		UPROPERTY(EditAnywhere) int32 PatrolCars = 1;
		UPROPERTY(EditAnywhere) int32 Unmarked = 0;
		UPROPERTY(EditAnywhere) int32 Helicopters = 0;
		UPROPERTY(EditAnywhere) int32 NOOSE = 0;
		UPROPERTY(EditAnywhere) bool bRoadblockCapable = false;
	};
}

UCLASS()
class GTAI_WORLD_API UGTAI_WorldWantedSystem : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UGTAI_WorldStateManager* InWorld);
	void SimTick(float SimDt, const FVector& PlayerLocation, bool bAnyCopHasLOS,
	             float DistToNearestCop);

	/** Player committed a crime. */
	void ReportCrime(GTAI::World::ECrimeSeverity Severity, bool bWitnessed,
	                 const FVector& Location);

	GTAI::World::EWantedLevel GetStars() const { return Core.GetStars(); }

	/** Build a deterministic dispatch request from current stars + influence map. */
	GTAI::World::FDispatchRequest BuildDispatch(const FVector& Location) const;

	/** AI tactical hooks (implemented in .cpp against BehaviorTree + EQS). */
	void RunPoliceTactics(float SimDt);

	UPROPERTY(EditDefaultsOnly, Category = "Wanted")
	TSoftObjectPtr<UDataTable> DispatchTable;

	UPROPERTY(EditDefaultsOnly, Category = "Wanted")
	float BribeBase = 500.f;

protected:
	GTAI::World::FWantedCore Core;
	TWeakObjectPtr<UGTAI_WorldStateManager> World;

	/** Per-star cooldown (sec) before a star can drop — fed by a CurveTable at runtime. */
	TStaticArray<float, 6> CooldownByStar{ 8.f, 12.f, 18.f, 26.f, 36.f, 50.f };
	/** Per-star evade radius (world units) the player must exceed to de-escalate. */
	TStaticArray<float, 6> EvadeRadiusByStar{ 1500.f, 2500.f, 4000.f, 6000.f, 9000.f, 14000.f };
};
