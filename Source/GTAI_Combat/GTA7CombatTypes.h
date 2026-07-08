// GTA7CombatTypes.h
// Shared enums, structs, and trace channels for the GTAI_Combat module.
// Namespace: GTA7::Combat / GTA7::Player
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GTA7CombatTypes.generated.h"

namespace GTA7::Combat
{
    // Custom trace channel for weapon rays (set in DefaultEngine.ini -> TraceChannels).
    // ECC_GameTraceChannel1 reserved for Weapon.
}

UENUM(BlueprintType)
enum class EGTA7LocomotionState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Walk        UMETA(DisplayName = "Walk"),
    Jog         UMETA(DisplayName = "Jog"),
    Sprint      UMETA(DisplayName = "Sprint"),
    Crouch      UMETA(DisplayName = "Crouch"),
    Cover       UMETA(DisplayName = "Cover"),
    Parkour     UMETA(DisplayName = "Parkour"),
    InVehicle   UMETA(DisplayName = "In Vehicle"),
};

UENUM(BlueprintType)
enum class EGTA7CameraMode : uint8
{
    Orbit   UMETA(DisplayName = "Orbit"),
    Aim     UMETA(DisplayName = "Aim"),
    Vehicle UMETA(DisplayName = "Vehicle"),
};

UENUM(BlueprintType)
enum class EGTA7HitZone : uint8
{
    Torso    UMETA(DisplayName = "Torso"),
    Head     UMETA(DisplayName = "Head"),
    Limb     UMETA(DisplayName = "Limb"),
    Arms     UMETA(DisplayName = "Arms"),
};

UENUM(BlueprintType)
enum class EGTA7WeaponType : uint8
{
    Hitscan    UMETA(DisplayName = "Hitscan"),
    Projectile UMETA(DisplayName = "Projectile"),
    Melee      UMETA(DisplayName = "Melee"),
};

UENUM(BlueprintType)
enum class EGTA7WeaponSlot : uint8
{
    None UMETA(DisplayName = "None"),
    Pistol UMETA(DisplayName = "Pistol"),
    Rifle  UMETA(DisplayName = "Rifle"),
    Melee  UMETA(DisplayName = "Melee"),
};

// Damage tuning per damage profile. Authored as a DataAsset / in weapon Blueprints.
USTRUCT(BlueprintType)
struct FGTA7DamageConfig
{
    GENERATED_BODY()

    // Base damage before zone multipliers.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float BaseDamage = 25.f;

    // Multipliers by hit zone.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float HeadshotMultiplier = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float LimbMultiplier = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float ArmsMultiplier = 0.7f;

    // Armor soak: fraction of effective damage the body armor absorbs (0..1).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float ArmorAbsorb = 0.6f;

    // Optional falloff: damage lost per meter past EffectiveRange.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float EffectiveRange = 3000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    float RangeFalloffPerMeter = 0.0f;
};

// Per-weapon configuration. Exposed to Blueprint children for tuning without recompile.
USTRUCT(BlueprintType)
struct FGTA7WeaponConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    EGTA7WeaponType WeaponType = EGTA7WeaponType::Hitscan;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    EGTA7WeaponSlot Slot = EGTA7WeaponSlot::Pistol;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 MagazineSize = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 ReserveAmmo = 90;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float FireRate = 6.f; // shots per second

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    bool bFullAuto = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float ReloadTime = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float Range = 5000.f;

    // Spread in degrees at the muzzle (cone half-angle).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float BaseSpread = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float MovingSpreadBonus = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float CrouchSpreadPenalty = -1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    float AimSpreadMultiplier = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
    FGTA7DamageConfig Damage;

    // Pellets per shot (shotgun > 1).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    int32 Pellets = 1;

    // Projectile class for projectile weapons.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    TSoftClassPtr<class AGTA7Projectile> ProjectileClass;

    // Recoil curve (time -> 2D pitch/yaw kick). Optional.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    TSoftObjectPtr<class UCurveVector> RecoilCurve;

    // Visual / audio (Blueprint-assigned).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
    TSoftObjectPtr<class USkeletalMesh> WeaponMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
    TSoftClassPtr<class UAnimInstance> AnimBlueprint;
};

// Result of a hit, enriched with zone + damage for the damage system.
USTRUCT(BlueprintType)
struct FGTA7HitResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Hit")
    FHitResult TraceHit;

    UPROPERTY(BlueprintReadOnly, Category = "Hit")
    EGTA7HitZone Zone = EGTA7HitZone::Torso;

    UPROPERTY(BlueprintReadOnly, Category = "Hit")
    float FinalDamage = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Hit")
    bool bKilled = false;
};
