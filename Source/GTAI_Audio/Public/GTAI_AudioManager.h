// GTAI_AudioManager.h
// Top-level audio director. A UGameInstanceSubsystem that owns the submix
// graph, coordinates the radio / music / voice / ambient / sfx sub-systems,
// and drives global ducking from gameplay intensity.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Sound/SubmixEffects/AudioMixerSubmixEffectSettings.h"
#include "GTAI_AudioManager.generated.h"

class UGTAI_RadioSystem;
class UGTAI_MusicGenerator;
class UGTAI_VoiceSynthesis;
class UGTAI_AmbientAudioManager;
class UGTAI_SFXManager;
class USubmixBase;

UCLASS()
class GTAI_AUDIO_API UGTAI_AudioManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// Accessors ---------------------------------------------------------------
	UGTAI_RadioSystem*        GetRadioSystem()     const { return RadioSystem; }
	UGTAI_MusicGenerator*     GetMusicGenerator()  const { return MusicGenerator; }
	UGTAI_VoiceSynthesis*     GetVoiceSynthesis()  const { return VoiceSynthesis; }
	UGTAI_AmbientAudioManager*GetAmbientManager()  const { return AmbientManager; }
	UGTAI_SFXManager*         GetSFXManager()      const { return SFXManager; }

	USubmixBase* GetBusSubmix(GTAI::Audio::EGTAI_AudioBus Bus) const;

	// Mixing / ducking --------------------------------------------------------
	// Intensity 0..1 (calm..full combat). Drives a global ducking envelope:
	// music + ambient duck down, SFX + voice rise.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Audio")
	void SetIntensity(float Intensity);

	UFUNCTION(BlueprintCallable, Category = "GTAI|Audio")
	float GetIntensity() const { return CurrentIntensity; }

	// Push a transient mix event (e.g. explosion) that briefly overrides ducking.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Audio")
	void PulseDuck(GTAI::Audio::EGTAI_AudioBus Bus, float TargetGain, float Attack, float Release);

	// Master mute / volume (settings menu).
	UFUNCTION(BlueprintCallable, Category = "GTAI|Audio")
	void SetMasterVolume(float Volume);

	// Called by gameplay systems when the player commits a notable action so the
	// radio (ads/DJ/news) can react. Lightweight; queues a context update.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Audio")
	void ReportPlayerAction(const FString& ActionTag);

protected:
	UPROPERTY()
	TObjectPtr<UGTAI_RadioSystem> RadioSystem;

	UPROPERTY()
	TObjectPtr<UGTAI_MusicGenerator> MusicGenerator;

	UPROPERTY()
	TObjectPtr<UGTAI_VoiceSynthesis> VoiceSynthesis;

	UPROPERTY()
	TObjectPtr<UGTAI_AmbientAudioManager> AmbientManager;

	UPROPERTY()
	TObjectPtr<UGTAI_SFXManager> SFXManager;

	// Bus submixes, wired in Initialize() from DefaultEngine.ini references.
	UPROPERTY()
	TMap<uint8, TObjectPtr<USubmixBase>> BusSubmixes;

	float CurrentIntensity = 0.f;

	void ApplyDuckingForIntensity(float Intensity);
};
