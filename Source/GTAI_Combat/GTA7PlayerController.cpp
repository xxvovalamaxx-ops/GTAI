// Copyright GTAI. All Rights Reserved.
// STRIKE — Player Controller with Enhanced Input
// GTA-style TPS controller: walk/run/sprint/crouch/aim/cover

#include "GTA7PlayerController.h"
#include "GTA7Character.h"
#include "GTA7CombatTypes.h"
#include "GTA7WeaponBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"

AGTA7PlayerController::AGTA7PlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    CameraMode = EGTA7_CameraMode::ThirdPerson;
}

void AGTA7PlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        Subsystem->AddMappingContext(DefaultMappingContext, 0);
    }

    // Cache character reference
    CachedCharacter = Cast<AGTA7Character>(GetPawn());
}

void AGTA7PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // Movement
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGTA7PlayerController::OnMove);
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGTA7PlayerController::OnLook);

        // Actions
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AGTA7PlayerController::OnJump);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AGTA7PlayerController::OnSprintStart);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &AGTA7PlayerController::OnSprintEnd);
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AGTA7PlayerController::OnCrouch);

        // Combat
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &AGTA7PlayerController::OnAimStart);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &AGTA7PlayerController::OnAimEnd);
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AGTA7PlayerController::OnFireStart);
        EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AGTA7PlayerController::OnFireEnd);
        EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AGTA7PlayerController::OnReload);

        // Camera
        EnhancedInput->BindAction(ShoulderSwapAction, ETriggerEvent::Started, this, &AGTA7PlayerController::OnShoulderSwap);
    }
}

void AGTA7PlayerController::OnMove(const FInputActionValue& Value)
{
    if (!CachedCharacter.IsValid()) return;

    FVector2D MovementVector = Value.Get<FVector2D>();
    if (MovementVector.Size() > 1.f) MovementVector.Normalize();

    FRotator ControlRotation = GetControlRotation();
    FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);
    FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    CachedCharacter->AddMovementInput(Forward, MovementVector.Y);
    CachedCharacter->AddMovementInput(Right, MovementVector.X);
}

void AGTA7PlayerController::OnLook(const FInputActionValue& Value)
{
    FVector2D LookVector = Value.Get<FVector2D>();
    AddYawInput(LookVector.X);
    AddPitchInput(LookVector.Y);
}

void AGTA7PlayerController::OnJump()
{
    if (CachedCharacter.IsValid())
        CachedCharacter->Jump();
}

void AGTA7PlayerController::OnSprintStart()
{
    if (CachedCharacter.IsValid())
        CachedCharacter->GetCharacterMovement()->MaxWalkSpeed = 1200.f; // Sprint
}

void AGTA7PlayerController::OnSprintEnd()
{
    if (CachedCharacter.IsValid())
        CachedCharacter->GetCharacterMovement()->MaxWalkSpeed = 600.f; // Walk
}

void AGTA7PlayerController::OnCrouch()
{
    if (CachedCharacter.IsValid())
        CachedCharacter->Crouch();
}

void AGTA7PlayerController::OnAimStart()
{
    CameraMode = EGTA7_CameraMode::Aiming;
    // TODO: Switch to aim camera (ADS position, shoulder swap lock)
}

void AGTA7PlayerController::OnAimEnd()
{
    CameraMode = EGTA7_CameraMode::ThirdPerson;
    // TODO: Return to orbit camera
}

void AGTA7PlayerController::OnFireStart()
{
    if (CachedCharacter.IsValid() && CachedCharacter->GetCurrentWeapon())
    {
        CachedCharacter->GetCurrentWeapon()->StartFire();
    }
}

void AGTA7PlayerController::OnFireEnd()
{
    if (CachedCharacter.IsValid() && CachedCharacter->GetCurrentWeapon())
    {
        CachedCharacter->GetCurrentWeapon()->StopFire();
    }
}

void AGTA7PlayerController::OnReload()
{
    if (CachedCharacter.IsValid() && CachedCharacter->GetCurrentWeapon())
    {
        CachedCharacter->GetCurrentWeapon()->StartReload();
    }
}

void AGTA7PlayerController::OnShoulderSwap()
{
    // Toggle camera shoulder side
    bRightShoulder = !bRightShoulder;
    // TODO: Update camera boom socket offset
}
