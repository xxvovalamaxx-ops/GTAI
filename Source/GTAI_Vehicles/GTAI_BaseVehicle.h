// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "GTAI_VehicleTypes.h"

#include "GTAI_BaseVehicle.generated.h"

class UChaosWheeledVehicleSimulationComponent;
class USpringArmComponent;
class UCameraComponent;
class UGTAI_VehicleDamageComponent;

/**
 * Base vehicle pawn for all GTAI vehicles.
 * Uses UE5.8 Chaos physics via AWheeledVehiclePawn.
 * Subclass in Blueprint for per-vehicle tuning + visual.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class GTAI_VEHICLES_API AGTAI_BaseVehicle : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    AGTAI_BaseVehicle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

    // --- Class / Identity ---

    /** Which handling profile to use. Set per-Blueprint. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GTAI|Vehicle")
    EGTAI_VehicleClass VehicleClass = EGTAI_VehicleClass::Sedan;

    /** Current control mode (set by entry/exit system). */
    UPROPERTY(BlueprintReadOnly, Category = "GTAI|Vehicle")
    EGTAI_VehicleControlMode ControlMode = EGTAI_VehicleControlMode::Idle;

    // --- Components ---

    /** Damage tracking (mechanical + visual). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GTAI|Components")
    TObjectPtr<UGTAI_VehicleDamageComponent> DamageComponent;

    /** Third-person chase camera boom. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GTAI|Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    /** Chase camera. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GTAI|Camera")
    TObjectPtr<UCameraComponent> ChaseCamera;

    // --- Entry / Exit ---

    /** Called when any seat changes occupant. */
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Vehicle")
    FGTAI_OnVehicleOccupantChanged OnOccupantChanged;

    /** Called when vehicle becomes wrecked. */
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Vehicle")
    FGTAI_OnVehicleWrecked OnWrecked;

    /** Attempt to enter this vehicle from a specific seat. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Vehicle")
    virtual bool EnterVehicle(APawn* Pawn, EGTAI_VehicleSeat Seat = EGTAI_VehicleSeat::Driver);

    /** Exit the vehicle (current driver). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Vehicle")
    virtual void ExitVehicle();

    /** Get current driver pawn (null if unoccupied). */
    UFUNCTION(BlueprintPure, Category = "GTAI|Vehicle")
    APawn* GetDriver() const { return DriverPawn; }

    // --- Handling Helpers ---

    /** Apply arcade-style handling modifications based on VehicleClass. */
    UFUNCTION(BlueprintImplementableEvent, Category = "GTAI|Vehicle|Handling")
    void ApplyHandlingProfile();

    /** Toggle a driver-assist system at runtime. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Vehicle|Handling")
    void ToggleAssist(EGTAI_VehicleAssist Assist, bool bEnabled);

    // --- Input ---

    /** Enhanced Input integration — called by PlayerController. */
    void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
        bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse,
        const FHitResult& Hit) override;

    /** Current driver. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Vehicle")
    TObjectPtr<APawn> DriverPawn;

    /** Assist toggles. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Vehicle|Handling")
    TMap<EGTAI_VehicleAssist, bool> AssistStates;

    // --- Camera tuning (Blueprint-editable) ---

    UPROPERTY(EditAnywhere, Category = "GTAI|Camera")
    float CameraBoomLength = 600.f;

    UPROPERTY(EditAnywhere, Category = "GTAI|Camera")
    float CameraLagSpeed = 10.f;

    UPROPERTY(EditAnywhere, Category = "GTAI|Camera")
    float MaxSpeedFOV = 90.f;

    UPROPERTY(EditAnywhere, Category = "GTAI|Camera")
    float MinSpeedFOV = 70.f;

    /** Update camera FOV based on current speed. */
    void UpdateCameraFOV(float DeltaSeconds);
};
