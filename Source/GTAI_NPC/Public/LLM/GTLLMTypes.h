// GTLLMTypes.h
// Types for the three-tier LLM manager (on-device / cache / cloud).
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "Dialogue/GTDialogueTypes.h"

class FLLMManager; // forward

namespace GTAI::NPC
{
	// A request handed to the LLM manager.
	struct FLLMRequest
	{
		FNPCId NPC = 0;
		FName PersonaId;             // advisor persona or NPC role
		FString Prompt;              // fully assembled prompt
		int32 MaxTokens = 512;
		float Temperature = 0.7f;
		bool bAllowCloud = true;     // if false, restrict to Tier1/2
		FString CacheBucket;         // groups similar contexts for cache keying
	};

	// Result returned (async) by the manager.
	struct FLLMResult
	{
		bool bSuccess = false;
		ELLMTier Tier = ELLMTier::None;
		FDialogueLLMResponse Response;   // parsed (Line + Actions)
		FString RawText;                 // fallback raw string
		float CostUSD = 0.f;             // for telemetry / budget guard
		FString Error;
	};

	// Async completion delegate.
	DECLARE_DELEGATE_TwoParams(FOnLLMResult, const FLLMRequest& /*Req*/, const FLLMResult& /*Res*/);
}
