// GTMemoryStore.cpp
// Per-NPC two-tier memory: ring-buffered short-term + salience-evicted
// long-term facts/relationships. Plain C++ (no UObject) so it can back a
// MassEntity fragment for the crowd.

#include "Memory/GTMemoryStore.h"
#include "Memory/GTMemoryTypes.h"
#include "NPC/GTNPCDefines.h"

namespace GTAI::NPC
{
	// Long-term fact cap before we evict the lowest-salience entries.
	static constexpr int32 LongTermCap = 256;

	// Importance threshold above which a short-term turn is promoted to a fact.
	static constexpr float ConsolidationThreshold = 0.6f;

	namespace
	{
		// Manual enum->string (EMemoryTag is not a reflected UENUM, so StaticEnum
		// is unavailable). Keeps the snippet human/machine readable.
		FString MemoryTagToString(EMemoryTag Tag)
		{
			switch (Tag)
			{
			case EMemoryTag::Topic:   return TEXT("Topic");
			case EMemoryTag::Promise: return TEXT("Promise");
			case EMemoryTag::Threat:  return TEXT("Threat");
			case EMemoryTag::Favor:   return TEXT("Favor");
			case EMemoryTag::Fact:    return TEXT("Fact");
			case EMemoryTag::Secret:  return TEXT("Secret");
			case EMemoryTag::None:
			default:                  return TEXT("None");
			}
		}

		int32 ApproxTokens(const FString& S) { return (S.Len() / 4) + 1; }
	}

	void FMemoryStore::PushTurn(const FConversationTurn& Turn)
	{
		ShortTerm.Add(Turn);

		// Ring buffer: hard cap on entry count.
		if (ShortTerm.Num() > ShortTermCap)
		{
			ShortTerm.RemoveAt(0, ShortTerm.Num() - ShortTermCap, EAllowShrinking::No);
		}

		// Soft cap on approximate token budget; evict oldest until under budget.
		int32 Tokens = 0;
		for (const FConversationTurn& T : ShortTerm) { Tokens += ApproxTokens(T.Content); }
		while (ShortTerm.Num() > 0 && Tokens > ShortTermTokenCap)
		{
			Tokens -= ApproxTokens(ShortTerm[0].Content);
			ShortTerm.RemoveAt(0);
		}
	}

	void FMemoryStore::FlushShortTermToLongTerm()
	{
		for (const FConversationTurn& T : ShortTerm)
		{
			if (T.Importance < ConsolidationThreshold)
			{
				continue;
			}

			FFactRecord Fact;
			const TCHAR* Spk = (T.Speaker == EDialogueSpeaker::Player) ? TEXT("Player")
							: (T.Speaker == EDialogueSpeaker::NPC)    ? TEXT("NPC")
							:                                            TEXT("System");
			Fact.Fact = FString::Printf(TEXT("%s: %s"), Spk, *T.Content);
			Fact.Tags = T.Tags;
			Fact.Salience = FMath::Clamp(T.Importance, 0.f, 1.f);
			Fact.LearnedAt = T.Timestamp;
			Facts.Add(MoveTemp(Fact));
		}

		ShortTerm.Empty();

		// Salience-evict if we blew past the cap during this flush.
		if (Facts.Num() > LongTermCap)
		{
			Facts.Sort([](const FFactRecord& A, const FFactRecord& B)
				{ return A.Salience > B.Salience; });
			Facts.SetNum(LongTermCap, EAllowShrinking::No);
		}
	}

	void FMemoryStore::AddFact(const FFactRecord& Fact)
	{
		Facts.Add(Fact);

		if (Facts.Num() > LongTermCap)
		{
			// Drop the single lowest-salience fact.
			int32 Lowest = 0;
			for (int32 i = 1; i < Facts.Num(); ++i)
			{
				if (Facts[i].Salience < Facts[Lowest].Salience) { Lowest = i; }
			}
			Facts.RemoveAt(Lowest);
		}
	}

	FString FMemoryStore::SummarizeFacts() const
	{
		FString Out;
		for (const FFactRecord& F : Facts)
		{
			Out += TEXT("- ");
			Out += F.Fact;
			if (F.Tags.Num() > 0)
			{
				Out += TEXT(" [");
				for (int32 i = 0; i < F.Tags.Num(); ++i)
				{
					if (i) { Out += TEXT(","); }
					Out += MemoryTagToString(F.Tags[i]);
				}
				Out += TEXT("]");
			}
			Out += TEXT("\n");
		}
		return Out;
	}

	FRelationshipRecord& FMemoryStore::RelationshipWith(FNPCId Other)
	{
		if (FRelationshipRecord* Existing = Relationships.Find(Other))
		{
			return *Existing;
		}
		FRelationshipRecord& NewRec = Relationships.Add(Other);
		NewRec.Other = Other;
		NewRec.Affinity = 0.f;
		NewRec.Trust = 50.f;
		NewRec.Fear = 0.f;
		NewRec.Respect = 0.f;
		return NewRec;
	}

	const FRelationshipRecord* FMemoryStore::FindRelationship(FNPCId Other) const
	{
		return Relationships.Find(Other);
	}

	void FMemoryStore::ApplyRelationshipDelta(FNPCId Other, float dAffinity, float dTrust, float dFear, const FString& Event)
	{
		FRelationshipRecord& R = RelationshipWith(Other);
		R.Affinity = ClampRange(R.Affinity + dAffinity, -100.f, 100.f);
		R.Trust    = ClampRange(R.Trust    + dTrust,      0.f, 100.f);
		R.Fear     = ClampRange(R.Fear     + dFear,       0.f, 100.f);
		if (!Event.IsEmpty())
		{
			R.EventHistory.Add(Event);
		}
		R.LastUpdated = 0.0; // city clock is injected by the gameplay layer on Tick
	}

	FString FMemoryStore::BuildContextSnippet(int32 MaxChars) const
	{
		FString Out;

		if (!Profile.Summary.IsEmpty())
		{
			Out += TEXT("Self-narrative: ");
			Out += Profile.Summary;
			Out += TEXT("\n");
		}
		if (Profile.Traits.Num() > 0)
		{
			Out += TEXT("Traits: ");
			Out += FString::Join(Profile.Traits, TEXT(", "));
			Out += TEXT("\n");
		}

		// Top facts by salience (most important first), bounded by MaxChars.
		TArray<FFactRecord> Sorted = Facts;
		Sorted.Sort([](const FFactRecord& A, const FFactRecord& B)
			{ return A.Salience > B.Salience; });

		Out += TEXT("Known facts:\n");
		for (const FFactRecord& F : Sorted)
		{
			FString Line = TEXT("- ") + F.Fact + TEXT("\n");
			if (Out.Len() + Line.Len() > MaxChars) { break; }
			Out += Line;
		}

		// Relationships (compact).
		for (const TPair<FNPCId, FRelationshipRecord>& Pair : Relationships)
		{
			const FRelationshipRecord& R = Pair.Value;
			FString Line = FString::Printf(
				TEXT("Relation %llu: aff=%d trust=%d fear=%d respect=%d\n"),
				static_cast<uint64>(R.Other),
				FMath::RoundToInt(R.Affinity),
				FMath::RoundToInt(R.Trust),
				FMath::RoundToInt(R.Fear),
				FMath::RoundToInt(R.Respect));
			if (Out.Len() + Line.Len() > MaxChars) { break; }
			Out += Line;
		}

		return Out;
	}

	void FMemoryStore::Serialize(FArchive& Ar)
	{
		Ar << Owner;
		Ar << ShortTerm;
		Ar << Facts;
		Ar << Relationships;
		Ar << Profile;
	}
}
