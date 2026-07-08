// GTA7CameraComponent.h
// Third-person orbit + aim + shoulder swap + vehicle blend.
// Wraps a USpringArmComponent. Namespace: GTA7::Player
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "GTA7CombatTypes.h"
#include "GTA7CameraComponent.generated.h"

UCLASS(ClassGroup = (GTAI), BlueprintType, meta = (BlueprintSpawnableComponent))
class GTAI_COMBAT_API UGTA7CameraComponent : public USpringArmComponent
{
    GENERATED_BODY()

public:
    UGTA7CameraComponent();

    // Mode switching (smoothly lerped).
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraMode(EGTA7CameraMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    EGTA7CameraMode GetCameraMode() const { return CurrentMode; }

    // Shoulder swap (peeking). Smoothly lerps target offset X.
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetShoulder(bool bRightShoulder);

    // Recoil impulse fed into control rotation (decays each frame).
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void AddRecoil(float PitchKick, float YawKick);

    // FOV punch (sprint/aim feel). Lerps to goal.
    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetFOVGoal(float FOV);

    // Per-mode defaults (BP-tunable).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
    float OrbitArmLength = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Orbit")
    float OrbitFOV = 75.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Aim")
    float AimArmLength = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Aim")
    float AimFOV = 55.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Aim")
    float AimShoulderOffset = 40.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Vehicle")
    float VehicleArmLength = 600.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Vehicle")
    float VehicleFOV = 70.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float ModeBlendTime = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float RecoilDecay = 8.f; // per second

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void ApplyModeDefaults();
    void UpdateRecoil(float DeltaTime);
    void UpdateFOV(float DeltaTime);

    UPROPERTY()
    EGTA7CameraMode CurrentMode = EGTA7CameraMode::Orbit;

    UPROPERTY()
    bool bRightShoulder = true;

    UPROPERTY()
    float CurrentFOV = 75.f;
    UPROPERTY()
    float GoalFOV = 75.f;

    UPROPERTY()
    FVector2D RecoilRemaining = FVector2D::ZeroVector; // x=pitch, y=yaw
};
