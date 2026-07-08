// Copyright GTAI. All Rights Reserved.
// VELOCE/ORACLE — Traffic AI Controller Implementation

#include "GTAI_TrafficAIController.h"
#include "GTAI_BaseVehicle.h"
#include "GTAI_VehicleTypes.h"
#include "GTAI_World_TrafficSpawner.h"
#include "Engine/World.h"
#include "AIController.h"

AGTAI_TrafficAIController::AGTAI_TrafficAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
}

void AGTAI_TrafficAIController::SetRoute(const TArray<FVector>& Waypoints)
{
    RouteWaypoints = Waypoints;
    CurrentWaypointIndex = 0;

    if (Waypoints.Num() > 0)
    {
        TargetSpeed = FMath::FRandRange(600.f, 1100.f); // Random speed for variety
    }
}

void AGTAI_TrafficAIController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (RouteWaypoints.Num() == 0) return;

    DriveTowardWaypoint(DeltaSeconds);
    CheckDespawn();
}

void AGTAI_TrafficAIController::DriveTowardWaypoint(float DeltaSeconds)
{
    if (!GetPawn()) return;

    AGTAI_BaseVehicle* Vehicle = Cast<AGTAI_BaseVehicle>(GetPawn());
    if (!Vehicle) return;

    FVector CurrentLocation = Vehicle->GetActorLocation();
    FVector TargetLocation = RouteWaypoints[CurrentWaypointIndex];

    float Distance = FVector::Dist(CurrentLocation, TargetLocation);

    // Check if reached waypoint
    if (Distance < 300.f)
    {
        AdvanceWaypoint();
        return;
    }

    // Should stop? (red light, obstacle)
    if (ShouldStop())
    {
        // Brake
        return;
    }

    // Steer toward waypoint
    FVector Direction = (TargetLocation - CurrentLocation).GetSafeNormal();
    FRotator TargetRotation = Direction.Rotation();
    FRotator CurrentRotation = Vehicle->GetActorRotation();

    float YawDiff = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw);
    float SteerAmount = FMath::Clamp(YawDiff / 90.f, -1.f, 1.f);

    // Apply movement
    FVector Movement = Direction * TargetSpeed * DeltaSeconds;
    Vehicle->AddActorWorldOffset(Movement, true);
    Vehicle->SetActorRotation(FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaSeconds, 3.f));

    // Speed modulation based on turn sharpness
    float SpeedMod = 1.f - FMath::Abs(SteerAmount) * 0.5f; // Slow down on turns
    float CurrentSpeed = TargetSpeed * SpeedMod;
    Vehicle->AddActorWorldOffset(Direction * CurrentSpeed * DeltaSeconds, true);
}

bool AGTAI_TrafficAIController::ShouldStop() const
{
    // TODO: Check traffic light state from WorldStateManager
    // TODO: Check for pedestrian in crosswalk
    // TODO: Check for obstacle ahead (raycast)
    return false;
}

void AGTAI_TrafficAIController::AdvanceWaypoint()
{
    CurrentWaypointIndex = (CurrentWaypointIndex + 1) % RouteWaypoints.Num();

    if (CurrentWaypointIndex == 0)
    {
        // Completed circuit — generate new random route
        // TODO: Query street graph for next route
        RouteWaypoints.Empty();
    }
}

void AGTAI_TrafficAIController::CheckDespawn()
{
    if (!GetPawn()) return;

    // Get player location from world
    if (GetWorld()->GetFirstPlayerController())
    {
        APawn* PlayerPawn = GetWorld()->GetFirstPlayerController()->GetPawn();
        if (PlayerPawn)
        {
            float Distance = FVector::Dist(GetPawn()->GetActorLocation(), PlayerPawn->GetActorLocation());
            if (Distance > DespawnDistance)
            {
                GetPawn()->Destroy();
            }
        }
    }
}
