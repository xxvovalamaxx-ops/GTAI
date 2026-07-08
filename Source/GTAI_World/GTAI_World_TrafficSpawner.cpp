// Copyright GTAI. All Rights Reserved.
// ORACLE — World Traffic Spawner (Deterministic Poisson-based)

#include "GTAI_World_TrafficSpawner.h"
#include "GTAI_World_WorldStateManager.h"

// ── FTrafficSpawnModel ──

int32 GTAI::World::FTrafficSpawnModel::ComputeBudget(int32 CellIndex, float TimeOfDay01, float WeatherMult, FRandomStream& Rng) const
{
    float TimeMult = 1.f - FMath::Abs(TimeOfDay01 - 0.5f) * 2.f; // Peak at midday
    float Budget = BaseDensity * FMath::Max(0.1f, TimeMult) * FMath::Max(0.3f, WeatherMult);
    return FMath::RoundToInt(Budget);
}

float GTAI::World::FTrafficSpawnModel::NextArrivalDelay(int32 CellIndex, FRandomStream& Rng) const
{
    return -FMath::Loge(1.f - Rng.GetFraction()) / PoissonLambda;
}

// ── UGTAI_WorldTrafficSpawner ──

void UGTAI_WorldTrafficSpawner::Initialize(UGTAI_WorldStateManager* InWorld) { World = InWorld; }

void UGTAI_WorldTrafficSpawner::SimTick(float SimDt, float TimeOfDay01, float WeatherMult)
{
    UpdateSignals(SimDt);
    // Per-cell spawn budget managed by caller (WorldStateManager)
}

int32 UGTAI_WorldTrafficSpawner::PickNextRouteNode(int32 CurrentNode, FRandomStream& Rng) const
{
    // TODO: Query street graph from ATLAS city data for valid next edges
    return CurrentNode + Rng.RandRange(1, 3);
}

void UGTAI_WorldTrafficSpawner::UpdateSignals(float SimDt)
{
    static const float SignalCycleDuration = 60.f;

    for (auto& Pair : SignalPhases)
    {
        int32 Id = Pair.Key;
        GTAI::World::ESignalPhase& Phase = Pair.Value;
        float& Timer = SignalTimers.FindOrAdd(Id);

        Timer += SimDt;

        bool bPreempted = PreemptedIntersections.Contains(Id);
        float GreenDuration = bPreempted ? 5.f : (SignalCycleDuration / 4.f);

        if (Timer >= GreenDuration)
        {
            Timer = 0.f;
            // Advance to next phase
            Phase = static_cast<GTAI::World::ESignalPhase>((static_cast<int32>(Phase) + 1) % 5);
            if (bPreempted) PreemptedIntersections.Remove(Id);
        }
    }
}

void UGTAI_WorldTrafficSpawner::RequestSignalPreempt(int32 IntersectionId, bool bNorthSouth)
{
    PreemptedIntersections.Add(IntersectionId);
    SignalPhases.FindOrAdd(IntersectionId) = bNorthSouth ?
        GTAI::World::ESignalPhase::NorthSouthGreen : GTAI::World::ESignalPhase::EastWestGreen;
}

bool UGTAI_WorldTrafficSpawner::IsCrosswalkBlocked(int32 IntersectionId) const
{
    // TODO: Query pedestrian positions near intersection
    return false;
}
