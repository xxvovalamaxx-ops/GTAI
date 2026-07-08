// GTMemoryStore.h
// Per-NPC two-tier memory: ring-buffered short-term + persistent long-term.
// Designed to back a MassEntity fragment (no UObject per pedestrian at runtime).
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "Memory/GTMemoryTypes.h"

namespace GTAI::NPC
{
	class GTAI_NPC_API FMemoryStore
	{
	public:
		explicit FMemoryStore(FNPCId InNPC) : Owner(InNPC) {}

		// --- Short-term (working) ---
		void PushTurn(const FConversationTurn& Turn);
		const TArray<FConversationTurn>& GetShortTerm() const { return ShortTerm; }
		void FlushShortTermToLongTerm();   // move important turns into facts

		// --- Long-term facts ---
		void AddFact(const FFactRecord& Fact);
		const TArray<FFactRecord>& GetFacts() const { return Facts; }
		FString SummarizeFacts() const;     // for prompt injection

		// --- Relationships ---
		FRelationshipRecord& RelationshipWith(FNPCId Other);
		const FRelationshipRecord* FindRelationship(FNPCId Other) const;
		void ApplyRelationshipDelta(FNPCId Other, float dAffinity, float dTrust, float dFear, const FString& Event);

		// --- Consolidation (called by a low-priority timer, uses Tier-1) ---
		// Returns a prompt-ready compact summary of this NPC's memory.
		FString BuildContextSnippet(int32 MaxChars = 1500) const;

		// --- Persistence ---
		void Serialize(FArchive& Ar);       // save/load via USaveGame blob

	private:
		FNPCId Owner = 0;
		TArray<FConversationTurn> ShortTerm; // ring buffer, cap ShortTermCap
		TArray<FFactRecord> Facts;           // unbounded, salience-evicted
		TMap<FNPCId, FRelationshipRecord> Relationships;
		FCharacterProfile Profile;

		static constexpr int32 ShortTermCap = 16;
		static constexpr int32 ShortTermTokenCap = 4096;
	};
}
