// GTPedestrianTypes.h
// MassEntity-friendly plain types for the pedestrian schedule system.
// (No UObject per pedestrian; these back MassEntity fragments.)
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"

namespace GTAI::NPC
{
	// One anchored routine entry.
	struct FScheduleSlot
	{
		float TimeOfDayMinutes = 0.f;   // 0..1440 (minutes from midnight)
		FLocationTag Location;           // anchor in the city
		FActivityTag Activity;           // "work", "shop", "loiter", "sleep"
		uint8 Priority = 1;              // higher overrides flexible slots
		bool bFlexible = false;          // resolved at runtime from city state
	};

	// A pedestrian's daily plan (procedural + seeded for determinism).
	struct FSchedule
	{
		FNPCId Owner = 0;
		TArray<FScheduleSlot> Slots;     // sorted by TimeOfDayMinutes
		uint32 Seed = 0;                 // deterministic regeneration on load

		const FScheduleSlot* ActiveSlot(float TimeOfDayMinutes) const;
		const FScheduleSlot* NextSlot(float TimeOfDayMinutes) const;
	};

	// Runtime movement/state fragment for a single pedestrian entity.
	enum class EPedestrianState : uint8
	{
		Idle,
		Moving,        // traveling to a schedule slot
		Occupied,      // performing an activity at a slot
		Curious,       // diverted by a point of interest
		Fleeing,       // panic override
		Mobbing        // mob formation override
	};

	struct FPedestrianFragment
	{
		FNPCId NPC = 0;
		EPedestrianState State = EPedestrianState::Idle;
		FVector GoalLocation = FVector::ZeroVector;
		FActivityTag CurrentActivity;
		float PersonalSpace = 60.f;      // cm; scales with fear
		uint8 CrowdFlags = 0;            // bitfield: bPanic, bCurious, bInMob
	};
}
