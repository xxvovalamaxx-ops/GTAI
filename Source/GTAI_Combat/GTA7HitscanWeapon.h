// GTA7HitscanWeapon.h
// Instant ray weapon (pistol, rifle, SMG, sniper, shotgun).
#pragma once

#include "CoreMinimal.h"
#include "GTA7WeaponBase.h"
#include "GTA7HitscanWeapon.generated.h"

UCLASS(Blueprintable, BlueprintType)
class GTAI_COMBAT_API UGTA7HitscanWeapon : public UGTA7WeaponBase
{
    GENERATED_BODY()
protected:
    virtual void PerformShot_Implementation(const FVector& MuzzleLocation, const FRotator& AimRotation) override;
};
