// GTNPCDefines.h
// Common types, enums and small helpers shared across GTAI::NPC systems.
#pragma once

#include "CoreMinimal.h"

namespace GTAI::NPC
{
	// Unique, stable identifier for an NPC (pedestrian, named character, or advisor).
	using FNPCId = uint64;

	// Identifier for a city location / anchor used by schedules and crowds.
	using FLocationTag = FName;

	// Identifier for an activity in a schedule slot.
	using FActivityTag = FName;

	// Semantic tag attached to a memory turn or fact.
	enum class EMemoryTag : uint8
	{
		None,
		Topic,      // casual subject of conversation
		Promise,    // NPC or player committed to something
		Threat,     // hostile intent expressed
		Favor,      // helpful act performed
		Fact,       // persistent world fact about the player/NPC
		Secret      // sensitive; only shared with high trust
	};

	// Top-level affective/emotional scalars live in GTEmotionModel.h.
	// Dialogue roles:
	enum class EDialogueSpeaker : uint8
	{
		None,
		Player,
		NPC,
		System
	};

	// Result of a single LLM tier resolution.
	enum class ELLMTier : uint8
	{
		None,      // no generation (bark / scripted)
		OnDevice,  // Tier 1: Phi-3 / Nemotron on-device
		Cached,    // Tier 2: response cache hit
		Cloud      // Tier 3: DeepSeek v4-flash
	};

	// Convenience: clamp a float to [Min,Max].
	FORCEINLINE float ClampRange(float V, float Min, float Max) { return FMath::Clamp(V, Min, Max); }

	// City clock is exposed by the gameplay layer; NPC systems only read it.
	// (Declared here to avoid a hard dependency on the WorldClock actor.)
	struct FCityTimeOfDay
	{
		int32 Day = 1;
		int32 Hour = 12;     // 0..23
		int32 Minute = 0;    // 0..59

		float AsMinutesOfDay() const { return static_cast<float>(Hour) * 60.0f + static_cast<float>(Minute); }
	};
}
