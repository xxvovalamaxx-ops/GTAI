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
		
		// Ensure URL doesn't end with slash for proper endpoint construction
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

		// Build request URL
		const FString Url = BaseUrl + TEXT("/v1/chat/completions");

		// Build JSON payload
		TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		JsonObject->SetStringField(TEXT("model"), Model);
		
		TArray<TSharedPtr<FJsonValue>> MessagesArray;
		
		// System message with caching hint (prefix caching)
		TSharedPtr<FJsonObject> SystemMsg = MakeShareable(new FJsonObject);
		SystemMsg->SetStringField(TEXT("role"), TEXT("system"));
		SystemMsg->SetStringField(TEXT("content"), Req.Prompt.Left(1024)); // Limit prompt size
		MessagesArray.Add(SystemMsg);
		
		// User message (if any additional context beyond system prompt)
		TSharedPtr<FJsonObject> UserMsg = MakeShareable(new FJsonObject);
		UserMsg->SetStringField(TEXT("role"), TEXT("user"));
		UserMsg->SetStringField(TEXT("content"), TEXT("")); // Empty for now, prompt in system
		MessagesArray.Add(UserMsg);
		
		JsonObject->SetArrayField(TEXT("messages"), MessagesArray);
		JsonObject->SetNumberField(TEXT("temperature"), Req.Temperature);
		JsonObject->SetNumberField(TEXT("max_tokens"), Req.MaxTokens);
		JsonObject->SetBoolField(TEXT("stream"), false); // Non-streaming for simplicity
		
		// Enable prompt caching (prefix caching) by including system prompt
		// DeepSeek V4 supports prompt caching automatically for repeated prefixes
		JsonObject->SetBoolField(TEXT("cache_prompt"), true);
		JsonObject->SetNumberField(TEXT("top_p"), 0.95f);
		
		// Serialize JSON
		FString JsonPayload;
		TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&JsonPayload);
		FJsonSerializer::Serialize(JsonObject, Writer);
		
		// Create HTTP request
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Url);
		Request->SetVerb(TEXT("POST"));
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ApiKey);
		Request->SetContentAsString(JsonPayload);
		
		// Set up response handler
		Request->OnProcessRequestComplete().BindLambda([this, Callback, Req](FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
		{
			FLLMResult Result;
			Result.bSuccess = false;
			
			if (!bWasSuccessful || !ResponsePtr.IsValid())
			{
				Result.Error = FString::Printf(TEXT("HTTP request failed: %s"), 
					*bWasSuccessful ? ResponsePtr->GetContentAsString() : TEXT("No response"));
				Callback.ExecuteIfBound(Req, Result);
				return;
			}
			
			int32 StatusCode = ResponsePtr->GetResponseCode();
			if (StatusCode != 200)
			{
				Result.Error = FString::Printf(TEXT("HTTP error %d: %s"), 
					StatusCode, *ResponsePtr->GetContentAsString());
				Callback.ExecuteIfBound(Req, Result);
				return;
			}
			
			// Parse JSON response
			const FString& ResponseString = ResponsePtr->GetContentAsString();
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
			TSharedPtr<FJsonObject> JsonResp;
			
			if (!FJsonSerializer::Deserialize(Reader, JsonResp) || !JsonResp.IsValid())
			{
				Result.Error = TEXT("Failed to parse JSON response");
				Callback.ExecuteIfBound(Req, Result);
				return;
			}
			
			// Check for error in response
			if (JsonResp->HasField(TEXT("error")))
			{
				TSharedPtr<FJsonObject> ErrorObj = JsonResp->GetObjectField(TEXT("error"));
				Result.Error = ErrorObj->GetStringField(TEXT("message"));
				Callback.ExecuteIfBound(Req, Result);
				return;
			}
			
			// Extract response
			if (JsonResp->HasField(TEXT("choices")) && JsonResp->GetArrayField(TEXT("choices")).Num() > 0)
			{
				TSharedPtr<FJsonObject> Choice = JsonResp->GetArrayField(TEXT("choices"))[0]->AsObject();
				if (Choice && Choice->HasField(TEXT("message")))
				{
					TSharedPtr<FJsonObject> Message = Choice->GetObjectField(TEXT("message"));
					if (Message && Message->HasField(TEXT("content")))
					{
						Result.bSuccess = true;
						Result.Tier = ELLMTier::Cloud;
						Result.RawText = Message->GetStringField(TEXT("content"));
						Result.Response = FDialogueLLMResponse::FromRawText(Result.RawText);
						
						// Extract usage and calculate cost
						if (JsonResp->HasField(TEXT("usage")))
						{
							TSharedPtr<FJsonObject> Usage = JsonResp->GetObjectField(TEXT("usage"));
							int32 PromptTokens = Usage->GetNumberField(TEXT("prompt_tokens"));
							int32 CompletionTokens = Usage->GetNumberField(TEXT("completion_tokens"));
							
							// Determine if we got cache hit (prompt tokens would be much lower if cached)
							// DeepSeek V4: $0.0028/M cache hit, $0.14/M cache miss, $0.28/M output
							bool bCacheHit = PromptTokens < 100; // Heuristic: cached prompts are much shorter
							float Cost = 0.0f;
							
							if (bCacheHit)
							{
								Cost = (PromptTokens * PriceCacheHitPerM / 1000000.0f) + 
									   (CompletionTokens * PriceOutPerM / 1000000.0f);
							}
							else
							{
								Cost = (PromptTokens * PriceCacheMissPerM / 1000000.0f) + 
									   (CompletionTokens * PriceOutPerM / 1000000.0f);
							}
							
							Result.CostUSD = Cost;
							LastCost = Cost;
						}
						
						Callback.ExecuteIfBound(Req, Result);
						return;
					}
				}
			}
			
			Result.Error = TEXT("Unexpected response format from DeepSeek API");
			Callback.ExecuteIfBound(Req, Result);
		});
		
		// Process the request
		Request->ProcessRequest();
	}

	void FDeepSeekClient::ParseUsage(const FString& JsonBody, FLLMResult& Out)
	{
		// This is kept for compatibility but not used in current implementation
		// Parsing is done inline in Generate() now
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
		TSharedPtr<FJsonObject> JsonObj;
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			if (JsonObj->HasField(TEXT("usage")))
			{
				TSharedPtr<FJsonObject> Usage = JsonObj->GetObjectField(TEXT("usage"));
				if (Usage.IsValid())
				{
					int32 PromptTokens = Usage->GetNumberField(TEXT("prompt_tokens"));
					int32 CompletionTokens = Usage->GetNumberField(TEXT("completion_tokens"));
					
					// Simple cost calculation (would be enhanced with cache hit detection)
					float Cost = (PromptTokens * 0.14f / 1000000.0f) + (CompletionTokens * 0.28f / 1000000.0f);
					Out.CostUSD = Cost;
				}
			}
		}
	}

} // namespace GTAI::NPC