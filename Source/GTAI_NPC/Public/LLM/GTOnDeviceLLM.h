// GTOnDeviceLLM.h
// Tier-1 on-device inference. Wraps llama.cpp (GGML) or the NVIDIA ACE LLM
// plugin. Runs Phi-3 / Nemotron-Nano-9B locally for ambient barks, intent
// classification, and memory consolidation. No cost, <120ms target.
// Plain C++ class (no UObject).
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "LLM/GTLLMTypes.h"

namespace GTAI::NPC
{
	class GTAI_NPC_API FOnDeviceLLM
	{
	public:
		// Load a GGUF model (Phi-3-mini / Nemotron-Nano-9B-v2 Q4_K_M).
		bool LoadModel(const FString& GgufPath, int32 MaxParallelSessions = 4);

		// Generation on a worker thread. Short outputs only.
		void Generate(const FLLMRequest& Req, const FOnLLMResult& Callback);

		bool IsReady() const { return bReady; }
		FString GetModelName() const { return ModelName; }

	private:
		bool bReady = false;
		FString ModelName;
		int32 ParallelCap = 4;
		void* NativeHandle = nullptr;   // llama.cpp context / ACE session pool
	};
}
