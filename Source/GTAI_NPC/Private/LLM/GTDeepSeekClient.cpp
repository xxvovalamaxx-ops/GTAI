// GTDeepSeekClient.cpp
// Cloud Tier-3 client for DeepSeek v4-flash (OpenAI-compatible API)

#include "GTDeepSeekClient.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "Misc/Base64.h"
#include "Misc/ScopeLock.h"

namespace GTAI::NPC
{
	void FDeepSeekClient::Configure(const FString& InApiKey,
		const FString& InBaseUrl,
		const FString& InModel)
	{
		ApiKey = InApiKey;
		BaseUrl = InBaseUrl;
		Model = InModel;
		
		// Ensure no trailing slash
		if (BaseUrl.EndsWith(TEXT("/")))
		{
			BaseUrl = BaseUrl.LeftChop(1);
		}
	}

	void FDeepSeekClient::Generate(const FLLMRequest& Req, const FOnLLMResult& Callback)
	{
		if (ApiKey.IsEmpty())
		{
			FLLMResult FailResult;
			FailResult.bSuccess = false;
			FailResult.Error = TEXT("DeepSeek API key not configured");
			AsyncTask(ENamedThreads::GameThread, [Callback, Req, FailResult]() mutable {
				Callback.ExecuteIfBound(Req, FailResult);
			});
			return;
		}

		// Build request
		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetURL(BaseUrl + TEXT("/v1/chat/completions"));
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ApiKey);
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));

		// Build JSON payload
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PostBody);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("model"), Model);
		
		// Messages array
		Writer->WriteValue(TEXT("messages"), TJsonWriterFactory<>::CreateNullValue()); // Placeholder
		Writer->WriteObjectStart(); // Start messages array
		Writer->WriteValue(TEXT("role"), TEXT("system"));
		Writer->WriteValue(TEXT("content"), TEXT("You are a helpful AI assistant for NPC dialogue generation in a GTA-style game."));
		Writer->WriteObjectEnd(); // End system message
		
		Writer->WriteObjectStart(); // Start user message
		Writer->WriteValue(TEXT("role"), TEXT("user"));
		Writer->WriteValue(TEXT("content"), Req.Prompt);
		Writer->WriteObjectEnd(); // End user message
		
		Writer->WriteObjectEnd(); // End messages array
		
		Writer->WriteValue(TEXT("max_tokens"), Req.MaxTokens);
		Writer->WriteValue(TEXT("temperature"), Req.Temperature);
		Writer->WriteValue(TEXT("stream"), false);
		Writer->WriteObjectEnd();
		Writer->Close();
		
		HttpRequest->SetContentAsString(PostBody);

		// Process response
		HttpRequest->OnProcessRequestComplete().BindLambda([this, Callback, Req](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			FLLMResult Result;
			Result.bSuccess = bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200;
			
			if (Result.bSuccess && Response.IsValid())
			{
				// Parse JSON response
				TSharedPtr<FJsonObject> JsonObject;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
				
				if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
				{
					// Extract response text
					if (JsonObject->HasField(TEXT("choices")) && JsonObject->GetArrayField(TEXT("choices")).Num() > 0)
					{
						TArray<TSharedPtr<FJsonValue>> Choices = JsonObject->GetArrayField(TEXT("choices"));
						if (Choices.Num() > 0 && Choices[0]->Type == EJson::Object)
						{
							TSharedPtr<FJsonObject> ChoiceObj = Choices[0]->AsObject();
							if (ChoiceObj->HasField(TEXT("message")) && ChoiceObj->GetObjectField(TEXT("message"))->HasField(TEXT("content")))
							{
								Result.RawText = ChoiceObj->GetObjectField(TEXT("message"))->GetStringField(TEXT("content"));
								Result.Response = FDialogueLLMResponse::FromRawText(Result.RawText);
							}
						}
					}
					
					// Extract usage/cost
					if (JsonObject->HasField(TEXT("usage")))
					{
						TSharedPtr<FJsonObject> UsageObj = JsonObject->GetObjectField(TEXT("usage"));
						int32 PromptTokens = UsageObj->GetIntegerField(TEXT("prompt_tokens"));
						int32 CompletionTokens = UsageObj->GetIntegerField(TEXT("completion_tokens"));
						int32 TotalTokens = UsageObj->GetIntegerField(TEXT("total_tokens"));
						
						// Calculate cost based on pricing
						bool bCacheHit = JsonObject->HasField(TEXT("cache_hit")) && JsonObject->GetBoolField(TEXT("cache_hit"));
						float PromptCost = (PromptTokens / 1000000.0f) * (bCacheHit ? PriceCacheHitPerM : PriceCacheMissPerM);
						float CompletionCost = (CompletionTokens / 1000000.0f) * PriceOutPerM;
						Result.CostUSD = PromptCost + CompletionCost;
						LastCost = Result.CostUSD;
					}
					
					Result.Tier = ELLMTier::Cloud;
				}
				else
				{
					Result.bSuccess = false;
					Result.Error = TEXT("Failed to parse DeepSeek response JSON");
				}
			}
			else
			{
				if (!Response.IsValid())
				{
					Result.Error = TEXT("No response from DeepSeek API");
				}
				else if (Response->GetResponseCode() != 200)
				{
					Result.Error = FString::Printf(TEXT("DeepSeek API error: %d - %s"), 
						Response->GetResponseCode(), *Response->GetContentAsString());
				}
				else
				{
					Result.Error = TEXT("HTTP request failed");
				}
			}
			
			AsyncTask(ENamedThreads::GameThread, [Callback, Req, Result]() mutable {
				Callback.ExecuteIfBound(Req, Result);
			});
		});

		// Process the request
		HttpRequest->ProcessRequest();
	}
}

} // namespace GTAI::NPC