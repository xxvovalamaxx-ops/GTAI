// GTA7MeleeWeapon.h
// Short arc / sweep weapon (fists, bat, knife).
#pragma once

#include "CoreMinimal.h"
#include "GTA7WeaponBase.h"
#include "GTA7MeleeWeapon.generated.h"

UCLASS(Blueprintable, BlueprintType)
class GTAI_COMBAT_API UGTA7MeleeWeapon : public UGTA7WeaponBase
{
    GENERATED_BODY()
public:
    // Sweep half-angle (degrees) and reach (cm).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
    float SweepArcDegrees = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
    float Reach = 150.f;

    // Small forward lunge impulse on swing.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
    float LungeImpulse = 200.f;

protected:
    virtual void PerformShot_Implementation(const FVector& MuzzleLocation, const FRotator& AimRotation) override;
};
