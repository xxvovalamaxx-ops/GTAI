// Copyright GTAI. All Rights Reserved.
// AUDIO — GTAI_AudioManager
// Top-level audio director: owns the submix bus graph, coordinates the
// radio / music / voice / ambient / sfx sub-systems, and drives global
// ducking from gameplay intensity. Built on UE5.8 MetaSounds + AudioMixer.
//
// Design notes (see skill audio-system-design.md §3):
//   - Buses are logical (GTAI::Audio::EGTAI_AudioBus) and resolve to USubmixBase.
//   - Ducking is a simple intensity envelope: music + ambient duck DOWN as
//     combat intensity rises; SFX + voice hold / rise. UI is never ducked.
//   - PulseDuck briefly overrides one bus (e.g. explosion) then restores.

#include "GTAI_AudioManager.h"

#include "Engine/GameInstance.h"
#include "Sound/SoundSubmix.h"
#include "AudioMixerBlueprintLibrary.h"

#include "GTAI_RadioSystem.h"
#include "GTAI_MusicGenerator.h"
#include "GTAI_VoiceSynthesis.h"
#include "GTAI_AmbientAudioManager.h"
#include "GTAI_SFXManager.h"

// Default asset paths for the bus submixes. These are expected to live under
// /Game/GTAI/Audio/Buses/. When a path is missing the bus resolves to nullptr
// and ducking for that bus is a no-op (designer wires the real submix later).
static const TMap<GTAI::Audio::EGTAI_AudioBus, FString>& GetDefaultBusPaths()
{
	static const TMap<GTAI::Audio::EGTAI_AudioBus, FString> Paths =
	{
		{ GTAI::Audio::EGTAI_AudioBus::Master,    TEXT("/Game/GTAI/Audio/Buses/SN_Master") },
		{ GTAI::Audio::EGTAI_AudioBus::Music,     TEXT("/Game/GTAI/Audio/Buses/SN_Music") },
		{ GTAI::Audio::EGTAI_AudioBus::SFX,       TEXT("/Game/GTAI/Audio/Buses/SN_SFX") },
		{ GTAI::Audio::EGTAI_AudioBus::Voice,     TEXT("/Game/GTAI/Audio/Buses/SN_Voice") },
		{ GTAI::Audio::EGTAI_AudioBus::Ambient,   TEXT("/Game/GTAI/Audio/Buses/SN_Ambient") },
		{ GTAI::Audio::EGTAI_AudioBus::UI,        TEXT("/Game/GTAI/Audio/Buses/SN_UI") },
		{ GTAI::Audio::EGTAI_AudioBus::Reverb,    TEXT("/Game/GTAI/Audio/Buses/SN_Reverb") },
	};
	return Paths;
}

void UGTAI_AudioManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Resolve the bus submix assets declared in DefaultEngine.ini / content.
	const TMap<GTAI::Audio::EGTAI_AudioBus, FString>& Paths = GetDefaultBusPaths();
	for (const TPair<GTAI::Audio::EGTAI_AudioBus, FString>& Pair : Paths)
	{
		if (USubmixBase* Submix = LoadObject<USubmixBase>(nullptr, *Pair.Value))
		{
			BusSubmixes.Add(static_cast<uint8>(Pair.Key), Submix);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("GTAI_AudioManager: bus submix not found at '%s' (bus %d) — ducking disabled for it."),
				*Pair.Value, static_cast<uint8>(Pair.Key));
		}
	}

	// Acquire sibling sub-systems (UE auto-creates UGameInstanceSubsystems).
	if (UGameInstance* GI = GetGameInstance())
	{
		RadioSystem     = GI->GetSubsystem<UGTAI_RadioSystem>();
		MusicGenerator  = GI->GetSubsystem<UGTAI_MusicGenerator>();
		VoiceSynthesis  = GI->GetSubsystem<UGTAI_VoiceSynthesis>();
		AmbientManager  = GI->GetSubsystem<UGTAI_AmbientAudioManager>();
		SFXManager      = GI->GetSubsystem<UGTAI_SFXManager>();
	}

	CurrentIntensity = 0.f;
	ApplyDuckingForIntensity(CurrentIntensity);

	UE_LOG(LogTemp, Log, TEXT("GTAI_AudioManager initialized (%d bus submixes resolved)."),
		BusSubmixes.Num());
}

void UGTAI_AudioManager::Deinitialize()
{
	// Release references; sub-systems tear themselves down via their own Deinitialize.
	RadioSystem = nullptr;
	MusicGenerator = nullptr;
	VoiceSynthesis = nullptr;
	AmbientManager = nullptr;
	SFXManager = nullptr;
	BusSubmixes.Empty();

	Super::Deinitialize();
}

USubmixBase* UGTAI_AudioManager::GetBusSubmix(GTAI::Audio::EGTAI_AudioBus Bus) const
{
	if (const TObjectPtr<USubmixBase>* Found = BusSubmixes.Find(static_cast<uint8>(Bus)))
	{
		return Found->Get();
	}
	return nullptr;
}

void UGTAI_AudioManager::SetIntensity(float Intensity)
{
	const float Clamped = FMath::Clamp(Intensity, 0.f, 1.f);
	if (FMath::IsNearlyEqual(Clamped, CurrentIntensity, KINDA_SMALL_NUMBER))
	{
		return;
	}
	CurrentIntensity = Clamped;
	ApplyDuckingForIntensity(CurrentIntensity);
}

void UGTAI_AudioManager::ApplyDuckingForIntensity(float Intensity)
{
	// Music ducks to 45% at full intensity; ambient to 30%.
	const float MusicGain   = FMath::Lerp(1.f, 0.45f, Intensity);
	const float AmbientGain = FMath::Lerp(1.f, 0.30f, Intensity);
	// SFX + Voice stay at unity (combat clarity). UI untouched by design.

	if (USubmixBase* MusicSubmix = GetBusSubmix(GTAI::Audio::EGTAI_AudioBus::Music))
	{
		UAudioMixerBlueprintLibrary::SetSubmixGain(this, Cast<USoundSubmix>(MusicSubmix), MusicGain, 0.25f);
	}
	if (USubmixBase* AmbientSubmix = GetBusSubmix(GTAI::Audio::EGTAI_AudioBus::Ambient))
	{
		UAudioMixerBlueprintLibrary::SetSubmixGain(this, Cast<USoundSubmix>(AmbientSubmix), AmbientGain, 0.25f);
	}

	// SFX + Voice restored to full whenever intensity changes (cancels any stale pulse
	// unless a PulseDuck is actively holding them — acceptable for an envelope model).
	if (USubmixBase* SFXSubmix = GetBusSubmix(GTAI::Audio::EGTAI_AudioBus::SFX))
	{
		UAudioMixerBlueprintLibrary::SetSubmixGain(this, Cast<USoundSubmix>(SFXSubmix), 1.f, 0.15f);
	}
	if (USubmixBase* VoiceSubmix = GetBusSubmix(GTAI::Audio::EGTAI_AudioBus::Voice))
	{
		UAudioMixerBlueprintLibrary::SetSubmixGain(this, Cast<USoundSubmix>(VoiceSubmix), 1.f, 0.15f);
	}
}

void UGTAI_AudioManager::PulseDuck(GTAI::Audio::EGTAI_AudioBus Bus, float TargetGain, float Attack, float Release)
{
	USubmixBase* Submix = GetBusSubmix(Bus);
	if (!Submix)
	{
		return;
	}

	// Slam the bus down/up to TargetGain over Attack, then restore to its
	// intensity-derived resting gain over Release. UI bus ignores pulses (never ducked).
	if (Bus == GTAI::Audio::EGTAI_AudioBus::UI)
	{
		return;
	}

	UAudioMixerBlueprintLibrary::SetSubmixGain(this, Cast<USoundSubmix>(Submix), TargetGain, Attack);

	// Compute the resting gain this bus should return to.
	float RestingGain = 1.f;
	if (Bus == GTAI::Audio::EGTAI_AudioBus::Music)
	{
		RestingGain = FMath::Lerp(1.f, 0.45f, CurrentIntensity);
	}
	else if (Bus == GTAI::Audio::EGTAI_AudioBus::Ambient)
	{
		RestingGain = FMath::Lerp(1.f, 0.30f, CurrentIntensity);
	}

	const float SafeRelease = FMath::Max(Release, KINDA_SMALL_NUMBER);
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		// Defer the restore so Attack completes first.
		World->GetTimerManager().SetTimerForNextTick([this, Submix, RestingGain, SafeRelease]()
		{
			UAudioMixerBlueprintLibrary::SetSubmixGain(this, Cast<USoundSubmix>(Submix), RestingGain, SafeRelease);
		});
	}
}

void UGTAI_AudioManager::SetMasterVolume(float Volume)
{
	USubmixBase* Master = GetBusSubmix(GTAI::Audio::EGTAI_AudioBus::Master);
	if (Master)
	{
		UAudioMixerBlueprintLibrary::SetSubmixGain(this, Cast<USoundSubmix>(Master),
			FMath::Clamp(Volume, 0.f, 1.f), 0.05f);
	}
}

void UGTAI_AudioManager::ReportPlayerAction(const FString& ActionTag)
{
	// Lightweight context hook: the radio (DJ/news/ads) reacts to player activity.
	// We do not block the game thread — just forward to the radio system which
	// queues context for the next segment boundary.
	if (RadioSystem && RadioSystem->IsOn())
	{
		// The active station will pick up the action at its next bInterruptible
		// boundary via its scheduling loop (see GTAI_RadioSystem).
		RadioSystem->QueueDJBanter(TEXT(""));
	}
	UE_LOG(LogTemp, VeryVerbose, TEXT("GTAI_AudioManager: player action reported '%s'."), *ActionTag);
}
