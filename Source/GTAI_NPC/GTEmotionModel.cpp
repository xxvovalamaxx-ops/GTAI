// Copyright GTAI. All Rights Reserved.
// LUMEN — NPC Emotion Model (affinity, trust, fear, respect, anger, mood)

#include "GTEmotionModel.h"
#include "GTMemoryTypes.h"

UGTEmotionModel::UGTEmotionModel()
{
    PrimaryComponentTick.bCanEverTick = true;
    InitializeDefaults();
}

void UGTEmotionModel::InitializeDefaults()
{
    Affinity = 0.f;   // Default neutral
    Trust = 0.f;
    Fear = 0.f;
    Respect = 0.f;
    Anger = 0.f;
    Mood = 0.f;       // -1 = unhappy, 0 = neutral, +1 = happy
}

void UGTEmotionModel::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Decay toward neutral (emotions naturally fade over time)
    float DecayRate = 0.1f * DeltaTime; // Full decay in ~10 seconds without reinforcement

    Affinity = FMath::FInterpTo(Affinity, 0.f, DeltaTime, DecayRate);
    Trust = FMath::FInterpTo(Trust, 0.f, DeltaTime, DecayRate);
    Fear = FMath::FInterpTo(Fear, 0.f, DeltaTime, DecayRate);
    Respect = FMath::FInterpTo(Respect, 0.f, DeltaTime, DecayRate);
    Anger = FMath::FInterpTo(Anger, 0.f, DeltaTime, DecayRate);
    Mood = FMath::FInterpTo(Mood, 0.f, DeltaTime, DecayRate);

    UpdateDialogueTone();
}

void UGTEmotionModel::ReactToEvent(ENPCEmotionEvent Event, float Intensity)
{
    switch (Event)
    {
    case ENPCEmotionEvent::PlayerHelped:
        Affinity += Intensity * 0.15f;
        Trust += Intensity * 0.10f;
        Respect += Intensity * 0.08f;
        Anger -= Intensity * 0.05f;
        Mood += Intensity * 0.12f;
        break;

    case ENPCEmotionEvent::PlayerHarmed:
        Affinity -= Intensity * 0.30f;
        Trust -= Intensity * 0.25f;
        Fear += Intensity * 0.20f;
        Anger += Intensity * 0.35f;
        Mood -= Intensity * 0.25f;
        break;

    case ENPCEmotionEvent::PlayerThreatened:
        Fear += Intensity * 0.40f;
        Trust -= Intensity * 0.15f;
        Anger += Intensity * 0.15f;
        Mood -= Intensity * 0.10f;
        break;

    case ENPCEmotionEvent::PlayerPaid:
        Affinity += Intensity * 0.10f;
        Trust += Intensity * 0.05f;
        Respect += Intensity * 0.12f;
        Mood += Intensity * 0.08f;
        break;

    case ENPCEmotionEvent::WitnessedCrime:
        Fear += Intensity * 0.50f;
        Trust -= Intensity * 0.20f;
        Mood -= Intensity * 0.30f;
        break;

    case ENPCEmotionEvent::WitnessedHeroism:
        Respect += Intensity * 0.25f;
        Trust += Intensity * 0.20f;
        Affinity += Intensity * 0.15f;
        Mood += Intensity * 0.15f;
        break;

    case ENPCEmotionEvent::CrowdPanic:
        Fear += Intensity * 0.60f;
        Anger = FMath::Max(Anger, Intensity * 0.10f);
        Mood -= Intensity * 0.35f;
        break;
    }

    // Clamp all values to [-1, 1]
    ClampAll();

    // Update tone for dialogue system
    UpdateDialogueTone();
}

void UGTEmotionModel::UpdateDialogueTone()
{
    if (Affinity > 0.3f && Trust > 0.2f)
        CurrentTone = ENPCDialogueTone::Friendly;
    else if (Fear > 0.4f)
        CurrentTone = ENPCDialogueTone::Fearful;
    else if (Anger > 0.4f)
        CurrentTone = ENPCDialogueTone::Aggressive;
    else if (Respect > 0.5f)
        CurrentTone = ENPCDialogueTone::Respectful;
    else if (Affinity < -0.3f)
        CurrentTone = ENPCDialogueTone::Hostile;
    else
        CurrentTone = ENPCDialogueTone::Neutral;
}

void UGTEmotionModel::ClampAll()
{
    Affinity = FMath::Clamp(Affinity, -1.f, 1.f);
    Trust = FMath::Clamp(Trust, -1.f, 1.f);
    Fear = FMath::Clamp(Fear, -1.f, 1.f);
    Respect = FMath::Clamp(Respect, -1.f, 1.f);
    Anger = FMath::Clamp(Anger, -1.f, 1.f);
    Mood = FMath::Clamp(Mood, -1.f, 1.f);
}
