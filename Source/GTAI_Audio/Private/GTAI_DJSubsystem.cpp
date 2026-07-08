// Copyright GTAI. All Rights Reserved.
// SIREN — DJ Subsystem (LLM-generated DJ banter + news)

#include "GTAI_DJSubsystem.h"
#include "GTAI_AudioTypes.h"
#include "GTAI_RadioSystem.h"
#include "HttpModule.h"

void UGTAI_DJSubsystem::Initialize(UGTAI_RadioSystem* InRadio) { Radio = InRadio; }

FString UGTAI_DJSubsystem::GenerateDJBanter(const FString& StationGenre, const TArray<FString>& RecentPlayerActions)
{
    // Assemble prompt with recent player actions for dynamic DJ references
    FString Actions = FString::Join(RecentPlayerActions, TEXT(", "));

    FString Prompt = FString::Printf(
        TEXT("You are a %s radio DJ in an open-world game. "
             "Mention recent player actions naturally: %s. "
             "20-30 words, casual street tone."),
        *StationGenre, *Actions);

    // TODO: Call LLM API (DeepSeek V4) for DJ banter generation
    // For offline mode, return canned response
    return TEXT("You're tuned to the sounds of the city. Stay safe out there.");
}

FString UGTAI_DJSubsystem::GenerateNewsSegment(const TMap<FString, FString>& WorldEvents)
{
    // Convert world events to news headlines
    FString News = TEXT("Breaking news from around the city. ");
    for (const auto& Event : WorldEvents)
    {
        News += FString::Printf(TEXT("%s: %s. "), *Event.Key, *Event.Value);
    }
    return News;
}

void UGTAI_DJSubsystem::QueueBanter(const FString& Genre) { PendingBanter.Add(GenerateDJBanter(Genre, {})); }
bool UGTAI_DJSubsystem::GetNextBanter(FString& OutBanter)
{
    if (PendingBanter.Num() == 0) return false;
    OutBanter = PendingBanter[0];
    PendingBanter.RemoveAt(0);
    return true;
}
