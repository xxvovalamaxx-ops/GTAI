// GTPedestrianSchedule.h
// Drives 100+ pedestrians along daily routines anchored to city locations.
// Evaluated in round-robin chunks to stay within the frame budget.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "Schedule/GTPedestrianTypes.h"

namespace GTAI::NPC
{
	// Resolves a LocationTag to a world position (gameplay-layer implementation).
	class IScheduleLocationResolver
	{
	public:
		virtual ~IScheduleLocationResolver() = default;
		virtual FVector Resolve(FLocationTag Tag) const = 0;
	};

	class GTAI_NPC_API FPedestrianScheduleSystem
	{
	public:
		FPedestrianScheduleSystem(IScheduleLocationResolver& Resolver)
			: LocResolver(Resolver) {}

		// Register a pedestrian with its procedural schedule.
		void Register(FNPCId NPC, const FSchedule& Plan);

		// Call on a city-time tick (every 30 in-game minutes).
		// Evaluates up to MaxPerTick entities (round-robin) and updates fragments.
		void Tick(float CityTimeOfDayMinutes, int32 MaxPerTick = 25);

		const TMap<FNPCId, FPedestrianFragment>& GetFragments() const { return Fragments; }
		FPedestrianFragment* Find(FNPCId NPC) { return Fragments.Find(NPC); }

		// City-event override entry points used by the crowd system.
		void PushTemporaryGoal(FNPCId NPC, const FVector& Goal, EPedestrianState Override);
		void ClearTemporaryGoal(FNPCId NPC);

	private:
		IScheduleLocationResolver* LocResolver = nullptr;
		TMap<FNPCId, FSchedule> Plans;
		TMap<FNPCId, FPedestrianFragment> Fragments;
		TMap<FNPCId, FVector> TempGoals;            // crowd overrides
		TMap<FNPCId, EPedestrianState> TempStates;
		TArray<FNPCId> Order;                        // round-robin cursor
		int32 Cursor = 0;
	};
}
