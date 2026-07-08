// Copyright GTAI. All Rights Reserved.
// STRIKE — Hit Detection Implementation

#include "GTA7HitDetection.h"
#include "GTA7CombatTypes.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

void UGTA7HitDetection::ApplyDamage(AActor* Target, float BaseDamage, AActor* Instigator,
    const FVector& HitLocation, const FVector& ShotDirection, FName BoneName, UPhysicalMaterial* PhysMat)
{
    if (!Target) return;

    // Bone → Hit Zone mapping
    EGTA7HitZone Zone = BoneToZone(BoneName);

    // Damage multiplier by zone
    float Multiplier = 1.f;
    switch (Zone)
    {
    case EGTA7HitZone::Head:     Multiplier = 2.5f; break;
    case EGTA7HitZone::Neck:     Multiplier = 2.0f; break;
    case EGTA7HitZone::UpperChest: Multiplier = 1.2f; break;
    case EGTA7HitZone::LowerChest: Multiplier = 1.0f; break;
    case EGTA7HitZone::Abdomen:  Multiplier = 1.0f; break;
    case EGTA7HitZone::LeftArm:
    case EGTA7HitZone::RightArm: Multiplier = 0.7f; break;
    case EGTA7HitZone::LeftLeg:
    case EGTA7HitZone::RightLeg: Multiplier = 0.6f; break;
    case EGTA7HitZone::Foot:
    case EGTA7HitZone::Hand:     Multiplier = 0.3f; break;
    default: Multiplier = 1.0f;
    }

    float FinalDamage = BaseDamage * Multiplier;

    // Apply via damage system (delegates to character's UGTA7DamageSystem)
    if (AGTA7Character* Char = Cast<AGTA7Character>(Target))
    {
        if (Char->DamageSystem)
        {
            Char->DamageSystem->TakeDamage(FinalDamage, Instigator, HitLocation, ShotDirection);
        }
    }
    // For non-character actors (vehicles, props), we'll add vehicle damage later
}

EGTA7HitZone UGTA7HitDetection::BoneToZone(FName BoneName)
{
    FString Bone = BoneName.ToString().ToLower();

    if (Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("skull")))     return EGTA7HitZone::Head;
    if (Bone.Contains(TEXT("neck")) || Bone.Contains(TEXT("spine1")))    return EGTA7HitZone::Neck;
    if (Bone.Contains(TEXT("spine2")) || Bone.Contains(TEXT("spine3")))  return EGTA7HitZone::UpperChest;
    if (Bone.Contains(TEXT("spine")))                                    return EGTA7HitZone::LowerChest;
    if (Bone.Contains(TEXT("pelvis")))                                   return EGTA7HitZone::Abdomen;
    if (Bone.Contains(TEXT("upperarm")) || Bone.Contains(TEXT("clavicle")) ||
        Bone.Contains(TEXT("shoulder")))
    {
        return Bone.Contains(TEXT("l")) || Bone.Contains(TEXT("left")) ?
            EGTA7HitZone::LeftArm : EGTA7HitZone::RightArm;
    }
    if (Bone.Contains(TEXT("lowerarm")) || Bone.Contains(TEXT("forearm")) ||
        Bone.Contains(TEXT("elbow")))
    {
        return Bone.Contains(TEXT("l")) || Bone.Contains(TEXT("left")) ?
            EGTA7HitZone::LeftArm : EGTA7HitZone::RightArm;
    }
    if (Bone.Contains(TEXT("thigh")) || Bone.Contains(TEXT("calf")) ||
        Bone.Contains(TEXT("knee")))
    {
        return Bone.Contains(TEXT("l")) || Bone.Contains(TEXT("left")) ?
            EGTA7HitZone::LeftLeg : EGTA7HitZone::RightLeg;
    }
    if (Bone.Contains(TEXT("foot")) || Bone.Contains(TEXT("toe")))
        return EGTA7HitZone::Foot;
    if (Bone.Contains(TEXT("hand")) || Bone.Contains(TEXT("finger")) || Bone.Contains(TEXT("thumb")))
        return EGTA7HitZone::Hand;

    return EGTA7HitZone::LowerChest; // Default
}
