// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GTAI_VehicleTypes.h"

#include "GTAI_TrafficAIController.generated.h"

/**
 * AI controller for ambient traffic vehicles.
 * Follows street network from ATLAS city data, respects traffic lights
 * from ORACLE world systems, avoids collisions, despawns out of range.
 */
UCLASS()
class GTAI_VEHICLES_API AGTAI_TrafficAIController : public AAIController
{
    GENERATED_BODY()

public:
    AGTAI_TrafficAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    /** Set the route waypoints for this traffic vehicle to follow. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Traffic AI")
    void SetRoute(const TArray<FVector>& Waypoints);

    /** Current target waypoint index. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GTAI|Traffic AI")
    int32 CurrentWaypointIndex = 0;

    /** Should this vehicle stop at red lights? */
    UPROPERTY(EditAnywhere, Category = "GTAI|Traffic AI")
    bool bRespectsTrafficLights = true;

    /** Should this vehicle yield to pedestrians? */
    UPROPERTY(EditAnywhere, Category = "GTAI|Traffic AI")
    bool bYieldsToPedestrians = true;

    /** Despawn distance — if further than this from player, request despawn. */
    UPROPERTY(EditAnywhere, Category = "GTAI|Traffic AI")
    float DespawnDistance = 3000.f;

protected:
    virtual void Tick(float DeltaSeconds) override;

    /** Route waypoints in world space (from street network). */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Traffic AI")
    TArray<FVector> RouteWaypoints;

    /** Target speed in cm/s. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Traffic AI")
    float TargetSpeed = 800.f; // ~30 mph default

    /** Drive toward current waypoint. */
    void DriveTowardWaypoint(float DeltaSeconds);

    /** Check if should stop (red light, pedestrian, obstacle). */
    bool ShouldStop() const;

    /** Advance to next waypoint. */
    void AdvanceWaypoint();

    /** Check despawn condition. */
    void CheckDespawn();
};
