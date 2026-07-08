// Copyright GTAI. All Rights Reserved.
// ORACLE — World Wanted System Implementation
#include "GTAI_World_Wanted.h"
#include "GTAI_World_WorldStateManager.h"

// ── FWantedCore (deterministic, no RNG) ──

GTAI::World::EWantedLevel GTAI::World::FWantedCore::CommitCrime(ECrimeSeverity Severity, bool bWitnessed, int32 CellIndex)
{
    if (!bWitnessed) return CurrentStars;

    EWantedLevel Contrib = SeverityToStars(Severity);
    CurrentStars = static_cast<EWantedLevel>(FMath::Min(static_cast<int32>(CurrentStars) + static_cast<int32>(Contrib), 5));
    OnStarsChanged.Broadcast(CurrentStars, CurrentStars);
    return CurrentStars;
}

void GTAI::World::FWantedCore::TickDeescalation(float SimDt, bool bAnyCopHasLOS, float DistToNearestCop)
{
    if (CurrentStars == EWantedLevel::Clean) return;
    if (!bAnyCopHasLOS && DistToNearestCop > 2500.f)
    {
        CooldownTimer += SimDt;
        if (CooldownTimer >= 30.f) // 30 seconds no cop sight = star drop
        {
            EWantedLevel Old = CurrentStars;
            CurrentStars = static_cast<EWantedLevel>(FMath::Max(static_cast<int32>(CurrentStars) - 1, 0));
            CooldownTimer = 0.f;
            OnStarsChanged.Broadcast(Old, CurrentStars);
        }
    }
    else { CooldownTimer = 0.f; }
}

bool GTAI::World::FWantedCore::TryBribe(float AvailableCash, bool bAnyCopHasLOS, float& OutCost)
{
    if (bAnyCopHasLOS || CurrentStars == EWantedLevel::Clean) return false;
    OutCost = 500.f * static_cast<int32>(CurrentStars);
    if (AvailableCash >= OutCost) { Clear(); return true; }
    return false;
}

GTAI::World::EWantedLevel GTAI::World::FWantedCore::SeverityToStars(ECrimeSeverity S)
{
    switch (S) {
    case ECrimeSeverity::Minor:    return EWantedLevel::OneStar;
    case ECrimeSeverity::Moderate: return EWantedLevel::TwoStar;
    case ECrimeSeverity::Major:    return EWantedLevel::ThreeStar;
    case ECrimeSeverity::Capital:  return EWantedLevel::FourStar;
    default: return EWantedLevel::OneStar;
    }
}

// ── UGTAI_WorldWantedSystem ──

void UGTAI_WorldWantedSystem::Initialize(UGTAI_WorldStateManager* InWorld) { World = InWorld; }

void UGTAI_WorldWantedSystem::ReportCrime(GTAI::World::ECrimeSeverity Severity, bool bWitnessed, const FVector& Location)
{
    int32 CellIndex = World.IsValid() ? World->GetCellIndex(Location) : INDEX_NONE;
    Core.CommitCrime(Severity, bWitnessed, CellIndex);
}

void UGTAI_WorldWantedSystem::SimTick(float SimDt, const FVector& PlayerLocation, bool bAnyCopHasLOS, float DistToNearestCop)
{
    Core.TickDeescalation(SimDt, bAnyCopHasLOS, DistToNearestCop);
    RunPoliceTactics(SimDt);
}

GTAI::World::FDispatchRequest UGTAI_WorldWantedSystem::BuildDispatch(const FVector& Location) const
{
    GTAI::World::FDispatchRequest Req;
    Req.Stars = Core.GetStars();
    Req.Location = Location;
    if (World.IsValid()) Req.SourceCellIndex = World->GetCellIndex(Location);

    switch (Core.GetStars()) {
    case GTAI::World::EWantedLevel::OneStar:   Req.PatrolCars = 1; break;
    case GTAI::World::EWantedLevel::TwoStar:   Req.PatrolCars = 2; break;
    case GTAI::World::EWantedLevel::ThreeStar: Req.PatrolCars = 3; Req.Helicopters = 1; Req.bRoadblockCapable = true; break;
    case GTAI::World::EWantedLevel::FourStar:  Req.PatrolCars = 4; Req.Helicopters = 2; Req.NOOSE = 1; Req.bRoadblockCapable = true; break;
    case GTAI::World::EWantedLevel::FiveStar:  Req.PatrolCars = 6; Req.Helicopters = 3; Req.NOOSE = 2; Req.bRoadblockCapable = true; break;
    default: break;
    }
    return Req;
}

void UGTAI_WorldWantedSystem::RunPoliceTactics(float SimDt)
{
    // AI tactical layer: Behavior Tree + EQS for police maneuvers
    // TODO: Spawn patrol cars at search radius, helicopters for air support
    // TODO: Roadblock placement on nearby streets using nav mesh
    // TODO: EQS queries for most-likely player escape routes
}
