// GTAI_AudioTypes.h
// Shared enums and lightweight data types for the GTAI audio stack.
// Plain (non-reflected) types live in namespace GTAI::Audio. Reflected
// USTRUCT/UENUM types are declared at global scope (UHT forbids them in a
// namespace) and wrap these where needed.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace GTAI::Audio
{
	// Logical audio bus. Maps to a USubmixBase in the manager.
	enum class EGTAI_AudioBus : uint8
	{
		Master,
		Music,      // radio + dynamic music
		SFX,        // weapons / vehicles / impacts
		Voice,      // NPC dialogue + DJ + ads
		Ambient,    // city beds
		UI,         // menus / phone — never ducked
		Reverb      // spatialized send target
	};

	// Identifies a radio station. Order is irrelevant; lookups are by tag.
	enum class EGTAI_StationId : uint8
	{
		None,
		PulseFM,        // hip-hop
		NeonDrive,      // electronic / synthwave (procedural)
		TheForum,       // talk radio
		NYCNow,         // news
		Airbrands,      // commercials-only
		ClassicNY,      // jazz / soul
		Latido          // latin / reggaeton
	};

	// One entry in a station's broadcast timeline.
	struct FRadioSegment
	{
		EGTAI_StationId Station = EGTAI_StationId::None;
		FGameplayTag SegmentType;          // Song, DJBanter, Ad, NewsBreak, StationID...
		TSoftObjectPtr<class USoundBase> Audio; // null for pure-TTS/LLM segments
		float DurationSec = 0.f;
		FString LLMPromptPackKey;           // key into the DJ/ad generator when Audio is null
		bool bInterruptible = true;        // news takeovers can override
	};

	// DJ identity used by the LLM banter + TTS layers.
	struct FDiscJockeyProfile
	{
		FName Id;
		FString DisplayName;
		FString ElevenLabsVoiceId;
		TArray<FString> Catchphrases;
		TArray<FString> AttitudeTags;      // "cynical", "hype", "smooth"...
		float Seed = 0.f;
	};

	// A cached/generated voice line result.
	struct FVoiceLine
	{
		FString Text;
		FName SpeakerId;
		TSoftObjectPtr<class USoundWave> Audio;
		TSoftObjectPtr<class UAssetUserData> LipSyncTrack; // viseme track
		float Seed = 0.f;
		bool bDynamic = false;             // synthesized at runtime vs pre-baked
		double GeneratedAt = 0.0;
	};

	// Prompt context pack handed to the LLM for DJ/news/ad generation.
	struct FDLLPromptPack
	{
		FString PlayerName;
		TMap<FName, float> Reputation;     // faction -> standing
		TArray<FString> RecentActions;     // sanitized, non-identifying
		TArray<FString> WorldEvents;
		FName ShowIdentity;
		TArray<FString> StyleTags;
		int32 MaxTokens = 120;
		float Temperature = 0.8f;
	};

	// Generic generation request result (Suno / ElevenLabs agnostic).
	struct FGenResult
	{
		bool bSuccess = false;
		FString Error;
		FString RemoteId;                  // task/request id from the provider
		FString LocalPath;                 // imported asset path once downloaded
	};
}
