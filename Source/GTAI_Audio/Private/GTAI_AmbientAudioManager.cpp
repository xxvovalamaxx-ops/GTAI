// Copyright GTAI. All Rights Reserved.
// SIREN — Ambient Audio Manager (city soundscape)

#include "GTAI_AmbientAudioManager.h"
#include "GTAI_AudioTypes.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"

void UGTAI_AmbientAudioManager::Initialize(UGTAI_AudioManager* InAudioManager)
{
    AudioManager = InAudioManager;
    CurrentTimeOfDay = 0.5f;
}

void UGTAI_AmbientAudioManager::TickAmbient(float DeltaSeconds)
{
    UpdateTimeOfDay(DeltaSeconds);
    UpdateWeatherSounds(DeltaSeconds);
    UpdateTrafficDensity(DeltaSeconds);
    UpdateDistrictAudio(DeltaSeconds);
}

void UGTAI_AmbientAudioManager::SetDistrict(const FString& District)
{
    CurrentDistrict = District;
    UpdateDistrictAudio(0.f);
}

void UGTAI_AmbientAudioManager::SetWeather(const FString& WeatherType)
{
    CurrentWeather = WeatherType;
    CrossfadeAmbientLayer(CurrentWeatherLayer, WeatherType);
}

void UGTAI_AmbientAudioManager::UpdateTimeOfDay(float DeltaSeconds)
{
    float Hour = CurrentTimeOfDay * 24.f;
    float TargetDensity = 0.f;

    if (Hour >= 7.f && Hour <= 9.f)       TargetDensity = 1.0f;   // Morning rush
    else if (Hour >= 9.f && Hour <= 17.f)  TargetDensity = 0.7f;   // Daytime
    else if (Hour >= 17.f && Hour <= 19.f) TargetDensity = 1.0f;   // Evening rush
    else if (Hour >= 19.f && Hour <= 22.f) TargetDensity = 0.5f;   // Evening
    else                                  TargetDensity = 0.2f;   // Night

    CityDensity = FMath::FInterpTo(CityDensity, TargetDensity, DeltaSeconds, 0.5f);
}

void UGTAI_AmbientAudioManager::UpdateWeatherSounds(float DeltaSeconds) { /* TODO */ }
void UGTAI_AmbientAudioManager::UpdateTrafficDensity(float DeltaSeconds) { /* TODO */ }
void UGTAI_AmbientAudioManager::UpdateDistrictAudio(float DeltaSeconds) { /* TODO */ }

void UGTAI_AmbientAudioManager::CrossfadeAmbientLayer(const FString& From, const FString& To)
{
    CurrentWeatherLayer = To;
}
