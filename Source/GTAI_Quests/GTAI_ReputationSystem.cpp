// Copyright GTAI. All Rights Reserved.
// SAGA — Reputation System Implementation

#include "GTAI_ReputationSystem.h"

void UGTAI_ReputationSystem::ModifyReputation(const FString& FactionName, int32 Delta)
{
    int32& Standing = FactionStandings.FindOrAdd(FactionName);
    int32 OldTier = ClampStanding(Standing);
    Standing = ClampStanding(Standing + Delta);

    FString OldTierName = GetTierName(OldTier);
    FString NewTierName = GetTierName(Standing);

    if (OldTierName != NewTierName)
    {
        OnReputationTierChanged.Broadcast(FactionName, OldTierName, NewTierName);
    }
}

void UGTAI_ReputationSystem::SetReputation(const FString& FactionName, int32 Value)
{
    FactionStandings.FindOrAdd(FactionName) = ClampStanding(Value);
}

int32 UGTAI_ReputationSystem::GetReputation(const FString& FactionName) const
{
    if (const int32* Found = FactionStandings.Find(FactionName)) return *Found;
    return 0;
}

FString UGTAI_ReputationSystem::GetRelationshipTier(const FString& FactionName) const
{
    return GetTierName(GetReputation(FactionName));
}

bool UGTAI_ReputationSystem::IsHostile(const FString& FactionName) const
{
    return GetReputation(FactionName) <= -50;
}

bool UGTAI_ReputationSystem::IsAllied(const FString& FactionName) const
{
    return GetReputation(FactionName) >= 50;
}

int32 UGTAI_ReputationSystem::GetTerritoryInfluence(const FString& FactionName, const FString& DistrictName) const
{
    if (const TMap<FString, int32>* DistrictMap = TerritoryInfluence.Find(FactionName))
        if (const int32* Influence = DistrictMap->Find(DistrictName)) return *Influence;
    return 0;
}

void UGTAI_ReputationSystem::SetTerritoryInfluence(const FString& FactionName, const FString& DistrictName, int32 Value)
{
    TerritoryInfluence.FindOrAdd(FactionName).FindOrAdd(DistrictName) = FMath::Clamp(Value, 0, 100);
}

TMap<FString, int32> UGTAI_ReputationSystem::GetAllStandings() const { return FactionStandings; }

FString UGTAI_ReputationSystem::GetReputationJSON() const
{
    FString Json = TEXT("{");
    for (const auto& Pair : FactionStandings)
    {
        Json += FString::Printf(TEXT("\"%s\": %d,"), *Pair.Key, Pair.Value);
    }
    if (Json.EndsWith(TEXT(","))) Json.RemoveAt(Json.Len() - 1);
    Json += TEXT("}");
    return Json;
}

FString UGTAI_ReputationSystem::GetTierName(int32 Standing) const
{
    if (Standing <= -50) return TEXT("Hostile");
    if (Standing <= -20) return TEXT("Unfriendly");
    if (Standing <= 20)  return TEXT("Neutral");
    if (Standing <= 50)  return TEXT("Friendly");
    return TEXT("Allied");
}
