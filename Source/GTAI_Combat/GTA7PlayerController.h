// GTA7PlayerController.h
// Player controller: owns Enhanced Input setup, look, camera mode switching,
// and weapon/cover command routing. Namespace: GTA7::Player
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GTA7CombatTypes.h"
#include "GTA7PlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UGTA7CameraComponent;
class UGTA7CoverSystem;
class UGTA7WeaponBase;

UCLASS()
class GTAI_COMBAT_API AGTA7PlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AGTA7PlayerController();

    // Enhanced Input context pushed on possession (priority 0).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TSoftObjectPtr<UInputMappingContext> VehicleMappingContext;

    // Input Actions (assigned in Blueprint / created as assets).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_Move;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_Look;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_Jump;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_Sprint;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_Crouch;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_Aim;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_Fire;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_Reload;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_SwapShoulder;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_TakeCover;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_WeaponSlot1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_WeaponSlot2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    TObjectPtr<UInputAction> IA_WeaponSlot3;

    // Camera mode control (called by character/weapon).
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraMode(EGTA7CameraMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SwapShoulder();

    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    void EnterVehicleMode();

    UFUNCTION(BlueprintCallable, Category = "Vehicle")
    void ExitVehicleMode();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // Bindings (Enhanced Input).
    void BindActions(UEnhancedInputComponent* EIC);
    void OnMove(const FInputActionInstance& Instance);
    void OnLook(const FInputActionInstance& Instance);
    void OnJump(const FInputActionInstance& Instance);
    void OnSprint(const FInputActionInstance& Instance);
    void OnCrouch(const FInputActionInstance& Instance);
    void OnAim(const FInputActionInstance& Instance);
    void OnFire(const FInputActionInstance& Instance);
    void OnReload(const FInputActionInstance& Instance);
    void OnSwapShoulder(const FInputActionInstance& Instance);
    void OnTakeCover(const FInputActionInstance& Instance);
    void OnWeaponSlot(int32 Slot, const FInputActionInstance& Instance);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    UGTA7CameraComponent* GetGTA7Camera() const;

    UPROPERTY()
    EGTA7CameraMode CurrentCameraMode = EGTA7CameraMode::Orbit;

    UPROPERTY()
    bool bRightShoulder = true;
};
