// GTA7Character.h
// Player pawn: locomotion FSM (move/sprint/crouch/cover/parkour),
// weapon inventory, Enhanced-Input-aware movement relative to camera.
// Namespace: GTA7::Player
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GTA7CombatTypes.h"
#include "GTA7Character.generated.h"

class UGTA7CameraComponent;
class UGTA7CoverSystem;
class UGTA7DamageSystem;
class UGTA7WeaponBase;

UCLASS()
class GTAI_COMBAT_API AGTA7Character : public ACharacter
{
    GENERATED_BODY()

public:
    AGTA7Character(const FObjectInitializer& ObjectInitializer);

    // Components.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UGTA7CameraComponent> GTA7Camera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UGTA7CoverSystem> CoverSystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UGTA7DamageSystem> DamageSystem;

    // Locomotion state (read by AnimBP).
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    EGTA7LocomotionState LocomotionState = EGTA7LocomotionState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float CurrentSpeed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float LeanAmount = 0.f; // cover lean / strafe lean

    // Weapon inventory (Blueprint grants starting weapons).
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void EquipWeapon(UGTA7WeaponBase* Weapon);

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    UGTA7WeaponBase* GetCurrentWeapon() const { return CurrentWeapon; }

    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SwitchToSlot(EGTA7WeaponSlot Slot);

    // Called by PlayerController input bindings.
    UFUNCTION(BlueprintCallable, Category = "Locomotion")
    void Move(const FVector2D& Input);

    UFUNCTION(BlueprintCallable, Category = "Locomotion")
    void SetSprinting(bool bSprinting);

    UFUNCTION(BlueprintCallable, Category = "Locomotion")
    void ToggleCrouch();

    UFUNCTION(BlueprintCallable, Category = "Locomotion")
    void TryVaultOrJump();

    // Parkour tunables.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
    float MantleMinHeight = 40.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
    float MantleMaxHeight = 160.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
    float SlideDuration = 0.5f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void UpdateLocomotionState();
    bool DetectMantle(FVector& OutLedge) const;

    UPROPERTY()
    TMap<EGTA7WeaponSlot, TObjectPtr<UGTA7WeaponBase>> WeaponInventory;

    UPROPERTY()
    TObjectPtr<UGTA7WeaponBase> CurrentWeapon;

    UPROPERTY()
    bool bIsSprinting = false;
};
