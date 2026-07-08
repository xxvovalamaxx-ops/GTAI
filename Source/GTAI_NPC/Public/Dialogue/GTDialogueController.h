// GTDialogueController.h
// Owns an active conversation: assembles context (persona + memory + city-state
// + BSM constraints), routes through LLMManager, parses + applies the response.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "Dialogue/GTDialogueTypes.h"
#include "Dialogue/GTDialogueStateMachine.h"
#include "Dialogue/GTPersona.h"
#include "Memory/GTMemoryTypes.h"

class FLLMManager;

namespace GTAI::NPC
{
	// Callbacks for the async LLM result.
	DECLARE_DELEGATE_TwoParams(FOnDialogueLineReady, const FString& /*Line*/, ELLMTier /*Tier*/);
	DECLARE_DELEGATE_OneParam(FOnDialogueActions, const TArray<FDialogueAction>& /*Actions*/);

	class GTAI_NPC_API FDialogueController
	{
	public:
		FDialogueController(
			FNPCId InNPC,
			const FPersona& InPersona,
			const FDialogueGraph& InGraph,
			class FLLMManager& InLLM,
			class FMemoryStore& InMemory);

		// Start / reset a conversation.
		void Begin();

		// Player said something. Drives the BSM then (if needed) the LLM.
		void SubmitPlayerInput(const FString& PlayerInput, class IDialogueConditionEvaluator& Evaluator);

		// Bind to receive the spoken line + any world actions.
		FOnDialogueLineReady  OnLineReady;
		FOnDialogueActions    OnActions;

		// The BSM landed on an LLM node -> build prompt + request generation.
		void RequestLLMLine(const FDialogueNode& Node, const FString& PlayerInput);

	private:
		FString BuildPrompt(const FDialogueNode& Node, const FString& PlayerInput) const;
		void HandleLLMResult(const FDialogueLLMResponse& Resp, ELLMTier Tier);

		FNPCId NPC;
		FPersona Persona;
		FDialogueStateMachine SM;
		class FLLMManager* LLM = nullptr;
		class FMemoryStore* Memory = nullptr;

		TArray<FString> History;     // recent player/npc lines (for prompt)
		int32 MaxHistory = 10;
	};
}
