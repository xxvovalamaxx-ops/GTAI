// Copyright GTAI. All Rights Reserved.
// STRIKE — Weapon Base Implementation (GTA-like arcade weapons)

#include "GTA7WeaponBase.h"
#include "GTA7Character.h"
#include "GTA7CombatTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UGTA7WeaponBase::StartFire()
{
    if (!OwnerCharacter || CurrentAmmo <= 0) return;
    if (bIsFiring) return;

    bIsFiring = true;

    // Fire rate control
    float FireDelay = 60.f / Config.RoundsPerMinute;
    GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &UGTA7WeaponBase::Fire, FireDelay, true);
    Fire(); // First shot immediately
}

void UGTA7WeaponBase::StopFire()
{
    bIsFiring = false;
    if (FireTimerHandle.IsValid())
        GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
}

void UGTA7WeaponBase::StartReload()
{
    if (CurrentAmmo >= Config.MagSize || bIsReloading) return;
    if (ReserveAmmo <= 0) return; // No ammo to reload

    bIsReloading = true;
    GetWorld()->GetTimerManager().SetTimer(ReloadTimerHandle, this, &UGTA7WeaponBase::FinishReload, Config.ReloadTime, false);
}

void UGTA7WeaponBase::FinishReload()
{
    int32 Needed = Config.MagSize - CurrentAmmo;
    int32 Available = FMath::Min(Needed, ReserveAmmo);
    CurrentAmmo += Available;
    ReserveAmmo -= Available;
    bIsReloading = false;
}

void UGTA7WeaponBase::OnEquipped(AGTA7Character* Character)
{
    OwnerCharacter = Character;
}

void UGTA7WeaponBase::OnUnequipped()
{
    StopFire();
    OwnerCharacter = nullptr;
}

void UGTA7WeaponBase::Fire()
{
    if (CurrentAmmo <= 0)
    {
        StopFire();
        // Auto-reload when empty
        if (ReserveAmmo > 0) StartReload();
        return;
    }

    CurrentAmmo--;
    PerformShot();
}

void UGTA7WeaponBase::PerformShot()
{
    // Apply recoil to owner character
    if (OwnerCharacter)
    {
        if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
        {
            float RecoilPitch = FMath::FRandRange(-Config.RecoilVertical * 0.6f, -Config.RecoilVertical);
            float RecoilYaw = FMath::FRandRange(-Config.RecoilHorizontal, Config.RecoilHorizontal);
            PC->AddPitchInput(RecoilPitch);
            PC->AddYawInput(RecoilYaw);
        }
    }

    // Spread (bloom)
    CurrentSpread = FMath::Min(CurrentSpread + Config.SpreadIncrease, Config.MaxSpread);

    // Actual hit detection delegated to subclass (HitscanWeapon, ProjectileWeapon)
    OnShotFired();
}

void UGTA7WeaponBase::TickSpread(float DeltaTime)
{
    if (!bIsFiring)
    {
        // Spread recovery when not firing
        CurrentSpread = FMath::FInterpTo(CurrentSpread, 0.f, DeltaTime, Config.SpreadRecoveryRate);
    }
}

FVector UGTA7WeaponBase::GetFireDirection() const
{
    if (!OwnerCharacter) return FVector::ForwardVector;

    if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
    {
        FRotator ControlRot = PC->GetControlRotation();
        // Apply spread
        ControlRot.Pitch += FMath::FRandRange(-CurrentSpread, CurrentSpread);
        ControlRot.Yaw += FMath::FRandRange(-CurrentSpread, CurrentSpread);
        return ControlRot.Vector();
    }

    return OwnerCharacter->GetActorForwardVector();
}

bool UGTA7WeaponBase::GetMuzzleTransform(FVector& OutLocation, FVector& OutDirection) const
{
    OutLocation = OwnerCharacter ? OwnerCharacter->GetActorLocation() + FVector(100.f, 0.f, 70.f) : FVector::ZeroVector;
    OutDirection = GetFireDirection();
    return true;
}
