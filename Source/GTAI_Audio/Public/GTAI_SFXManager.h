// GTAI_SFXManager.h
// SFX pipeline: weapons (GTAI_Combat hooks), vehicles (GTAI_Vehicles hooks),
// impacts (surface-aware footsteps, glass, metal), UI sounds, and ambient
// one-shots (sirens, gunshot echoes). Combines authored USoundCue assets with
// MetaSound procedural layers (engine note, weapon transients, tails). All
// world SFX route through spatialization + the reverb submix.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "GTAI_AudioTypes.h"
// Vehicle class enum for procedural engine notes.
#include "GTAI_VehicleTypes.h"
#include "GTAI_SFXManager.generated.h"

class USoundBase;
class UMetaSoundSource;

UENUM(BlueprintType)
enum class EGTAI_SFXCategory : uint8
{
	WeaponShot, WeaponReload, WeaponDryFire, WeaponHitFlesh, WeaponHitMetal, Explosion,
	VehicleEngine, VehicleTire, VehicleCollision, VehicleHorn, VehicleDoor,
	Footstep, Bodyfall, GlassBreak, MetalClang,
	UI_Nav, UI_Confirm, UI_Back, UI_Notification, UI_MapPing, PhoneRing, Cash,
	SirenPass, CrowdCheer, GunshotEcho
};

UCLASS()
class GTAI_AUDIO_API UGTAI_SFXManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Play a categorized SFX at a world location (spatialized).
	UFUNCTION(BlueprintCallable, Category = "GTAI|SFX")
	void PlaySFX(EGTAI_SFXCategory Category, const FVector& Location,
	             FGameplayTag SurfaceType = FGameplayTag::EmptyTag);

	// Play a non-spatial UI sound (UI bus, never ducked).
	UFUNCTION(BlueprintCallable, Category = "GTAI|SFX")
	void PlayUI(EGTAI_SFXCategory Category);

	// Procedural vehicle engine: continuous MetaSound driven by RPM/load.
	UFUNCTION(BlueprintCallable, Category = "GTAI|SFX")
	UAudioComponent* StartEngineNote(EGTAI_VehicleClass VehicleClass, USceneComponent* AttachTo);
	UFUNCTION(BlueprintCallable, Category = "GTAI|SFX")
	void UpdateEngineNote(UAudioComponent* Engine, float RPM, float Load, float Throttle, int32 Gear);

	// Bind a MetaSound procedural weapon layer to a weapon type.
	UFUNCTION(BlueprintCallable, Category = "GTAI|SFX")
	void RegisterProceduralLayer(EGTAI_SFXCategory Category, UMetaSoundSource* MetaSound);

protected:
	// Cue table: (Category, Surface) -> USoundBase, populated from SFXDataAsset.
	TMap<FString, TObjectPtr<USoundBase>> CueTable;

	// Procedural MetaSound overrides per category.
	TMap<uint8, TObjectPtr<UMetaSoundSource>> ProceduralLayers;
};
