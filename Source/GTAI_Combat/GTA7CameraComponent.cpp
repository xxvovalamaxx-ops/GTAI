// Copyright GTAI. All Rights Reserved.
// STRIKE — Camera Component Implementation (GTA-style orbit + aim)

#include "GTA7CameraComponent.h"
#include "GTA7Character.h"
#include "GTA7CombatTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void UGTA7CameraComponent::BeginPlay()
{
    OwnerCharacter = Cast<AGTA7Character>(GetOwner());
    if (OwnerCharacter)
    {
        SpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
        Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();

        if (SpringArm)
        {
            DefaultArmLength = SpringArm->TargetArmLength;
            SpringArm->bEnableCameraLag = true;
            SpringArm->CameraLagSpeed = 8.f;
            SpringArm->bEnableCameraRotationLag = true;
            SpringArm->CameraRotationLagSpeed = 10.f;
            SpringArm->bDoCollisionTest = true;
            SpringArm->ProbeSize = 8.f;
        }

        if (Camera) DefaultFOV = Camera->FieldOfView;
    }
}

void UGTA7CameraComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!SpringArm || !Camera) return;

    // FOV change based on speed
    if (OwnerCharacter)
    {
        float Speed = OwnerCharacter->GetCharacterMovement()->Velocity.Size2D();
        float SpeedRatio = FMath::Clamp(Speed / 500.f, 0.f, 1.f);
        float TargetFOV = FMath::Lerp(DefaultFOV, DefaultFOV + 15.f, SpeedRatio);
        Camera->FieldOfView = FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaTime, 2.f);
    }

    // Aim mode — tighter camera, lower FOV
    float TargetLength = bIsAiming ? DefaultArmLength * 0.4f : DefaultArmLength;
    SpringArm->TargetArmLength = FMath::FInterpTo(
        SpringArm->TargetArmLength, TargetLength, DeltaTime, 10.f);

    float AimFOV = bIsAiming ? DefaultFOV * 0.7f : TargetFOV;
    if (bIsAiming)
        Camera->FieldOfView = FMath::FInterpTo(Camera->FieldOfView, AimFOV, DeltaTime, 15.f);

    // Shoulder swap — offset camera boom socket
    ShoulderOffset = FMath::FInterpTo(ShoulderOffset, bRightShoulder ? 80.f : -80.f, DeltaTime, 8.f);
    if (SpringArm) SpringArm->SocketOffset.Y = ShoulderOffset;
}

void UGTA7CameraComponent::SetAiming(bool bAiming)
{
    bIsAiming = bAiming;
}

void UGTA7CameraComponent::SwapShoulder()
{
    bRightShoulder = !bRightShoulder;
}

void UGTA7CameraComponent::SetCameraMode(EGTA7_CameraMode NewMode)
{
    CurrentMode = NewMode;
    switch (NewMode)
    {
    case EGTA7_CameraMode::ThirdPerson:
        bIsAiming = false;
        if (SpringArm) SpringArm->TargetArmLength = DefaultArmLength;
        break;
    case EGTA7_CameraMode::Aiming:
        bIsAiming = true;
        if (SpringArm) SpringArm->TargetArmLength = DefaultArmLength * 0.4f;
        break;
    case EGTA7_CameraMode::Vehicle:
        if (SpringArm) SpringArm->TargetArmLength = DefaultArmLength * 1.5f; // Wider for driving
        break;
    }
}
