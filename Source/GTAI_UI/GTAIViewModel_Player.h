// GTAIViewModel_Player.h
// MVVM ViewModel: player vital/inventory state. Exposes FieldNotify properties
// so bound widgets update ONLY on change (event-driven, never per-frame bind).
// See design doc sections 1.3, 2.2-2.5, 2.8. Under namespace GTAI::UI.
#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GTAI_UI.h"
#include "GTAIViewModel_Player.generated.h"

UCLASS()
class GTAIUI_API UGTAIViewModel_Player : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    // ---- Health ----
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    float GetHealth() const { return Health; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetHealth(float InValue);

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

    // ---- Armor ----
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    float GetArmor() const { return Armor; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetArmor(float InValue);

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    float GetArmorPercent() const { return MaxArmor > 0.f ? Armor / MaxArmor : 0.f; }

    // ---- Money ----
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    int32 GetCash() const { return Cash; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetCash(int32 InValue);

    /** Formatted "$12,450" string (FieldNotify so the counter rebinds). */
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    FText GetCashString() const;

    // ---- Weapon ----
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    FText GetWeaponName() const { return WeaponName; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetWeaponName(const FText& InName);

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    FText GetAmmoString() const { return AmmoString; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetAmmo(int32 Current, int32 Max);

    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    bool IsInVehicle() const { return bInVehicle; }
    UFUNCTION(BlueprintCallable, FieldNotify, Setter)
    void SetInVehicle(bool bValue) { bInVehicle = bValue; BroadcastFieldValueChanged(GET_MEMBER_NAME_CHECKED(UGTAIViewModel_Player, bInVehicle)); }

    /** True when low health threshold crossed (drives pulse animation). */
    UFUNCTION(BlueprintCallable, FieldNotify, Getter)
    bool IsLowHealth() const { return HealthPercent < 0.3f; }

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GTAI|Balance")
    float MaxHealth = 100.f;

    UPROPERTY(EditDefaultsOnly, Category = "GTAI|Balance")
    float MaxArmor = 100.f;

    float Health = 100.f;
    float Armor = 0.f;
    int32 Cash = 0;
    FText WeaponName;
    FText AmmoString;
    bool bInVehicle = false;
};
