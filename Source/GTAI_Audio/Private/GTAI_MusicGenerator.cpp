// Copyright GTAI. All Rights Reserved.
// SIREN — Music Generator (Suno API) + LipSync stubs

#include "GTAI_MusicGenerator.h"
void UGTAI_MusicGenerator::Initialize() {}
FString UGTAI_MusicGenerator::GenerateSong(const FString& Genre, const FString& Mood) {
    return FString::Printf(TEXT("/Game/Audio/Music/%s_%s_%d.wav"), *Genre, *Mood, FMath::Rand());
}
