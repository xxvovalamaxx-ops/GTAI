// GTDialogueStateMachine.h
// Authoring-time + runtime dialogue state machine. Provides deterministic
// structure (quest flow, gating) while delegating open-ended lines to the LLM.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "Dialogue/GTDialogueTypes.h"

class FLLMManager; // forward

namespace GTAI::NPC
{
	// Evaluates Branch/transition conditions against live game state + memory.
	// Implemented by the gameplay layer; the SM only calls it.
	class IDialogueConditionEvaluator
	{
	public:
		virtual ~IDialogueConditionEvaluator() = default;
		// Returns the transition key that matched, or NAME_None if none.
		virtual FName Evaluate(const TMap<FName, FName>& Conditions, FNPCId NPC, const FString& PlayerInput) = 0;
	};

	class GTAI_NPC_API FDialogueStateMachine
	{
	public:
		explicit FDialogueStateMachine(const FDialogueGraph& InGraph)
			: Graph(InGraph), CurrentNode(InGraph.RootNode) {}

		// Player spoke; advance the machine. Returns the node we landed on.
		const FDialogueNode& ProcessInput(
			const FString& PlayerInput,
			IDialogueConditionEvaluator& Evaluator,
			FNPCId NPC);

		void Reset() { CurrentNode = Graph.RootNode; }
		FName GetCurrentNode() const { return CurrentNode; }
		bool IsAtExit() const;

	private:
		const FDialogueGraph& Graph;
		FName CurrentNode;
	};
}
