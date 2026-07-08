// Copyright GTAI. All Rights Reserved.
// LUMEN — Crowd Behavior (emotional contagion over MassEntity)

#include "GTCrowdBehavior.h"
#include "GTCrowdTypes.h"
#include "GTEmotionModel.h"
#include "Engine/World.h"

void AGTCrowdManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    PropagateEmotions(DeltaSeconds);
}

void AGTCrowdManager::PropagateEmotions(float DeltaSeconds)
{
    // Emotional contagion: nearby NPCs influence each other's fear/mood
    // Based on proximity-based propagation with decay
    for (int32 i = 0; i < ActiveNPCs.Num(); ++i)
    {
        for (int32 j = i + 1; j < ActiveNPCs.Num(); ++j)
        {
            float Distance = FVector::Dist(ActiveNPCs[i]->GetActorLocation(),
                                           ActiveNPCs[j]->GetActorLocation());
            if (Distance > ContagionRadius) continue;

            float Influence = 1.f - (Distance / ContagionRadius);

            // Fear contagion (panic spreads fast in crowds)
            if (ActiveNPCs[i]->EmotionModel->Fear > PanicThreshold &&
                ActiveNPCs[j]->EmotionModel->Fear < PanicThreshold)
            {
                ActiveNPCs[j]->EmotionModel->ReactToEvent(ENPCEmotionEvent::CrowdPanic,
                    Influence * ContagionRate * DeltaSeconds);
            }
        }
    }
}

void AGTCrowdManager::TriggerPanicEvent(const FVector& Epicenter, float Radius, float Intensity)
{
    for (AGTCrowdNPC* NPC : ActiveNPCs)
    {
        float Distance = FVector::Dist(NPC->GetActorLocation(), Epicenter);
        if (Distance < Radius)
        {
            float LocalIntensity = Intensity * (1.f - Distance / Radius);
            if (NPC->EmotionModel)
            {
                NPC->EmotionModel->ReactToEvent(ENPCEmotionEvent::CrowdPanic, LocalIntensity);
            }

            // Flee behavior — run away from epicenter
            FVector FleeDirection = (NPC->GetActorLocation() - Epicenter).GetSafeNormal();
            NPC->SetFleeDirection(FleeDirection);
        }
    }
}

void AGTCrowdNPC::SetFleeDirection(const FVector& Direction)
{
    FleeDirection = Direction;
    bIsFleeing = true;
    FleeTimer = FMath::FRandRange(3.f, 8.f); // Flee for 3-8 seconds
}

void AGTCrowdNPC::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bIsFleeing)
    {
        FleeTimer -= DeltaSeconds;
        if (FleeTimer <= 0.f)
        {
            bIsFleeing = false;
            FleeDirection = FVector::ZeroVector;
        }
    }

    // Curiosity — attracted to interesting events
    if (bIsCurious && !bIsFleeing)
    {
        FVector ToTarget = CuriosityTarget - GetActorLocation();
        if (ToTarget.Size() > 50.f)
        {
            AddMovementInput(ToTarget.GetSafeNormal(), 0.3f); // Walk slowly toward curiosity
        }
    }
}

void AGTCrowdNPC::SetCurious(const FVector& Target)
{
    bIsCurious = true;
    CuriosityTarget = Target;
    CuriosityTimer = FMath::FRandRange(5.f, 15.f); // Watch for 5-15 seconds
}
