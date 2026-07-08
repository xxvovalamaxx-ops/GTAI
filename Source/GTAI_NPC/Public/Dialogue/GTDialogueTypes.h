// GTDialogueTypes.h
// Data structures for the hybrid dialogue system (state-machine + LLM).
// NOTE: Core logic types are plain C++ structs inside namespace GTAI::NPC.
// UHT does not permit USTRUCT/UPROPERTY inside a namespace, so reflected
// DataAssets (designer-authored) are declared at global scope in
// Dialogue/GTDialogueAssets.h and wrap these plain types.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"

namespace GTAI::NPC
{
	// A single action the NPC wants to take in the world as a result of dialogue.
	// Produced by the LLM (Tier 1/3) or authored on a node.
	struct FDialogueAction
	{
		FName Verb;       // e.g. "GiveItem", "SetQuestFlag", "Flee", "Emote", "ShareRumor"
		FString Target;   // item id, quest id, location tag, etc.
		int32 Amount = 0;
	};

	// Structured LLM response. The LLM is asked for JSON conforming to this.
	struct FDialogueLLMResponse
	{
		FString Line;                       // spoken/displayed text
		TArray<FDialogueAction> Actions;    // world-changing intents
		FName NextNodeHint;                 // optional BSM jump hint
		float Confidence = 1.0f;            // 0..1, used for fallback
	};

	enum class EDialogueNodeType : uint8
	{
		Branch,   // deterministic, gated by game state
		LLM,      // generate via LLMManager (open-ended)
		Bark,     // fixed line, no LLM
		Action,   // perform world action, no line
		Exit      // end conversation
	};

	// One node in the authored dialogue graph.
	struct FDialogueNode
	{
		FName Id;
		EDialogueNodeType Type = EDialogueNodeType::Bark;
		FString FixedLine;                  // for Bark nodes
		FString LLMConstraints;             // for LLM nodes: injected prompt constraints
		TMap<FName, FName> Transitions;     // Branch: condition -> next node id
		FName DefaultNext;
	};

	// A whole authored conversation graph.
	struct FDialogueGraph
	{
		FName RootNode = TEXT("Start");
		TMap<FName, FDialogueNode> Nodes;
	};
}
