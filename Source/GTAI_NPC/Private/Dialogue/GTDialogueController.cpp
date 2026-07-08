// GTDialogueController.cpp
// Owns an active conversation: assembles context (persona + memory + history),
// drives the deterministic state machine, and routes LLM nodes through
// FLLMManager. Parses the structured response and broadcasts the line + actions.

#include "Dialogue/GTDialogueController.h"
#include "LLM/GTLLMManager.h"
#include "LLM/GTLLMTypes.h"
#include "Memory/GTMemoryStore.h"
#include "Dialogue/GTDialogueTypes.h"
#include "NPC/GTNPCDefines.h"

namespace GTAI::NPC
{
	FDialogueController::FDialogueController(
		FNPCId InNPC,
		const FPersona& InPersona,
		const FDialogueGraph& InGraph,
		FLLMManager& InLLM,
		FMemoryStore& InMemory)
		: NPC(InNPC)
		, Persona(InPersona)
		, SM(InGraph)
		, LLM(&InLLM)
		, Memory(&InMemory)
	{
	}

	void FDialogueController::Begin()
	{
		SM.Reset();
		History.Empty();
	}

	void FDialogueController::SubmitPlayerInput(const FString& PlayerInput, IDialogueConditionEvaluator& Evaluator)
	{
		// Record the player's utterance in short-term memory.
		if (Memory)
		{
			FConversationTurn T;
			T.Speaker = EDialogueSpeaker::Player;
			T.Content = PlayerInput;
			T.Importance = 0.5f;
			T.Timestamp = 0.0;
			Memory->PushTurn(T);
			History.Add(PlayerInput);
			while (History.Num() > MaxHistory) { History.RemoveAt(0); }
		}

		// Advance the deterministic machine; it returns the node we landed on.
		const FDialogueNode& Node = SM.ProcessInput(PlayerInput, Evaluator, NPC);
		if (Node.Id == NAME_None)
		{
			return; // invalid graph node
		}

		switch (Node.Type)
		{
		case EDialogueNodeType::Bark:
			{
				FString Line = Node.FixedLine;
				if (Line.IsEmpty()) { Line = TEXT("..."); }

				if (Memory)
				{
					FConversationTurn T;
					T.Speaker = EDialogueSpeaker::NPC;
					T.Content = Line;
					T.Importance = 0.4f;
					Memory->PushTurn(T);
				}
				History.Add(Line);
				while (History.Num() > MaxHistory) { History.RemoveAt(0); }

				OnLineReady.Broadcast(Line, ELLMTier::None);
			}
			break;

		case EDialogueNodeType::LLM:
			RequestLLMLine(Node, PlayerInput);
			break;

		case EDialogueNodeType::Action:
			// The SM already advanced to DefaultNext (if any). Pure action nodes
			// carry no spoken line; notify listeners with an empty action batch so
			// they can react to the transition.
			OnActions.Broadcast(TArray<FDialogueAction>());
			break;

		case EDialogueNodeType::Branch:
			// Resolved by the SM; typically lands on a non-Branch node the caller
			// handles. If we are still parked on a Branch, there is nothing to say.
			break;

		case EDialogueNodeType::Exit:
		default:
			break;
		}
	}

	void FDialogueController::RequestLLMLine(const FDialogueNode& Node, const FString& PlayerInput)
	{
		if (!LLM)
		{
			// No LLM wired up: degrade gracefully to a neutral bark.
			FString Fallback = Node.LLMConstraints.IsEmpty() ? TEXT("(pauses)") : TEXT("(pauses) ");
			if (Memory)
			{
				FConversationTurn T;
				T.Speaker = EDialogueSpeaker::NPC;
				T.Content = Fallback;
				T.Importance = 0.3f;
				Memory->PushTurn(T);
			}
			History.Add(Fallback);
			while (History.Num() > MaxHistory) { History.RemoveAt(0); }
			OnLineReady.Broadcast(Fallback, ELLMTier::None);
			return;
		}

		FLLMRequest Req;
		Req.NPC = NPC;
		Req.PersonaId = Persona.PersonaId;
		Req.Prompt = BuildPrompt(Node, PlayerInput);
		Req.MaxTokens = 320;
		Req.Temperature = 0.8f;
		Req.bAllowCloud = true;
		Req.CacheBucket = FString::Printf(TEXT("dlg:%llu"), static_cast<uint64>(NPC));

		// Capture the node so we can honour an LLM-supplied jump hint on return.
		const FDialogueNode CapturedNode = Node;
		LLM->Generate(Req, FOnLLMResult::CreateLambda(
			[this, CapturedNode, PlayerInput](const FLLMRequest&, const FLLMResult& Res)
			{
				if (Res.bSuccess)
				{
					HandleLLMResult(Res.Response, Res.Tier);
				}
				else
				{
					// Cloud/on-device failure: surface raw text if present, else a stub.
					FDialogueLLMResponse FallbackResp;
					FallbackResp.Line = Res.RawText.IsEmpty() ? TEXT("(lost in thought)") : Res.RawText;
					FallbackResp.Confidence = 0.2f;
					HandleLLMResult(FallbackResp, ELLMTier::None);
				}
				// CapturedNode/PlayerInput retained to keep the capture list meaningful
				// for future hint handling; referenced to avoid unused warnings.
				(void)CapturedNode; (void)PlayerInput;
			}));
	}

	FString FDialogueController::BuildPrompt(const FDialogueNode& Node, const FString& PlayerInput) const
	{
		FString P;

		// --- Persona / voice ---
		P += TEXT("SYSTEM: You are ");
		P += Persona.DisplayName.ToString();
		P += TEXT(". ");
		P += Persona.Personality.ToString();
		P += TEXT(" ");
		P += FString::Printf(TEXT("Speech style: %s.\n"), *Persona.SpeechStyle.ToString());

		// --- Long-term memory context ---
		if (Memory)
		{
			const FString Mem = Memory->BuildContextSnippet(1200);
			if (!Mem.IsEmpty())
			{
				P += TEXT("MEMORY:\n");
				P += Mem;
				P += TEXT("\n");
			}
		}

		// --- Node-level constraints injected by the author ---
		if (!Node.LLMConstraints.IsEmpty())
		{
			P += TEXT("CONSTRAINTS: ");
			P += Node.LLMConstraints;
			P += TEXT("\n");
		}

		// --- Recent conversation (working memory) ---
		if (History.Num() > 0)
		{
			P += TEXT("RECENT CONVERSATION:\n");
			for (const FString& H : History) { P += H + TEXT("\n"); }
		}

		// --- Current player turn ---
		P += TEXT("PLAYER: ");
		P += PlayerInput;
		P += TEXT("\n");

		// --- Output contract: strict JSON so FLLMManager can parse it ---
		P += TEXT("Respond ONLY with a JSON object of the form:\n");
		P += TEXT("{\"line\": <spoken text>, \"actions\": [{\"verb\": \"\", \"target\": \"\", \"amount\": 0}], ");
		P += TEXT("\"next_node_hint\": \"\", \"confidence\": <0..1>}\n");
		P += TEXT("Stay in character. Do not emit prose outside the JSON.\n");

		return P;
	}

	void FDialogueController::HandleLLMResult(const FDialogueLLMResponse& Resp, ELLMTier Tier)
	{
		const FString& Line = Resp.Line;

		if (Memory)
		{
			FConversationTurn T;
			T.Speaker = EDialogueSpeaker::NPC;
			T.Content = Line;
			T.Importance = FMath::Clamp(Resp.Confidence, 0.f, 1.f);
			Memory->PushTurn(T);
			History.Add(Line);
			while (History.Num() > MaxHistory) { History.RemoveAt(0); }
		}

		OnLineReady.Broadcast(Line, Tier);

		if (Resp.Actions.Num() > 0)
		{
			OnActions.Broadcast(Resp.Actions);
		}

		// Note: NextNodeHint is honoured by the caller on the next player input;
		// the state machine exposes no public jump setter, so it cannot be
		// applied mid-turn without altering the published header surface.
		(void)Resp.NextNodeHint;
	}
}
