// GTAI_AmbientAudioManager.h
// City ambient audio: layered beds (traffic, crowds, weather, wildlife,
// mechanical) that adapt to district, time-of-day, weather state, crowd density
// and player notoriety. Layers are USoundWave loops / MetaSound beds attached to
// Audio Volumes so they stream with World Partition cells. This manager owns the
// crossfade logic and exposes hooks called by GTAI_World (weather, TOD, density).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "GTAI_AudioTypes.h"
#include "GTAI_AmbientAudioManager.generated.h"

class UAudioComponent;
class USoundBase;

// One adaptive ambient layer's runtime state.
USTRUCT(BlueprintType)
struct GTAI_AUDIO_API FGTAI_AmbientLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	FGameplayTag LayerTag;        // "Ambient.Traffic", "Ambient.Crowd"...

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY()
	TObjectPtr<UAudioComponent> Component;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	float BaseGain = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
	float CurrentGain = 1.f;
};

UCLASS()
class GTAI_AUDIO_API UGTAI_AmbientAudioManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Set the active district (called by GTAI_World as the player crosses cells).
	UFUNCTION(BlueprintCallable, Category = "GTAI|Ambient")
	void SetDistrict(const FGameplayTag& DistrictTag);

	// Time-of-day 0..24; crossfades layer gains (loud midday -> quiet 3am).
	UFUNCTION(BlueprintCallable, Category = "GTAI|Ambient")
	void SetTimeOfDay(float Hour);

	// Weather state ('Weather.Clear', 'Weather.Rain', 'Weather.Storm'...).
	UFUNCTION(BlueprintCallable, Category = "GTAI|Ambient")
	void SetWeather(const FGameplayTag& WeatherTag);

	// Live pedestrian count -> scales crowd murmur gain.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Ambient")
	void SetCrowdDensity(float Normalized0To1);

	// Player notoriety 0..1 -> raises siren/alert ambient, ducks music (via manager).
	UFUNCTION(BlueprintCallable, Category = "GTAI|Ambient")
	void SetNotoriety(float Normalized0To1);

	// One-shot thunder / siren pass / crowd cheer.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Ambient")
	void TriggerOneShot(const FGameplayTag& EventTag);

	void TickAmbient(float DeltaTime);

protected:
	UPROPERTY()
	TArray<FGTAI_AmbientLayer> ActiveLayers;

	FGameplayTag CurrentDistrict;
	float CurrentTOD = 12.f;
	FGameplayTag CurrentWeather = FGameplayTag::RequestGameplayTag(TEXT("Weather.Clear"));
	float CrowdDensity = 0.5f;
	float Notoriety = 0.f;

	void RebuildLayersForDistrict(const FGameplayTag& DistrictTag);
	void ApplyLayerGains();
	float TimeOfDayGain(const FGameplayTag& LayerTag) const;
};
