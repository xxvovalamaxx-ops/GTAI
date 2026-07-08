// GTMemoryTypes.h
// Two-tier memory data structures: short-term (working) + long-term (facts/relationships).
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"

namespace GTAI::NPC
{
	// ---- Short-term (working) memory ----
	struct FConversationTurn
	{
		EDialogueSpeaker Speaker = EDialogueSpeaker::None;
		FString Content;
		float Importance = 0.5f;          // 0..1, drives eviction + consolidation
		TArray<EMemoryTag> Tags;
		double Timestamp = 0.0;           // game-time seconds
	};

	// ---- Long-term (episodic facts) ----
	struct FFactRecord
	{
		FString Fact;                     // compact natural-language fact
		TArray<EMemoryTag> Tags;
		float Salience = 0.5f;            // 0..1, decays slowly
		double LearnedAt = 0.0;           // game-time seconds
	};

	// ---- Long-term relationship (dyad: this NPC -> Other) ----
	struct FRelationshipRecord
	{
		FNPCId Other = 0;                 // player id or another NPC id
		float Affinity = 0.f;             // -100..100
		float Trust = 50.f;               // 0..100
		float Fear = 0.f;                 // 0..100
		float Respect = 0.f;              // -100..100
		TArray<FString> EventHistory;     // compact log ("helped on Day3")
		double LastUpdated = 0.0;
	};

	// Compact character profile (backstory delta), produced by consolidation.
	struct FCharacterProfile
	{
		FString Summary;                  // summarized long-term self-narrative
		TArray<FString> Traits;           // mutable personality tags
	};
}
