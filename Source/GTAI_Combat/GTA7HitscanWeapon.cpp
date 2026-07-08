// Copyright GTAI. All Rights Reserved.
// STRIKE — Hitscan Weapon Implementation

#include "GTA7HitscanWeapon.h"
#include "GTA7Character.h"
#include "GTA7HitDetection.h"
#include "GTA7DamageSystem.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

void UGTA7HitscanWeapon::OnShotFired()
{
    FVector MuzzleLoc, MuzzleDir;
    GetMuzzleTransform(MuzzleLoc, MuzzleDir);

    FVector End = MuzzleLoc + MuzzleDir * MaxRange;

    // Line trace
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);
    Params.bReturnPhysicalMaterial = true;

    if (GetWorld()->LineTraceSingleByChannel(Hit, MuzzleLoc, End, ECC_GameTraceChannel1, Params))
    {
        // Hit something
        AActor* HitActor = Hit.GetActor();
        if (HitActor)
        {
            // Apply damage
            if (AGTA7Character* HitCharacter = Cast<AGTA7Character>(HitActor))
            {
                float FinalDamage = Config.DamagePerShot;
                // Headshot bonus
                if (Hit.BoneName == TEXT("head") || Hit.BoneName == TEXT("neck"))
                    FinalDamage *= 2.5f;

                UGTA7HitDetection::ApplyDamage(HitActor, FinalDamage, OwnerCharacter,
                    Hit.Location, MuzzleDir, Hit.BoneName, Hit.PhysMaterial.Get());
            }
        }

        // Visuals — tracers, impacts handled by BP/GameplayCue
    }

    if (bDebugTraces && GetWorld())
    {
        DrawDebugLine(GetWorld(), MuzzleLoc, End, FColor::Red, false, 1.f);
        if (Hit.bBlockingHit)
            DrawDebugPoint(GetWorld(), Hit.Location, 10.f, FColor::Yellow, false, 1.f);
    }
}
