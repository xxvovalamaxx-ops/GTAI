// GTAI_VoiceSynthesis.h
// ElevenLabs TTS integration with two paths (per project constraint):
//   Path A - Pre-generated (batch, offline build step): common NPC lines,
//            greetings, barks -> baked USoundWave + viseme lip-sync track.
//   Path B - Runtime (dynamic): unique lines from the NPC dialogue LLM are
//            synthesized on demand via the STREAMING endpoint (Flash v2.5,
//            ~75ms first chunk) and lip-synced in parallel from known text.
//
// ElevenLabs facts (elevenlabs.io/docs):
//   POST /v1/text-to-speech/{voice_id}  (or /stream for streaming)
//   Models: eleven_v3 (expressive, 5k char), eleven_multilingual_v2 (29 langs),
//           eleven_flash_v2_5 (~75ms, 40k char, cheap)
//   Output mp3/pcm/opus; seed param for determinism; Voice Design + cloning.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_AudioTypes.h"
#include "GTAI_VoiceSynthesis.generated.h"

class USoundWave;
class UAssetUserData;

UCLASS()
class GTAI_AUDIO_API UGTAI_VoiceSynthesis : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Path A: load a pre-baked line from the cache/asset registry.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Voice")
	USoundWave* GetCachedLine(const FName& SpeakerId, const FString& Text, float Seed = 0.f);

	// Path B: synthesize a dynamic line. OnCacheMiss + bDynamic streams TTS and
	// fires OnLineReady with audio + a viseme lip-sync track. Subtitles (text)
	// are returned immediately so UI can show them before audio arrives.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Voice")
	void Speak(const FName& SpeakerId, const FString& Text, bool bDynamic,
	           FName VoiceId = NAME_None, float Seed = 0.f);

	// Streaming variant: begins playback as soon as the first chunk arrives.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Voice")
	void SpeakStreaming(const FName& SpeakerId, const FString& Text, FName VoiceId,
	                    TFunction<void(USoundWave*, UAssetUserData*)> OnLineReady);

	// Lip sync: generate/attach a viseme track for a line. For dynamic lines the
	// phoneme model runs from the known text in parallel with TTS.
	UFUNCTION(BlueprintCallable, Category = "GTAI|Voice")
	UAssetUserData* GenerateLipSync(USoundWave* Audio, const FString& Text);

	// Voice roster: NPC archetype -> ElevenLabs voice id (clone or library).
	UFUNCTION(BlueprintCallable, Category = "GTAI|Voice")
	void RegisterVoice(FName SpeakerId, FName ElevenLabsVoiceId, bool bCloned = false, float Seed = 0.f);

	DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnLineReady, USoundWave*, Audio, UAssetUserData*, LipSync);

protected:
	// LRU cache keyed by hash(Speaker + text + voiceSettings + seed).
	TMap<FString, GTAI::Audio::FVoiceLine> LineCache;
	uint32 CacheCapacity = 512;

	FName ResolveVoiceId(FName SpeakerId, FName Override) const;
	FString CacheKey(const FName& SpeakerId, const FString& Text, float Seed) const;
};
