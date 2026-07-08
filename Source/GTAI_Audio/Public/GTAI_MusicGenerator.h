// GTAI_MusicGenerator.h
// Wrapper around the OFFLINE Suno pipeline. Suno is never called at runtime;
// this class imports pre-generated tracks (from Tools/AudioPipeline/suno_batch.py)
// into the radio station schedules and exposes them to the radio system. It also
// drives the MetaSound generative music beds (e.g. Neon Drive) at runtime.
//
// Suno API facts that shaped this (api.sunoapi.org, Bearer token):
//   - async generate + webhook callback; models V4..V5.5; vocal/instrumental
//   - extend / add_vocals / separate_vocals / convert_to_wav endpoints
//   - watermark-free commercial output; ~20s streaming latency (offline only)
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_AudioTypes.h"
#include "GTAI_MusicGenerator.generated.h"

class USoundBase;
class UMetaSoundSource;

// Spec for one Suno-generated track, authored in a StationDataAsset.
USTRUCT(BlueprintType)
struct GTAI_AUDIO_API FGTAI_MusicSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	FString GenrePrompt;       // Suno style prompt

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	int32 BPM = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	float DurationSec = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	bool bVocal = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	float Seed = 0.f;          // reproducible regeneration

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TSoftObjectPtr<USoundBase> ImportedTrack; // filled by import pipeline

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TSoftObjectPtr<USoundBase> StemsVocals;   // from separate_vocals

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TSoftObjectPtr<USoundBase> StemsInstrumental;
};

UCLASS()
class GTAI_AUDIO_API UGTAI_MusicGenerator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Drives a runtime MetaSound generative bed (e.g. Neon Drive). Intensity,
	// key and BPM are bound to gameplay via the MetaSound input parameters.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Music")
	void SetGenerativeIntensity(float Intensity);

	UFUNCTION(BlueprintCallable, Category = "GTAI|Music")
	void SetGenerativeKey(int32 ScaleRoot, int32 ScaleMode);

	UFUNCTION(BlueprintCallable, Category = "GTAI|Music")
	void StartGenerativeBed(UMetaSoundSource* Bed);

	UFUNCTION(BlueprintCallable, Category = "GTAI|Music")
	void StopGenerativeBed();

	// Returns an imported Suno track for a station by spec index (radio uses this).
	UFUNCTION(BlueprintCallable, Category = "GTAI|Music")
	USoundBase* GetStationTrack(EGTAI_StationId Station, int32 Index) const;

protected:
	UPROPERTY()
	TObjectPtr<UMetaSoundSource> ActiveBed;
};
