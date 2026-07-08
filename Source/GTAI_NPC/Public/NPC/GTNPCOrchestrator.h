// GTNPCOrchestrator.h
// Per-NPC "brain" that wires together the dialogue controller, memory, emotion,
// and schedule/crowd state. One instance per attended NPC; pedestrians use only
// the lightweight fragments (no orchestrator) for performance.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "Dialogue/GTDialogueController.h"
#include "Memory/GTMemoryStore.h"
#include "Emotion/GTEmotionModel.h"
#include "Dialogue/GTPersona.h"

namespace GTAI::NPC
{
	class GTAI_NPC_API FGTNPCOrchestrator
	{
	public:
		FGTNPCOrchestrator(FNPCId InNPC,
		                   const FPersona& InPersona,
		                   const FDialogueGraph& InGraph,
		                   class FLLMManager& InLLM);

		// Player initiates/continues conversation.
		void OnPlayerSpeak(const FString& Input, class IDialogueConditionEvaluator& Evaluator);

		// Affective event from gameplay (combat near NPC, favor completed, etc.).
		void OnEmotionEvent(const FEmotionEvent& Ev) { Emotion.ApplyEvent(Ev); }

		// Periodic upkeep.
		void Tick(float CityHoursDelta, float RealDelta);

		FMemoryStore& GetMemory() { return Memory; }
		FEmotionModel& GetEmotion() { return Emotion; }
		const FPersona& GetPersona() const { return Persona; }

	private:
		FNPCId NPC;
		FPersona Persona;
		FMemoryStore Memory;
		FEmotionModel Emotion;
		FDialogueController Dialogue;
	};
}
