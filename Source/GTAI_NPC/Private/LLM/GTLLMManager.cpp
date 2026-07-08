// GTLLMManager.cpp
// Three-tier router: on-device → cache → cloud (DeepSeek V4 via OpenAI-compatible API)

#include "GTLLMManager.h"
#include "GTOnDeviceLLM.h"
#include "GTDeepSeekClient.h"
#include "Engine/World.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"

namespace GTAI::NPC
{
	FLLMManager::FLLMManager()
	{
		// Initialize components lazily on first use
	}

	FLLMManager::~FLLMManager()
	{
		if (OnDevice)
		{
			delete OnDevice;
			OnDevice = nullptr;
		}
		if (Cloud)
		{
			delete Cloud;
			Cloud = nullptr;
		}
	}

	void FLLMManager::Initialize()
	{
		// Initialize on-device LLM (Phi-3 / Nemotron-Nano-9B)
		OnDevice = new FOnDeviceLLM();
		const FString ModelPath = FPaths::ProjectContentDir() / TEXT("AI/Models/Phi-3-mini-4k-instruct-q4.gguf");
		if (!OnDevice->LoadModel(ModelPath, 4)) // 4 parallel sessions
		{
			UE_LOG(LogTemp, Warning, TEXT("GTLLMManager: Failed to load on-device LLM"));
		}

		// Initialize cloud client (DeepSeek V4 via OpenAI-compatible API)
		Cloud = new FDeepSeekClient();
		const FString ApiKey = FApp::GetProjectSetting(TEXT("/Script/Engine.GameEngine"), TEXT("DeepSeekApiKey"));
		if (!ApiKey.IsEmpty())
		{
			Cloud->Configure(ApiKey);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GTLLMManager: DeepSeek API key not configured"));
		}
	}

	void FLLMManager::Shutdown()
	{
		if (OnDevice)
		{
			delete OnDevice;
			OnDevice = nullptr;
		}
		if (Cloud)
		{
			delete Cloud;
			Cloud = nullptr;
		}
		ResponseCache.Empty();
	}

	void FLLMManager::Generate(const FLLMRequest& Req, const FOnLLMResult& Callback)
	{
		if (!OnDevice || !Cloud)
		{
			// Fallback: if not initialized, try to init now
			Initialize();
			if (!OnDevice || !Cloud)
			{
				FLLMResult FailResult;
				FailResult.bSuccess = false;
				FailResult.Error = TEXT("LLM manager not initialized");
				AsyncTask(ENamedThreads::GameThread, [Callback, Req, FailResult]() mutable {
					Callback.ExecuteIfBound(Req, FailResult);
				});
				return;
			}
		}

		// Tier 0: Disallow cloud if requested
		if (!Req.bAllowCloud)
		{
			HandleOnDevice(Req, Callback);
			return;
		}

		// Tier 1: Try on-device first (fast, free)
		if (OnDevice->IsReady())
		{
			HandleOnDevice(Req, Callback);
			return;
		}

		// Tier 2: Check cache (if enabled and cacheable)
		if (!Req.CacheBucket.IsEmpty() && Req.MaxTokens < 256) // cache only short prompts
		{
			const FString CacheKey = FString::Printf(TEXT("%s|%s|%d|%.2f"), 
				*Req.CacheBucket, *Req.Prompt, Req.MaxTokens, Req.Temperature);
			if (ResponseCache.Contains(CacheKey))
			{
				FLLMResult CachedResult;
				CachedResult.bSuccess = true;
				CachedResult.Tier = ELLMTier::Cache;
				CachedResult.RawText = ResponseCache[CacheKey];
				CachedResult.Response = FDialogueLLMResponse::FromRawText(CachedResult.RawText); // simple parse
				CachedResult.CostUSD = 0.f;
				CacheHits++;
				AsyncTask(ENamedThreads::GameThread, [Callback, Req, CachedResult]() mutable {
					Callback.ExecuteIfBound(Req, CachedResult);
				});
				return;
			}
		}

		// Tier 3: Cloud (DeepSeek V4) - check budget
		if (RollingDeepCost >= DeepBudgetPerHour)
		{
			UE_LOG(LogTemp, Warning, TEXT("GTLLMManager: DeepSeek hourly budget exceeded"));
			// Fallback to on-device if available, else fail
			if (OnDevice->IsReady())
			{
				HandleOnDevice(Req, Callback);
				return;
			}
			else
			{
				FLLMResult FailResult;
				FailResult.bSuccess = false;
				FailResult.Error = TEXT("LLM budget exceeded and on-device unavailable");
				AsyncTask(ENamedThreads::GameThread, [Callback, Req, FailResult]() mutable {
					Callback.ExecuteIfBound(Req, FailResult);
				});
				return;
			}
		}

		// Go to cloud
		HandleCloud(Req, Callback);
	}

	void FLLMManager::HandleCacheHit(const FLLMRequest& Req, const FString& Key, const FOnLLMResult& CB)
	{
		if (ResponseCache.Contains(Key))
		{
			FLLMResult CachedResult;
			CachedResult.bSuccess = true;
			CachedResult.Tier = ELLMTier::Cache;
			CachedResult.RawText = ResponseCache[Key];
			CachedResult.Response = FDialogueLLMResponse::FromRawText(CachedResult.RawText);
			CachedResult.CostUSD = 0.f;
			CacheHits++;
			AsyncTask(ENamedThreads::GameThread, [CB, Req, CachedResult]() mutable {
				CB.ExecuteIfBound(Req, CachedResult);
			});
		}
		else
		{
			// Cache miss - fall back to on-device
			HandleOnDevice(Req, CB);
		}
	}

	void FLLMManager::HandleOnDevice(const FLLMRequest& Req, const FOnLLMResult& CB)
	{
		if (OnDevice && OnDevice->IsReady())
		{
			OnDevice->Generate(Req, CB);
			CacheMisses++;
		}
		else
		{
			// Fallback to cloud if on-device not ready
			if (Cloud && RollingDeepCost < DeepBudgetPerHour)
			{
				HandleCloud(Req, CB);
			}
			else
			{
				FLLMResult FailResult;
				FailResult.bSuccess = false;
				FailResult.Error = TEXT("On-device LLM not ready and cloud unavailable/budget exceeded");
				AsyncTask(ENamedThreads::GameThread, [CB, Req, FailResult]() mutable {
					CB.ExecuteIfBound(Req, FailResult);
				});
			}
		}
	}

	void FLLMManager::HandleCloud(const FLLMRequest& Req, const FOnLLMResult& CB)
	{
		if (Cloud)
		{
			Cloud->Generate(Req, [this, CB, Req](const FLLMRequest& OriginalReq, const FLLMResult& Result)
			{
				// Update rolling cost (simple moving average over last hour)
				static const float DecayFactor = 0.99f; // ~1 hour half-life
				RollingDeepCost = RollingDeepCost * DecayFactor + Result.CostUSD;
				if (RollingDeepCost > DeepBudgetPerHour * 2) // hard cap
				{
					RollingDeepCost = DeepBudgetPerHour * 2;
				}

				// Cache successful short responses
				if (Result.bSuccess && !OriginalReq.CacheBucket.IsEmpty() && OriginalReq.MaxTokens < 256)
				{
					const FString CacheKey = FString::Printf(TEXT("%s|%s|%d|%.2f"),
						*OriginalReq.CacheBucket, *OriginalReq.Prompt, OriginalReq.MaxTokens, OriginalReq.Temperature);
					if (ResponseCache.Num() >= CacheCap)
					{
						// Remove oldest entry (simple FIFO - in practice would use LRU)
						if (ResponseCache.Num() > 0)
						{
							const FString OldestKey = ResponseCache.GetKeys()[0];
							ResponseCache.Remove(OldestKey);
						}
					}
					ResponseCache.Add(CacheKey, Result.RawText);
				}

				CB.ExecuteIfBound(OriginalReq, Result);
			});
		}
		else
		{
			FLLMResult FailResult;
			FailResult.bSuccess = false;
			FailResult.Error = TEXT("Cloud LLM not configured");
			AsyncTask(ENamedThreads::GameThread, [CB, Req, FailResult]() mutable {
				CB.ExecuteIfBound(Req, FailResult);
			});
		}
	}
}

} // namespace GTAI::NPC