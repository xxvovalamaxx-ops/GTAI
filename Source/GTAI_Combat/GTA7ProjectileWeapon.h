// GTA7ProjectileWeapon.h
// Spawns a projectile actor (RPG, grenade launcher, thrown).
#pragma once

#include "CoreMinimal.h"
#include "GTA7WeaponBase.h"
#include "GTA7ProjectileWeapon.generated.h"

UCLASS(Blueprintable, BlueprintType)
class GTAI_COMBAT_API UGTA7ProjectileWeapon : public UGTA7WeaponBase
{
    GENERATED_BODY()
protected:
    virtual void PerformShot_Implementation(const FVector& MuzzleLocation, const FRotator& AimRotation) override;
};

// AGTA7Projectile — travels then performs its own hit test on overlap.
UCLASS()
class GTAI_COMBAT_API AGTA7Projectile : public AActor
{
    GENERATED_BODY()
public:
    AGTA7Projectile();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
    class UProjectileMovementComponent* Movement;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
    FGTA7DamageConfig Damage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
    float DirectDamage = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
    float ExplosionRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
    bool bExplodes = false;

protected:
    virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp,
                           bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse,
                           const FHitResult& Hit) override;
};
