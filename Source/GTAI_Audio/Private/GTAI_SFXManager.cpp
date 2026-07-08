// Copyright GTAI. All Rights Reserved.
// AUDIO — GTAI_SFXManager
// SFX pipeline: weapons (GTAI_Combat hooks), vehicles (GTAI_Vehicles hooks),
// impacts (surface-aware footsteps/glass/metal), UI sounds, and ambient
// one-shots (sirens, gunshot echoes). Combines authored USoundCue assets (via
// the CueTable) with MetaSound procedural layers (engine note, weapon
// transients, tails). All world SFX route through spatialization; UI sounds are
// non-spatial and never ducked (per GTAI_AudioManager design).
//
// Population of CueTable / ProceduralLayers is data-driven from a designer
// SFXDataAsset. Until that asset exists, both maps start empty and the manager
// logs a one-time warning; gameplay still runs (just silent) and binds to
// assets hot-reloaded later without code changes.

#include "GTAI_SFXManager.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "MetaSoundSource.h"

#include "GTAI_VehicleTypes.h"

namespace
{
	// Base oscillator frequency multiplier per vehicle class — sports cars rev
	// higher/brighter, trucks lower/rougher. Applied to the MetaSound "RPM" ->
	// pitch mapping by the engine-note graph.
	float VehicleBaseFreqScale(EGTAI_VehicleClass VehicleClass)
	{
		switch (VehicleClass)
		{
		case EGTAI_VehicleClass::SportsCar:  return 1.35f;
		case EGTAI_VehicleClass::Truck:      return 0.70f;
		case EGTAI_VehicleClass::Traffic:    return 0.85f;
		case EGTAI_VehicleClass::Sedan:     return 1.0f;
		default:                             return 1.0f;
		}
	}

	FString CueKey(EGTAI_SFXCategory Category, FGameplayTag SurfaceType)
	{
		if (SurfaceType.IsValid())
		{
			return FString::Printf(TEXT("%d_%s"),
				static_cast<uint8>(Category), *SurfaceType.ToString());
		}
		return FString::Printf(TEXT("%d"), static_cast<uint8>(Category));
	}
}

void UGTAI_SFXManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Designer wires cues here via a SFXDataAsset (CueTable) and procedural
	// MetaSound layers (ProceduralLayers). Attempt to load a default asset path;
	// missing asset is non-fatal (silent until populated).
	static bool bWarnedEmpty = false;
	if (CueTable.Num() == 0 && ProceduralLayers.Num() == 0 && !bWarnedEmpty)
	{
		bWarnedEmpty = true;
		UE_LOG(LogTemp, Warning,
			TEXT("GTAI_SFXManager: CueTable + ProceduralLayers empty — no SFXDataAsset loaded. "
				 "SFX will be silent until cues are registered (runtime or via data asset)."));
	}

	UE_LOG(LogTemp, Log, TEXT("GTAI_SFXManager initialized (%d cues, %d procedural layers)."),
		CueTable.Num(), ProceduralLayers.Num());
}

void UGTAI_SFXManager::PlaySFX(EGTAI_SFXCategory Category, const FVector& Location,
                                FGameplayTag SurfaceType)
{
	// Prefer a procedural MetaSound layer if one is bound for this category.
	if (const TObjectPtr<UMetaSoundSource>* Proc = ProceduralLayers.Find(static_cast<uint8>(Category)))
	{
		if (UMetaSoundSource* MS = Proc->Get())
		{
			if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
			{
				UGameplayStatics::PlaySoundAtLocation(World, MS, Location);
			}
			return;
		}
	}

	// Otherwise look up the authored cue (Category + optional surface).
	const FString Key = CueKey(Category, SurfaceType);
	if (const TObjectPtr<USoundBase>* Found = CueTable.Find(Key))
	{
		if (USoundBase* Sound = Found->Get())
		{
			if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
			{
				UGameplayStatics::PlaySoundAtLocation(World, Sound, Location);
			}
			return;
		}
	}

	// No cue bound — silent (already warned once at init).
	UE_LOG(LogTemp, VeryVerbose,
		TEXT("GTAI_SFXManager: no cue for category %d (surface '%s')."),
		static_cast<uint8>(Category), *SurfaceType.ToString());
}

void UGTAI_SFXManager::PlayUI(EGTAI_SFXCategory Category)
{
	// UI bus: non-spatial, never ducked.
	if (const TObjectPtr<USoundBase>* Found = CueTable.Find(CueKey(Category, FGameplayTag::EmptyTag)))
	{
		if (USoundBase* Sound = Found->Get())
		{
			if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
			{
				UGameplayStatics::PlaySound2D(World, Sound);
			}
			return;
		}
	}

	UE_LOG(LogTemp, VeryVerbose,
		TEXT("GTAI_SFXManager: no UI cue for category %d."), static_cast<uint8>(Category));
}

UAudioComponent* UGTAI_SFXManager::StartEngineNote(EGTAI_VehicleClass VehicleClass,
                                                    USceneComponent* AttachTo)
{
	if (!AttachTo)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GTAI_SFXManager::StartEngineNote: null AttachTo component."));
		return nullptr;
	}

	// Use the bound procedural engine MetaSound if present, else a bare
	// audio component the caller can feed a MetaSound into later.
	const TObjectPtr<UMetaSoundSource>* Proc =
		ProceduralLayers.Find(static_cast<uint8>(EGTAI_SFXCategory::VehicleEngine));
	UMetaSoundSource* EngineMS = Proc ? Proc->Get() : nullptr;

	UAudioComponent* EngineComp = nullptr;
	if (EngineMS)
	{
		EngineComp = UGameplayStatics::SpawnSoundAttached(EngineMS, AttachTo);
	}
	else
	{
		// No procedural layer: spawn an empty component so UpdateEngineNote's
		// parameter calls are safe no-ops until a MetaSound is assigned.
		EngineComp = NewObject<UAudioComponent>(AttachTo->GetOwner());
		EngineComp->RegisterComponent();
		EngineComp->AttachToComponent(AttachTo,
			FAttachmentTransformRules::KeepRelativeTransform);
	}

	if (EngineComp)
	{
		// Seed base class-specific scaling so the MetaSound graph can map it.
		EngineComp->SetFloatParameter(FName("VehicleClass"),
			static_cast<float>(static_cast<uint8>(VehicleClass)));
		EngineComp->SetFloatParameter(FName("BaseFreqScale"), VehicleBaseFreqScale(VehicleClass));
		EngineComp->SetFloatParameter(FName("RPM"), 800.f);
		EngineComp->SetFloatParameter(FName("Load"), 0.f);
		EngineComp->SetFloatParameter(FName("Throttle"), 0.f);
		EngineComp->SetFloatParameter(FName("Gear"), 1.f);
		EngineComp->Play();
	}
	return EngineComp;
}

void UGTAI_SFXManager::UpdateEngineNote(UAudioComponent* Engine, float RPM, float Load,
                                         float Throttle, int32 Gear)
{
	if (!Engine)
	{
		return;
	}

	Engine->SetFloatParameter(FName("RPM"), RPM);
	Engine->SetFloatParameter(FName("Load"), FMath::Clamp(Load, 0.f, 1.f));
	Engine->SetFloatParameter(FName("Throttle"), FMath::Clamp(Throttle, 0.f, 1.f));
	Engine->SetFloatParameter(FName("Gear"), static_cast<float>(Gear));
}

void UGTAI_SFXManager::RegisterProceduralLayer(EGTAI_SFXCategory Category,
                                               UMetaSoundSource* MetaSound)
{
	if (!MetaSound)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GTAI_SFXManager::RegisterProceduralLayer: null MetaSound for category %d."),
			static_cast<uint8>(Category));
		return;
	}
	ProceduralLayers.Add(static_cast<uint8>(Category), MetaSound);
	UE_LOG(LogTemp, Log,
		TEXT("GTAI_SFXManager: registered procedural layer for category %d."),
		static_cast<uint8>(Category));
}
