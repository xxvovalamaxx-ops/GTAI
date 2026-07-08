// GTDialogueStateMachine.cpp
// Deterministic dialogue structure for quest flow.
// Authoring-time graph drives gating/branching; open-ended lines are delegated
// to the LLM by the controller that owns this machine.

#include "Dialogue/GTDialogueStateMachine.h"
#include "Dialogue/GTDialogueTypes.h"
#include "NPC/GTNPCDefines.h"

namespace GTAI::NPC
{
	// Shared sentinel so we never return a dangling reference.
	static const FDialogueNode GInvalidNode;

	const FDialogueNode& FDialogueStateMachine::ProcessInput(
		const FString& PlayerInput,
		IDialogueConditionEvaluator& Evaluator,
		FNPCId NPC)
	{
		const FDialogueNode* Node = Graph.Nodes.Find(CurrentNode);
		if (!Node)
		{
			// Current node id is not in the graph (corrupt graph / bad jump).
			return GInvalidNode;
		}

		switch (Node->Type)
		{
		case EDialogueNodeType::Branch:
			{
				// Ask the gameplay layer which transition condition matched.
				const FName Matched = Evaluator.Evaluate(Node->Transitions, NPC, PlayerInput);
				if (Matched != NAME_None)
				{
					if (const FName* Next = Node->Transitions.Find(Matched))
					{
						CurrentNode = *Next;
					}
				}
				else if (Node->DefaultNext != NAME_None)
				{
					CurrentNode = Node->DefaultNext;
				}
			}
			break;

		case EDialogueNodeType::LLM:
		case EDialogueNodeType::Bark:
		case EDialogueNodeType::Action:
			// These "speak / act" nodes advance to their DefaultNext when present
			// so the authored flow keeps moving without an explicit player branch.
			if (Node->DefaultNext != NAME_None)
			{
				CurrentNode = Node->DefaultNext;
			}
			break;

		case EDialogueNodeType::Exit:
		default:
			// Terminal — stay put until the conversation is torn down.
			break;
		}

		const FDialogueNode* Landed = Graph.Nodes.Find(CurrentNode);
		return Landed ? *Landed : GInvalidNode;
	}

	bool FDialogueStateMachine::IsAtExit() const
	{
		if (const FDialogueNode* Node = Graph.Nodes.Find(CurrentNode))
		{
			return Node->Type == EDialogueNodeType::Exit;
		}
		return false;
	}
}
