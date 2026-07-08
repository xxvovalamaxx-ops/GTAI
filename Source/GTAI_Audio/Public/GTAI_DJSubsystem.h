// GTAI_DJSubsystem.h
// Generates DJ banter / talk-show monologues referencing the player and world
// state. Uses the shared GTAI_NPC LLM manager (DeepSeek V4 / GPT-5 per DESIGN.md)
// for generation and ElevenLabs (via UGTAI_VoiceSynthesis) for voice. Always
// falls back to pre-authored lines so the radio is never silent offline.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_AudioTypes.h"
#include "GTAI_DJSubsystem.generated.h"

class UGTAI_VoiceSynthesis;

UCLASS()
class GTAI_AUDIO_API UGTAI_DJSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Generate banter for a station. Calls the LLM asynchronously; on success the
	// line is synthesized (TTS) and the result delegate fires with audio.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio|DJ")
	void RequestBanter(EGTAI_StationId Station, const GTAI::Audio::FDLLPromptPack& Pack,
	                   FName DJProfileId = NAME_None);

	// Pure-LLM text generation (no TTS) — used by The Forum monologues.
	void RequestMonologue(EGTAI_StationId Station, const GTAI::Audio::FDLLPromptPack& Pack,
	                      TFunction<void(const FString&)> OnComplete);

	// Register a DJ personality (loaded from StationDataAsset at startup).
	UFUNCTION(BlueprintCallable, Category = "GTAI|Radio|DJ")
	void RegisterDJ(const GTAI::Audio::FDiscJockeyProfile& Profile);

	// Returns a pre-authored fallback line for a station (offline-safe).
	FString GetFallbackLine(EGTAI_StationId Station) const;

	DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnBanterReady, EGTAI_StationId, Station, class USoundBase*, Audio);

protected:
	UPROPERTY()
	TObjectPtr<UGTAI_VoiceSynthesis> VoiceSynthesis;

	TMap<FName, GTAI::Audio::FDiscJockeyProfile> DJProfiles;

	// Builds the LLM prompt from a station's DJ profile + the player/world pack.
	FString BuildPrompt(EGTAI_StationId Station,
	                    const GTAI::Audio::FDiscJockeyProfile& DJ,
	                    const GTAI::Audio::FDLLPromptPack& Pack) const;

	// Sanitizes LLM output (satire OK, hate/illegal filtered) before TTS.
	static bool PassesContentFilter(const FString& Text);
};
