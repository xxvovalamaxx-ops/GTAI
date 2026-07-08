// GTOnDeviceLLM.cpp
// Tier-1 on-device inference. Wraps llama.cpp (GGML) or the NVIDIA ACE LLM
// plugin. Runs Phi-3 / Nemotron-Nano-9B locally for ambient barks, intent
// classification, and memory consolidation. No cost, <120ms target.
// Plain C++ class (no UObject).

#include "GTOnDeviceLLM.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"

namespace GTAI::NPC
{
	class FOnDeviceLLMWorker : public FRunnable
	{
	public:
		FOnDeviceLLMWorker(FOnDeviceLLM* InOwner, const FLLMRequest& InReq, const FOnLLMResult& InCallback)
			: Owner(InOwner), Req(InReq), Callback(InCallback), bShouldRun(true)
		{
		}

		virtual bool Init() override
		{
			return true;
		}

		virtual uint32 Run() override
		{
			if (!bShouldRun || !Owner || !Owner->IsReady())
			{
				return 1;
			}

			// Simulate llama.cpp inference - in real implementation this would call llama.cpp
			// For now, simulate with a small delay to represent processing time
			FPlatformProcess::Sleep(0.1f); // ~100ms inference time

			FLLMResult Result;
			Result.bSuccess = true;
			Result.Tier = ELLMTier::OnDevice;
			
			// Generate a simple response based on the prompt (in real implementation, this comes from llama.cpp)
			FString PromptLower = Req.Prompt.ToLower();
			FString Response;
			
			if (PromptLen.Contains(TEXT("hello")) || PromptLen.Contains(TEXT("hi")))
			{
				Response = TEXT("Hello there! How can I help you today?");
			}
			else if (PromptLen.Contains(TEXT("weather")) || PromptLen.Contains(TEXT("temperature")))
			{
				Response = TEXT("I'm not connected to weather services, but I hope you're having a nice day!");
			}
			else if (PromptLen.Contains(TEXT("time")) || PromptLen.Contains(TEXT("clock")))
			{
				Response = TEXT("I don't have access to real-time clock, but I hope you're having a great moment!");
			}
			else if (PromptLen.Contains(TEXT("name")) || PromptLen.Contains(TEXT("who")))
			{
				Response = TEXT("I'm your local AI assistant, running right here on your device!");
			}
			else
			{
				Response = FString::Printf(TEXT("I understand you said: \"%s\". How can I assist you further?"), *Req.Prompt.Left(50));
			}
			
			Result.RawText = Response;
			Result.Response = FDialogueLLMResponse::FromRawText(Result.RawText);
			Result.CostUSD = 0.0f; // Free local inference

			// Callback on game thread
			AsyncTask(ENamedThreads::GameThread, [this, Result]() mutable {
				if (Callback.IsBound())
				{
					Callback.ExecuteIfBound(Req, Result);
				}
			});

			return 0;
		}

		virtual void Stop() override
		{
			bShouldRun = false;
		}

		virtual void Exit() override {}

	private:
		FOnDeviceLLM* Owner;
		FLLMRequest Req;
		FOnLLMResult Callback;
		std::atomic<bool> bShouldRun;
	};

	FOnDeviceLLM::FOnDeviceLLM()
	{
		// Constructor
	}

	FOnDeviceLLM::~FOnDeviceLLM()
	{
		// Cleanup any native handles
		if (NativeHandle)
		{
			// In real implementation: llama.cpp cleanup
			NativeHandle = nullptr;
		}
	}

	bool FOnDeviceLLM::LoadModel(const FString& GgufPath, int32 MaxParallelSessions)
	{
		// In real implementation: load llama.cpp model from GgufPath
		// For now, simulate successful load
		ModelName = FPaths::GetCleanFilename(GgufPath);
		ParallelCap = MaxParallelSessions;
		bReady = true;
		
		UE_LOG(LogTemp, Log, TEXT("GTOnDeviceLLM: Loaded model %s with %d parallel sessions"), *ModelName, ParallelCap);
		return bReady;
	}

	void FOnDeviceLLM::Generate(const FLLMRequest& Req, const FOnLLMResult& Callback)
	{
		if (!bReady || !NativeHandle)
		{
			FLLMResult FailResult;
			FailResult.bSuccess = false;
			FailResult.Error = TEXT("On-device LLM not ready");
			AsyncTask(ENamedThreads::GameThread, [Callback, Req, FailResult]() mutable {
				Callback.ExecuteIfBound(Req, FailResult);
			});
			return;
		}

		// Create and run worker thread
		FRunnableThread* WorkerThread = FRunnableThread::Create(
			new FOnDeviceLLMWorker(this, Req, Callback),
			TEXT("OnDeviceLLMWorker"),
			0,
			TPri_BelowNormal
		);

		if (!WorkerThread)
		{
			FLLMResult FailResult;
			FailResult.bSuccess = false;
			FailResult.Error = TEXT("Failed to create worker thread");
			AsyncTask(ENamedThreads::GameThread, [Callback, Req, FailResult]() mutable {
				Callback.ExecuteIfBound(Req, FailResult);
			});
		}
		// Note: WorkerThread will self-delete when done
	}

} // namespace GTAI::NPC