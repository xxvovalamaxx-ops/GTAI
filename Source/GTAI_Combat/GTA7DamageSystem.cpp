// Copyright GTAI. All Rights Reserved.
// STRIKE — Damage System Implementation

#include "GTA7DamageSystem.h"
#include "GTA7Character.h"
#include "GTA7CombatTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

UGTA7DamageSystem::UGTA7DamageSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UGTA7DamageSystem::BeginPlay()
{
    Super::BeginPlay();
    CurrentHealth = MaxHealth;
    CurrentArmor = MaxArmor;
}

void UGTA7DamageSystem::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Health regeneration (slow, only when not recently damaged)
    if (CurrentHealth < MaxHealth && TimeSinceLastDamage > RegenDelay)
    {
        CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + RegenRate * DeltaTime);
        OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
    }

    TimeSinceLastDamage += DeltaTime;
}

void UGTA7DamageSystem::TakeDamage(float Damage, AActor* Instigator,
    const FVector& HitLocation, const FVector& ShotDirection)
{
    if (bIsDead) return;

    TimeSinceLastDamage = 0.f;

    // Armor absorbs damage first
    float ArmorAbsorption = FMath::Min(CurrentArmor, Damage * ArmorEfficiency);
    CurrentArmor -= ArmorAbsorption;
    float HealthDamage = Damage - ArmorAbsorption;

    // Apply to health
    CurrentHealth -= HealthDamage;

    OnDamageTaken.Broadcast(Damage, Instigator, HitLocation, ShotDirection);
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

    if (CurrentHealth <= 0.f)
    {
        Die(Instigator);
    }
}

void UGTA7DamageSystem::Heal(float Amount)
{
    CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
    OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UGTA7DamageSystem::AddArmor(float Amount)
{
    CurrentArmor = FMath::Min(MaxArmor, CurrentArmor + Amount);
}

void UGTA7DamageSystem::Die(AActor* Killer)
{
    bIsDead = true;
    CurrentHealth = 0.f;
    OnDeath.Broadcast(Killer);

    if (AGTA7Character* Char = Cast<AGTA7Character>(GetOwner()))
    {
        // Disable character
        Char->GetCharacterMovement()->DisableMovement();
        Char->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        // Ragdoll
        Char->GetMesh()->SetSimulatePhysics(true);
        Char->GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    }

    // Respawn timer — GTA-style "Wasted" screen
    GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this,
        &UGTA7DamageSystem::Respawn, RespawnTime, false);

    OnRespawnAvailable.Broadcast(RespawnTime);
}

void UGTA7DamageSystem::Respawn()
{
    bIsDead = false;
    CurrentHealth = MaxHealth * RespawnHealthPercent;
    CurrentArmor = 0.f;

    if (AGTA7Character* Char = Cast<AGTA7Character>(GetOwner()))
    {
        Char->GetMesh()->SetSimulatePhysics(false);
        Char->GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
        Char->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Char->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

        // Teleport to nearest hospital / safe location (placeholder: spawn point)
        Char->SetActorLocation(LastSafeLocation);
    }

    OnRespawned.Broadcast();
}

float UGTA7DamageSystem::GetHealthPercent() const
{
    return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f;
}

float UGTA7DamageSystem::GetArmorPercent() const
{
    return MaxArmor > 0.f ? CurrentArmor / MaxArmor : 0.f;
}

void UGTA7DamageSystem::SetLastSafeLocation(const FVector& Location)
{
    LastSafeLocation = Location;
}
