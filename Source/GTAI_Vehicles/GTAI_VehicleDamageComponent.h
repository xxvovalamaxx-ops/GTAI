// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTAI_VehicleTypes.h"

#include "GTAI_VehicleDamageComponent.generated.h"

/**
 * Tracks per-component mechanical damage and visual damage for a vehicle.
 * Damage is accumulated from impacts and weapon hits; it degrades handling
 * progressively rather than using a single health bar.
 */
UCLASS(ClassGroup = (GTAI), meta = (BlueprintSpawnableComponent))
class GTAI_VEHICLES_API UGTAI_VehicleDamageComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGTAI_VehicleDamageComponent();

    /** Current health snapshot (0 = destroyed, 1 = healthy). */
    UPROPERTY(BlueprintReadOnly, Category = "GTAI|Damage")
    FGTAI_VehicleHealth Health;

    /** Called when structural damage increases. */
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Damage")
    FGTAI_OnVehicleDamaged OnDamaged;

    /** Called when vehicle becomes wrecked. */
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Damage")
    FGTAI_OnVehicleWrecked OnWrecked;

    /** Apply impact damage from a collision hit. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Damage")
    void ApplyImpactDamage(float ImpulseMagnitude, const FVector& HitLocation, int32 WheelIndex = -1);

    /** Apply weapon damage (bullet, explosion) to specific components. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Damage")
    void ApplyWeaponDamage(float Amount, EGTAI_VehicleAssist TargetSystem = EGTAI_VehicleAssist::TractionControl);

    /** Repair all components by a fraction (0-1). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Damage")
    void Repair(float Fraction = 0.5f);

    /** Fully repair the vehicle. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Damage")
    void FullRepair();

    /** Is the vehicle currently drivable? */
    UFUNCTION(BlueprintPure, Category = "GTAI|Damage")
    bool IsDrivable() const { return Health.StructuralDamage < 1.f && !Health.bWrecked; }

    /** Get engine power multiplier (1 = full, 0 = none) based on engine health. */
    UFUNCTION(BlueprintPure, Category = "GTAI|Damage")
    float GetEnginePowerScale() const { return Health.EngineHealth; }

    /** Get tire grip multiplier for a specific wheel. */
    UFUNCTION(BlueprintPure, Category = "GTAI|Damage")
    float GetTireGripScale(int32 WheelIndex) const;

protected:
    virtual void InitializeComponent() override;

    /** Threshold impulse before damage is applied. */
    UPROPERTY(EditAnywhere, Category = "GTAI|Damage|Tuning")
    float ImpactDamageThreshold = 5000.f;

    /** Damage per unit of impulse above threshold. */
    UPROPERTY(EditAnywhere, Category = "GTAI|Damage|Tuning")
    float ImpulseDamageRate = 0.0001f;

    /** Structural damage per impact. */
    UPROPERTY(EditAnywhere, Category = "GTAI|Damage|Tuning")
    float StructuralDamageRate = 0.05f;

    /** Fire threshold — structural damage above this starts fire. */
    UPROPERTY(EditAnywhere, Category = "GTAI|Damage|Tuning")
    float FireThreshold = 0.8f;

    /** Update derived states (fire, wrecked) after damage changes. */
    void UpdateDerivedStates();
};
