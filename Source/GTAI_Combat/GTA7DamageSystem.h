// GTA7DamageSystem.h
// Health / armor / regeneration / headshot model.
// Self-contained attribute-set-style system (Lyra-inspired, GAS optional).
// Namespace: GTA7::Combat
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTA7CombatTypes.h"
#include "GTA7DamageSystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, float, NewHealth, float, OldHealth, float, DamageTaken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnArmorChanged, float, NewArmor, float, OldArmor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDamageTaken, const FGTA7HitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

// Owning actor must implement this to receive damage from any source.
UINTERFACE(MinimalAPI, Blueprintable)
class UGTA7Damageable : public UInterface { GENERATED_BODY() };

class IGTA7Damageable
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, Category = "Combat")
    class UGTA7DamageSystem* GetDamageSystem() const;
};

/**
 * UGTA7DamageSystem — arcade health/armor model (GTA-style).
 * Armor soaks a fraction of damage before health; headshots multiply.
 * Health regen is optional and delayed; armor never regenerates.
 */
UCLASS(ClassGroup = (GTAI), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GTAI_COMBAT_API UGTA7DamageSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UGTA7DamageSystem();

    // ---- Configuration ----
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    float MaxHealth = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    float MaxArmor = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    float StartingHealth = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    float StartingArmor = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Regen")
    bool bHealthRegenEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Regen")
    float HealthRegenDelay = 5.f; // seconds after last damage

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Regen")
    float HealthRegenRate = 5.f;  // hp per second

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Regen")
    float HealthRegenCap = 100.f; // do not regen to full (arcade)

    // ---- State ----
    UPROPERTY(BlueprintReadOnly, Category = "Health")
    float Health = 100.f;

    UPROPERTY(BlueprintReadOnly, Category = "Health")
    float Armor = 0.f;

    // ---- API ----
    UFUNCTION(BlueprintCallable, Category = "Combat")
    FGTA7HitResult ApplyDamage(float InDamage, EGTA7HitZone Zone, AController* Instigator, AActor* DamageCauser = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void Heal(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AddArmor(float Amount);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool IsDead() const { return bIsDead; }

    // ---- Events ----
    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FOnHealthChanged OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FOnArmorChanged OnArmorChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FOnDamageTaken OnDamageTaken;

    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FOnDeath OnDeath;

    // Mirrors Lyra's OnOutOfHealth -> death sequence trigger.
    UPROPERTY(BlueprintAssignable, Category = "Combat")
    FOnDeath OnOutOfHealth;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void HandleRegen(float DeltaTime);
    void TriggerDeath(AController* Instigator);

    UPROPERTY()
    bool bIsDead = false;

    UPROPERTY()
    float TimeSinceLastDamage = 0.f;
};
