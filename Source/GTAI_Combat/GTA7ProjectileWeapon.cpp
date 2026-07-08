// Copyright GTAI. All Rights Reserved.
// STRIKE — Projectile Weapon Implementation

#include "GTA7ProjectileWeapon.h"
#include "GTA7Character.h"
#include "GTA7HitDetection.h"
#include "Engine/World.h"

void UGTA7ProjectileWeapon::OnShotFired()
{
    FVector MuzzleLoc, MuzzleDir;
    GetMuzzleTransform(MuzzleLoc, MuzzleDir);

    FActorSpawnParameters Params;
    Params.Owner = OwnerCharacter;
    Params.Instigator = OwnerCharacter;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (ProjectileClass)
    {
        AGTA7Projectile* Proj = GetWorld()->SpawnActor<AGTA7Projectile>(
            ProjectileClass, MuzzleLoc, MuzzleDir.Rotation(), Params);

        if (Proj)
        {
            Proj->Damage = Config.DamagePerShot;
            Proj->Shooter = OwnerCharacter;
            Proj->SetLifeSpan(ProjectileLifetime);
            Proj->Launch(MuzzleDir * Config.ProjectileSpeed);
        }
    }
}

// ── AGTA7Projectile ──

AGTA7Projectile::AGTA7Projectile()
{
    PrimaryActorTick.bCanEverTick = true;
    Damage = 0.f;
    Shooter = nullptr;
    Speed = 0.f;
}

void AGTA7Projectile::Launch(const FVector& Velocity)
{
    Speed = Velocity.Size();
    SetActorEnableCollision(true);
}

void AGTA7Projectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    SetActorLocation(GetActorLocation() + GetActorForwardVector() * Speed * DeltaTime, true);
}

void AGTA7Projectile::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other,
    UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
    FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

    if (Other && Other != Shooter)
    {
        UGTA7HitDetection::ApplyDamage(Other, Damage, Shooter,
            HitLocation, HitNormal, Hit.BoneName, Hit.PhysMaterial.Get());
        Destroy();
    }
}
