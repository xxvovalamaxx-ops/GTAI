// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GTAI_ReputationSystem.generated.h"

/**
 * Tracks player standing with all factions (-100 to +100).
 * Integrated by ORACLE's world state manager and SAGA's quest system.
 */
UCLASS()
class GTAI_QUESTS_API UGTAI_ReputationSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // --- Reputation Modifiers ---

    /** Change standing with a faction (clamped to -100..100). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Reputation")
    void ModifyReputation(const FString& FactionName, int32 Delta);

    /** Set absolute standing (clamped). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Reputation")
    void SetReputation(const FString& FactionName, int32 Value);

    /** Get current standing with a faction. */
    UFUNCTION(BlueprintPure, Category = "GTAI|Reputation")
    int32 GetReputation(const FString& FactionName) const;

    // --- Relationship Tiers ---

    /** Get relationship tier name for a faction. */
    UFUNCTION(BlueprintPure, Category = "GTAI|Reputation")
    FString GetRelationshipTier(const FString& FactionName) const;

    /** Is the player hostile with this faction? */
    UFUNCTION(BlueprintPure, Category = "GTAI|Reputation")
    bool IsHostile(const FString& FactionName) const;

    /** Is the player allied with this faction? */
    UFUNCTION(BlueprintPure, Category = "GTAI|Reputation")
    bool IsAllied(const FString& FactionName) const;

    // --- Territory Influence ---

    /** Get faction influence in a district (0-100). */
    UFUNCTION(BlueprintPure, Category = "GTAI|Reputation")
    int32 GetTerritoryInfluence(const FString& FactionName, const FString& DistrictName) const;

    /** Set territory influence (called by ORACLE world state). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Reputation")
    void SetTerritoryInfluence(const FString& FactionName, const FString& DistrictName, int32 Value);

    // --- Full Snapshot ---

    /** Get all faction standings (for LLM quest generation input). */
    UFUNCTION(BlueprintPure, Category = "GTAI|Reputation")
    TMap<FString, int32> GetAllStandings() const;

    /** Get full reputation snapshot as JSON string (for LLM pipeline). */
    UFUNCTION(BlueprintPure, Category = "GTAI|Reputation")
    FString GetReputationJSON() const;

    // --- Delegates ---

    /** Fired when reputation crosses a tier boundary. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGTAI_OnReputationTierChanged,
        FString, FactionName, FString, OldTier, FString, NewTier);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Reputation")
    FGTAI_OnReputationTierChanged OnReputationTierChanged;

protected:
    /** Faction name -> standing (-100 to +100). */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Reputation")
    TMap<FString, int32> FactionStandings;

    /** Faction -> District -> influence (0-100). */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Reputation")
    TMap<FString, TMap<FString, int32>> TerritoryInfluence;

    /** Clamp helper. */
    int32 ClampStanding(int32 Value) const { return FMath::Clamp(Value, -100, 100); }

    /** Determine tier from standing value. */
    FString GetTierName(int32 Standing) const;
};
