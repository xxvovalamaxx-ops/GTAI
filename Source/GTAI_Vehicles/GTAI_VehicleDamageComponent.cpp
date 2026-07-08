// Copyright GTAI. All Rights Reserved.
// VELOCE — Vehicle Damage Component Implementation

#include "GTAI_VehicleDamageComponent.h"
#include "GTAI_VehicleTypes.h"
#include "GTAI_BaseVehicle.h"

UGTAI_VehicleDamageComponent::UGTAI_VehicleDamageComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UGTAI_VehicleDamageComponent::InitializeComponent()
{
    Super::InitializeComponent();
    FullRepair();
}

void UGTAI_VehicleDamageComponent::ApplyImpactDamage(float ImpulseMagnitude, const FVector& HitLocation, int32 WheelIndex)
{
    if (ImpulseMagnitude < ImpactDamageThreshold) return;
    if (Health.bWrecked) return;

    // Calculate damage from impact
    float ExcessImpulse = ImpulseMagnitude - ImpactDamageThreshold;
    float DamageAmount = ExcessImpulse * ImpulseDamageRate;

    // Structural damage
    Health.StructuralDamage += DamageAmount * StructuralDamageRate;

    // Component-specific damage based on hit location
    float HeightRatio = HitLocation.Z / 200.f; // Normalize Z to ~0-1

    if (HeightRatio < 0.3f && WheelIndex >= 0)
    {
        // Wheel/tire damage
        if (Health.TireHealth.IsValidIndex(WheelIndex))
            Health.TireHealth[WheelIndex] -= DamageAmount * 0.5f;
        if (Health.SuspensionHealth.IsValidIndex(WheelIndex))
            Health.SuspensionHealth[WheelIndex] -= DamageAmount * 0.3f;
    }
    else if (HeightRatio < 0.6f)
    {
        // Engine bay — engine and transmission damage
        Health.EngineHealth -= DamageAmount * 0.4f;
        Health.TransmissionHealth -= DamageAmount * 0.3f;
    }
    else
    {
        // Body — visual damage, minor structural
        Health.VisualDamage += DamageAmount;
    }

    // Clamp all values
    Health.EngineHealth = FMath::Clamp(Health.EngineHealth, 0.f, 1.f);
    Health.TransmissionHealth = FMath::Clamp(Health.TransmissionHealth, 0.f, 1.f);
    Health.StructuralDamage = FMath::Clamp(Health.StructuralDamage, 0.f, 1.f);
    Health.VisualDamage = FMath::Clamp(Health.VisualDamage, 0.f, 1.f);

    for (int32 i = 0; i < Health.TireHealth.Num(); ++i)
    {
        Health.TireHealth[i] = FMath::Clamp(Health.TireHealth[i], 0.f, 1.f);
        Health.SuspensionHealth[i] = FMath::Clamp(Health.SuspensionHealth[i], 0.f, 1.f);
        Health.BrakeHealth[i] = FMath::Clamp(Health.BrakeHealth[i], 0.f, 1.f);
    }

    OnDamaged.Broadcast(Health.StructuralDamage);
    UpdateDerivedStates();
}

void UGTAI_VehicleDamageComponent::ApplyWeaponDamage(float Amount, EGTAI_VehicleAssist TargetSystem)
{
    if (Health.bWrecked) return;

    switch (TargetSystem)
    {
    case EGTAI_VehicleAssist::TractionControl:
        Health.EngineHealth -= Amount;
        break;
    case EGTAI_VehicleAssist::ABS:
        for (float& Brake : Health.BrakeHealth) Brake -= Amount * 0.5f;
        break;
    case EGTAI_VehicleAssist::StabilityControl:
        Health.SuspensionHealth[0] -= Amount * 0.3f;
        Health.SuspensionHealth[1] -= Amount * 0.3f;
        break;
    default:
        Health.StructuralDamage += Amount * 0.5f;
        break;
    }

    Health.StructuralDamage += Amount * 0.3f;
    Health.EngineHealth = FMath::Clamp(Health.EngineHealth, 0.f, 1.f);
    Health.StructuralDamage = FMath::Clamp(Health.StructuralDamage, 0.f, 1.f);

    OnDamaged.Broadcast(Health.StructuralDamage);
    UpdateDerivedStates();
}

void UGTAI_VehicleDamageComponent::Repair(float Fraction)
{
    Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
    Health.EngineHealth = FMath::Min(1.f, Health.EngineHealth + Fraction);
    Health.TransmissionHealth = FMath::Min(1.f, Health.TransmissionHealth + Fraction);
    for (int32 i = 0; i < Health.TireHealth.Num(); ++i)
    {
        Health.TireHealth[i] = FMath::Min(1.f, Health.TireHealth[i] + Fraction);
        Health.SuspensionHealth[i] = FMath::Min(1.f, Health.SuspensionHealth[i] + Fraction);
        Health.BrakeHealth[i] = FMath::Min(1.f, Health.BrakeHealth[i] + Fraction);
    }
    Health.StructuralDamage = FMath::Max(0.f, Health.StructuralDamage - Fraction);
    Health.bWrecked = Health.StructuralDamage >= 1.f;
    Health.bOnFire = false;
}

void UGTAI_VehicleDamageComponent::FullRepair()
{
    Health = FGTAI_VehicleHealth();
    Health.TireHealth = { 1.f, 1.f, 1.f, 1.f };
    Health.SuspensionHealth = { 1.f, 1.f, 1.f, 1.f };
    Health.BrakeHealth = { 1.f, 1.f, 1.f, 1.f };
}

float UGTAI_VehicleDamageComponent::GetTireGripScale(int32 WheelIndex) const
{
    if (WheelIndex >= 0 && WheelIndex < Health.TireHealth.Num())
        return Health.TireHealth[WheelIndex];
    return 1.f;
}

void UGTAI_VehicleDamageComponent::UpdateDerivedStates()
{
    if (Health.StructuralDamage >= FireThreshold)
    {
        Health.bOnFire = true;
    }

    if (Health.StructuralDamage >= 1.f && !Health.bWrecked)
    {
        Health.bWrecked = true;
        OnWrecked.Broadcast(Cast<AGTAI_BaseVehicle>(GetOwner()));
    }
}
