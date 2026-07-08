// Copyright GTAI. All Rights Reserved.
// STRIKE — Player Character Implementation

#include "GTA7Character.h"
#include "GTA7CombatTypes.h"
#include "GTA7CameraComponent.h"
#include "GTA7CoverSystem.h"
#include "GTA7DamageSystem.h"
#include "GTA7WeaponBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

AGTA7Character::AGTA7Character(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    GetCharacterMovement()->MaxWalkSpeed = 600.f; // Walk
    GetCharacterMovement()->MaxWalkSpeedCrouched = 200.f;
    GetCharacterMovement()->JumpZVelocity = 500.f;
    GetCharacterMovement()->AirControl = 0.3f;

    GTA7Camera = CreateDefaultSubobject<UGTA7CameraComponent>(TEXT("GTA7Camera"));
    CoverSystem = CreateDefaultSubobject<UGTA7CoverSystem>(TEXT("CoverSystem"));
    DamageSystem = CreateDefaultSubobject<UGTA7DamageSystem>(TEXT("DamageSystem"));
}

void AGTA7Character::BeginPlay() { Super::BeginPlay(); }
void AGTA7Character::Tick(float DeltaTime) { Super::Tick(DeltaTime); UpdateLocomotionState(); }
void AGTA7Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) { Super::SetupPlayerInputComponent(PlayerInputComponent); }

void AGTA7Character::Move(const FVector2D& Input)
{
    if (!Controller) return;
    FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
    FVector Fwd = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
    FVector Rgt = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
    AddMovementInput(Fwd, Input.Y);
    AddMovementInput(Rgt, Input.X);
}

void AGTA7Character::SetSprinting(bool bSprinting)
{
    bIsSprinting = bSprinting;
    GetCharacterMovement()->MaxWalkSpeed = bSprinting ? 1200.f : 600.f;
}

void AGTA7Character::ToggleCrouch()
{
    if (bIsCrouched) UnCrouch(); else Crouch();
}

void AGTA7Character::TryVaultOrJump()
{
    FVector Ledge;
    if (DetectMantle(Ledge)) { /** TODO: Mantle animation */ return; }
    Jump();
}

void AGTA7Character::EquipWeapon(UGTA7WeaponBase* Weapon)
{
    if (!Weapon) return;
    WeaponInventory.Add(Weapon->GetWeaponSlot(), Weapon);
    CurrentWeapon = Weapon;
    Weapon->OnEquipped(this);
}

void AGTA7Character::SwitchToSlot(EGTA7WeaponSlot Slot)
{
    if (TObjectPtr<UGTA7WeaponBase>* Found = WeaponInventory.Find(Slot))
    {
        if (CurrentWeapon) CurrentWeapon->OnUnequipped();
        CurrentWeapon = *Found;
        CurrentWeapon->OnEquipped(this);
    }
}

void AGTA7Character::UpdateLocomotionState()
{
    CurrentSpeed = GetVelocity().Size2D();
    if (!GetCharacterMovement()->IsMovingOnGround()) LocomotionState = EGTA7LocomotionState::InAir;
    else if (CurrentSpeed < 10.f) LocomotionState = EGTA7LocomotionState::Idle;
    else if (bIsSprinting) LocomotionState = EGTA7LocomotionState::Sprinting;
    else if (bIsCrouched) LocomotionState = EGTA7LocomotionState::Crouching;
    else LocomotionState = EGTA7LocomotionState::Walking;
}

bool AGTA7Character::DetectMantle(FVector& OutLedge) const
{
    FVector Start = GetActorLocation() + FVector(0, 0, MantleMinHeight);
    FVector End = Start + GetActorForwardVector() * 100.f;
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
    {
        float LedgeHeight = Hit.Location.Z - GetActorLocation().Z;
        if (LedgeHeight >= MantleMinHeight && LedgeHeight <= MantleMaxHeight)
        {
            OutLedge = Hit.Location;
            return true;
        }
    }
    return false;
}
