// Copyright GTAI. All Rights Reserved.
// STRIKE — Melee Weapon Implementation

#include "GTA7MeleeWeapon.h"
#include "GTA7Character.h"
#include "GTA7HitDetection.h"
#include "GTA7DamageSystem.h"
#include "Engine/World.h"

void UGTA7MeleeWeapon::OnShotFired()
{
    FVector Start, Dir;
    GetMuzzleTransform(Start, Dir);

    FVector End = Start + Dir * MeleeRange;

    // Sweep trace for melee hit
    FCollisionShape SweepShape = FCollisionShape::MakeSphere(MeleeRadius);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);

    if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_GameTraceChannel1, SweepShape, Params))
    {
        if (AActor* HitActor = Hit.GetActor())
        {
            float Damage = Config.DamagePerShot;
            UGTA7HitDetection::ApplyDamage(HitActor, Damage, OwnerCharacter,
                Hit.Location, Dir, Hit.BoneName, Hit.PhysMaterial.Get());
        }
    }
}
