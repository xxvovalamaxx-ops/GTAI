// Copyright GTAI. All Rights Reserved.
// SIREN — Voice Synthesis (ElevenLabs API integration)

#include "GTAI_VoiceSynthesis.h"
#include "GTAI_AudioTypes.h"
#include "HttpModule.h"

void UGTAI_VoiceSynthesis::Initialize() {}

FString UGTAI_VoiceSynthesis::SynthesizeVoice(const FString& Text, const FString& VoiceId)
{
    // ElevenLabs API call for voice synthesis
    // TODO: HTTP POST to api.elevenlabs.io/v1/text-to-speech/{voice-id}
    // Returns audio file path

    return FString::Printf(TEXT("/Game/Audio/Voice/%s_%d.wav"), *VoiceId, FMath::Rand());
}

void UGTAI_VoiceSynthesis::BatchSynthesize(const TArray<FNPCDialogueLine>& Lines, const FString& VoiceId)
{
    for (const auto& Line : Lines)
    {
        SynthesizeVoice(Line.Text, VoiceId);
    }
}

void UGTAI_VoiceSynthesis::PreloadCommonLines(const FString& VoiceId)
{
    // Pre-generate common NPC barks during loading screen
    TArray<FString> CommonLines = {
        TEXT("Hey, watch where you're going!"),
        TEXT("Have a nice day."),
        TEXT("What's up?"),
        TEXT("Get out of here!"),
        TEXT("I'm calling the cops!")
    };

    for (const auto& Line : CommonLines)
    {
        SynthesizeVoice(Line, VoiceId);
    }
}
