// GTA7HitDetection.h
// Hybrid hit detection: hitscan line traces + projectile overlap,
// bone->zone mapping for headshots. Namespace: GTA7::Combat
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GTA7CombatTypes.h"
#include "GTA7HitDetection.generated.h"

// Maps a skeleton's bones to damage zones. Configure per skeleton in Blueprint/DataAsset.
USTRUCT(BlueprintType)
struct FGTA7BoneZoneMap
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
    TMap<FName, EGTA7HitZone> BoneToZone;
};

UCLASS(Blueprintable, BlueprintType)
class GTAI_COMBAT_API UGTA7HitDetection : public UObject
{
    GENERATED_BODY()

public:
    // Single hitscan shot. Returns enriched hit result (zone + final damage applied).
    UFUNCTION(BlueprintCallable, Category = "Combat")
    static FGTA7HitResult TraceShot(
        UWorld* World,
        const FVector& Start,
        const FRotator& Direction,
        float SpreadDegrees,
        float Range,
        const FGTA7DamageConfig& DamageConfig,
        AActor* Instigator,
        AActor* DamageCauser,
        const FGTA7BoneZoneMap& BoneMap,
        int32 Pellets = 1);

    // Resolve which zone a bone belongs to (defaults Torso).
    UFUNCTION(BlueprintCallable, Category = "Combat")
    static EGTA7HitZone ResolveZone(const FName& BoneName, const FGTA7BoneZoneMap& BoneMap);

    // Apply the resolved hit to the target's damage system (if present).
    UFUNCTION(BlueprintCallable, Category = "Combat")
    static FGTA7HitResult ApplyHitToTarget(
        AActor* Target,
        float BaseDamage,
        EGTA7HitZone Zone,
        const FGTA7DamageConfig& DamageConfig,
        AController* Instigator,
        AActor* DamageCauser);

    // Compute final damage after zone multiplier + range falloff.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    static float ComputeDamage(float BaseDamage, EGTA7HitZone Zone, const FGTA7DamageConfig& DamageConfig, float Distance);
};
