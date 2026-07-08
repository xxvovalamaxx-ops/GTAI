// GTLLMManager.h
// Three-tier inference router. Resolves each request to:
//   Tier 2 (cache) -> Tier 1 (on-device) -> Tier 3 (cloud DeepSeek).
// Plain C++ class (UE reflection cannot place a UCLASS in a namespace).
// Owns budget telemetry to enforce the <$0.01-per-deep-interaction target.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "LLM/GTLLMTypes.h"

namespace GTAI::NPC
{
	class FOnDeviceLLM;
	class FDeepSeekClient;

	class GTAI_NPC_API FLLMManager
	{
	public:
		void Initialize(FOnDeviceLLM* InOnDevice, FDeepSeekClient* InCloud);

		// Entry point. Resolves tier, dispatches, and invokes Callback on completion.
		void Generate(const FLLMRequest& Req, const FOnLLMResult& Callback);

		// Budget guard: rolling cost for deep (cloud) interactions.
		float GetRollingDeepCostUSD() const { return RollingDeepCost; }
		void SetDeepBudgetUSD(float PerHour) { DeepBudgetPerHour = PerHour; }
		bool IsWithinBudget() const { return RollingDeepCost < DeepBudgetPerHour; }

		float CacheHitRate() const;
		void ClearCache();

	private:
		ELLMTier ResolveTier(const FLLMRequest& Req, const FString& CacheKey);
		FString MakeCacheKey(const FLLMRequest& Req) const;
		void HandleCacheHit(const FLLMRequest& Req, const FString& Key, const FOnLLMResult& CB);
		void HandleOnDevice(const FLLMRequest& Req, const FOnLLMResult& CB);
		void HandleCloud(const FLLMRequest& Req, const FOnLLMResult& CB);

		FOnDeviceLLM* OnDevice = nullptr;
		FDeepSeekClient* Cloud = nullptr;

		TMap<FString, FString> ResponseCache;   // key -> serialized response
		mutable uint32 CacheHits = 0, CacheMisses = 0;
		int32 CacheCap = 2048;

		float RollingDeepCost = 0.f;
		float DeepBudgetPerHour = 0.10f;         // deep avg ~$0.00026 -> ample
	};
}
