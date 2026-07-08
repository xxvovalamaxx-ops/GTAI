// GTA7WeaponBase.h
// Abstract weapon base. Subclassed in Blueprint for per-weapon config.
// Hitscan / Projectile / Melee derive from this.
// Namespace: GTA7::Combat
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GTA7CombatTypes.h"
#include "GTA7WeaponBase.generated.h"

class UGTA7HitDetection;
class AGTA7Character;

UENUM(BlueprintType)
enum class EGTA7FireState : uint8
{
    Idle, Firing, Reloading
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAmmoChanged, int32, Magazine);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReloadStart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReloadEnd);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFire, const FVector&, MuzzleLocation);

/**
 * UGTA7WeaponBase — extensible weapon contract.
 * Config (FGTA7WeaponConfig) authored in Blueprint children / DataAssets.
 * Core fire/reload/ammo/spread logic is C++; visuals in Blueprint.
 */
UCLASS(Blueprintable, BlueprintType, Abstract)
class GTAI_COMBAT_API UGTA7WeaponBase : public UObject
{
    GENERATED_BODY()

public:
    UGTA7WeaponBase();

    // Called when equipped by a character.
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void OnEquip(AGTA7Character* InOwner);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void OnUnequip();

    // Input entry points (bound by character from Enhanced Input actions).
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StartFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StopFire();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void StartReload();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void CancelReload();

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual void SetAiming(bool bAiming);

    // ---- Config (Blueprint-assigned) ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Weapon")
    FGTA7WeaponConfig Config;

    // ---- Ammo ----
    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    int32 CurrentMagazine = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    int32 CurrentReserve = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    EGTA7FireState FireState = EGTA7FireState::Idle;

    // ---- Events ----
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnAmmoChanged OnAmmoChanged;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnReloadStart OnReloadStart;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnReloadEnd OnReloadEnd;

    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnFire OnFire;

protected:
    // Per-shot entry: subclasses implement the actual damage delivery.
    UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
    void PerformShot(const FVector& MuzzleLocation, const FRotator& AimRotation);
    virtual void PerformShot_Implementation(const FVector& MuzzleLocation, const FRotator& AimRotation) {}

    // Spread cone (degrees) given current locomotion/aim state.
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual float GetCurrentSpread() const;

    // Resolves the muzzle world transform from the owning character's camera/weapon socket.
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    virtual bool GetMuzzleTransform(FVector& OutLocation, FRotator& OutRotation) const;

    void ConsumeAmmo();
    void BeginReloadInternal();
    void FinishReloadInternal();

    UPROPERTY()
    TWeakObjectPtr<AGTA7Character> OwnerCharacter;

    UPROPERTY()
    bool bIsAiming = false;

    UPROPERTY()
    float LastFireTime = -999.f;

    FTimerHandle FireTimerHandle;
    FTimerHandle ReloadTimerHandle;
};
