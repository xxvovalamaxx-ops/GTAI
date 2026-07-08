// Copyright GTAI. All Rights Reserved.
// SAGA — Quest Generator Implementation (DeepSeek V4 LLM pipeline)

#include "GTAI_QuestGenerator.h"
#include "GTAI_QuestTypes.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"

void UGTAI_QuestGenerator::GenerateQuest(const FString& WorldStateJSON, const FString& ReputationJSON, const FString& ConstraintsJSON)
{
    FString Prompt = BuildPrompt(WorldStateJSON, ReputationJSON, ConstraintsJSON);
    MakeAPIRequest(Prompt, true);
}

FGTAI_QuestDefinition UGTAI_QuestGenerator::GenerateQuestSync(const FString& WorldStateJSON, const FString& ReputationJSON)
{
    FString Prompt = BuildPrompt(WorldStateJSON, ReputationJSON, TEXT("{}"));
    // In production, block on HTTP request. For now, return empty struct.
    return FGTAI_QuestDefinition();
}

bool UGTAI_QuestGenerator::ValidateQuest(const FGTAI_QuestDefinition& Quest, FString& OutError) const
{
    if (Quest.Id.IsEmpty()) { OutError = TEXT("Missing quest ID"); return false; }
    if (Quest.Title.IsEmpty()) { OutError = TEXT("Missing title"); return false; }
    if (Quest.Objectives.Num() == 0) { OutError = TEXT("No objectives"); return false; }
    if (Quest.Objectives.Num() > 10) { OutError = TEXT("Too many objectives (max 10)"); return false; }
    for (const auto& Obj : Quest.Objectives)
    {
        if (Obj.Description.IsEmpty()) { OutError = TEXT("Objective missing description"); return false; }
    }
    if (Quest.Rewards.Money < 0) { OutError = TEXT("Negative money reward"); return false; }
    return true;
}

FString UGTAI_QuestGenerator::BuildPrompt(const FString& WorldState, const FString& Reputation, const FString& Constraints) const
{
    return FString::Printf(TEXT("%s\nWorld state: %s\nPlayer reputation: %s\nConstraints: %s\nGenerate a quest as JSON."),
        *SystemPrompt, *WorldState, *Reputation, *Constraints);
}

void UGTAI_QuestGenerator::MakeAPIRequest(const FString& Prompt, bool bAsync)
{
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(APIEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *APIKey));

    FString Body = FString::Printf(
        TEXT("{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"max_tokens\":%d,\"response_format\":{\"type\":\"json_object\"}}"),
        *ModelName, *Prompt.Replace(TEXT("\""), TEXT("\\\"")), MaxTokens);

    Request->SetContentAsString(Body);

    if (bAsync)
    {
        Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
        {
            if (bSuccess && Resp.IsValid())
            {
                FGTAI_QuestDefinition Quest;
                FString Response = Resp->GetContentAsString();
                if (ParseQuestJSON(Response, Quest))
                {
                    OnQuestGenerated.Broadcast(Quest);
                    return;
                }
            }
            OnGenerationFailed.Broadcast(TEXT("API request failed"));
        });
    }

    Request->ProcessRequest();
}

bool UGTAI_QuestGenerator::ParseQuestJSON(const FString& JsonResponse, FGTAI_QuestDefinition& OutQuest) const
{
    return FJsonObjectConverter::JsonObjectStringToUStruct(JsonResponse, &OutQuest, 0, 0);
}
