// GTDeepSeekClient.h
// Cloud Tier-3 client for DeepSeek v4-flash (OpenAI-compatible).
// Uses prompt caching (prefix cache of system + city-state) to hit the
// $0.0028/M cache-hit input price. Streaming enabled for UI.
// Plain C++ class (no UObject) — uses FHttpModule directly.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "LLM/GTLLMTypes.h"

namespace GTAI::NPC
{
	class GTAI_NPC_API FDeepSeekClient
	{
	public:
		// Configure endpoint + key. Defaults to api.deepseek.com (OpenAI format).
		void Configure(const FString& InApiKey,
		               const FString& InBaseUrl = TEXT("https://api.deepseek.com"),
		               const FString& InModel = TEXT("deepseek-v4-flash"));

		// Async generation via HTTP; result via Callback.
		void Generate(const FLLMRequest& Req, const FOnLLMResult& Callback);

		float GetLastCostUSD() const { return LastCost; }

	private:
		static void ParseUsage(const FString& JsonBody, FLLMResult& Out);
		FString ApiKey;
		FString BaseUrl;
		FString Model;
		float LastCost = 0.f;

		// Pricing (2026-07): cache-hit in $0.0028/M, miss in $0.14/M, out $0.28/M.
		static constexpr float PriceCacheHitPerM = 0.0028f;
		static constexpr float PriceCacheMissPerM = 0.14f;
		static constexpr float PriceOutPerM = 0.28f;
	};
}
