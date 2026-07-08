// GTOnDeviceLLM.cpp
// Tier-1 on-device inference. Wraps llama.cpp (GGML) or the NVIDIA ACE LLM
// plugin. Runs Phi-3 / Nemotron-Nano-9B locally for ambient barks, intent
// classification, and memory consolidation. No cost, <120ms target.
// Plain C++ class (no UObject).

#include "GTOnDeviceLLM.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Async/Async.h"
#include "Misc/ScopeLock.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#include "HideWindowsPlatformTypes.h"
#endif

namespace GTAI::NPC
{
	class FOnDeviceLLMWorker : public FRunnable
	{
	public:
		FOnDeviceLLMWorker(FOnDeviceLLM* InOwner, const FLLMRequest& InReq, const FOnLLMResult& InCallback)
			: Owner(InOwner), Req(InReq), Callback(InCallback), bShouldRun)
		{
			bShouldRun = true;
		}

		virtual uint32 Run() override
		{
			// Simulate llama.cpp inference - in real implementation this would call llama.cpp
			// For now, simulate with a simple echo + delay
			FPlatformProcess::Sleep(0.1f); // Simulate 100ms inference time

			FLLMResult Result;
			Result.bSuccess = true;
			Result.Tier = ELLMTier::OnDevice;
			
			// Simple echo response for demo - in real implementation this would be actual LLM output
			Result.RawText = TEXT("On-device response to: ") + Req.Prompt.Left(50);
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
		std::atomic<bool> bInRun{false};
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