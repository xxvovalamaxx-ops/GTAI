// Copyright GTAI. All Rights Reserved.
// LUMEN — NPC Orchestrator (per-NPC brain wiring dialogue/memory/emotion/schedule)

#include "GTNPCOrchestrator.h"
#include "GTNPCDefines.h"
#include "GTDialogueController.h"
#include "GTMemoryStore.h"
#include "GTEmotionModel.h"
#include "GTPedestrianSchedule.h"
#include "GTCrowdBehavior.h"
#include "GameFramework/Character.h"

void UGTNPCOrchestrator::Initialize(ACharacter* InOwner, const FNPCDefinition& Def)
{
    Owner = InOwner;
    Definition = Def;
    bIsPlayerFacing = false;

    // Create subsystems
    if (Definition.bHasDialogue)
    {
        DialogueController = NewObject<UGTDialogueController>(this);
        DialogueController->Initialize(Def.PersonaId, Def.Backstory);
    }

    if (Definition.bHasMemory)
    {
        MemoryStore = NewObject<UGTMemoryStore>(this);
        MemoryStore->Initialize(Def.NPCId);
    }

    if (Definition.bHasSchedule)
    {
        PedestrianSchedule = NewObject<UGTPedestrianSchedule>(this);
    }

    // Emotion model for all NPCs
    EmotionModel = NewObject<UGTEmotionModel>(this);
}

void UGTNPCOrchestrator::TickNPC(float DeltaSeconds)
{
    TickDialogue(DeltaSeconds);
    TickSchedule(DeltaSeconds);
    TickEmotion(DeltaSeconds);
}

void UGTNPCOrchestrator::OnPlayerApproach(float Distance)
{
    if (Distance < InteractionDistance && !bIsPlayerFacing)
    {
        ActivateDialogue();
    }
    else if (Distance > InteractionDistance * 1.5f && bIsPlayerFacing)
    {
        DeactivateDialogue();
    }
}

void UGTNPCOrchestrator::ActivateDialogue()
{
    bIsPlayerFacing = true;
    if (DialogueController && MemoryStore)
    {
        DialogueController->StartConversation(MemoryStore);
    }
}

void UGTNPCOrchestrator::DeactivateDialogue()
{
    bIsPlayerFacing = false;
    if (DialogueController)
    {
        DialogueController->EndConversation();
    }
}

FString UGTNPCOrchestrator::ProcessPlayerInput(const FString& PlayerText)
{
    if (!DialogueController || !bIsPlayerFacing) return TEXT("");

    // Route through dialogue controller with full context
    return DialogueController->GetResponse(PlayerText, MemoryStore, EmotionModel);
}

void UGTNPCOrchestrator::ReactToWorldEvent(const FGameplayTag& EventTag, float Intensity)
{
    if (EmotionModel)
    {
        // Map gameplay tag to emotion event
        if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("World.Crime"))))
            EmotionModel->ReactToEvent(ENPCEmotionEvent::WitnessedCrime, Intensity);
        else if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("World.Heroism"))))
            EmotionModel->ReactToEvent(ENPCEmotionEvent::WitnessedHeroism, Intensity);
        else if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("World.Panic"))))
            EmotionModel->ReactToEvent(ENPCEmotionEvent::CrowdPanic, Intensity);
    }
}

void UGTNPCOrchestrator::TickDialogue(float DeltaSeconds)
{
    if (DialogueController && bIsPlayerFacing)
    {
        DialogueController->Tick(DeltaSeconds);
    }
}

void UGTNPCOrchestrator::TickSchedule(float DeltaSeconds)
{
    if (PedestrianSchedule && !bIsPlayerFacing && Owner)
    {
        // Only process schedule when not in dialogue (avoids NPC wandering off mid-conversation)
        PedestrianSchedule->TickSchedules(DeltaSeconds, 5);
    }
}

void UGTNPCOrchestrator::TickEmotion(float DeltaSeconds)
{
    if (EmotionModel)
    {
        EmotionModel->TickComponent(DeltaSeconds, ELevelTick::LEVELTICK_All, nullptr);
    }
}
