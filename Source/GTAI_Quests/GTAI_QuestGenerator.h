// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_QuestTypes.h"

#include "GTAI_QuestGenerator.generated.h"

/**
 * LLM-driven quest generation system.
 * Takes world state + player reputation + constraints as input,
 * calls DeepSeek V4 API, and outputs validated quest definitions.
 */
UCLASS()
class GTAI_QUESTS_API UGTAI_QuestGenerator : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Generate a quest from world state + reputation. Async — calls OnQuestGenerated delegate. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Quests|Generation")
    void GenerateQuest(const FString& WorldStateJSON, const FString& ReputationJSON, const FString& ConstraintsJSON);

    /** Generate a quest synchronously (blocking, for testing). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Quests|Generation")
    FGTAI_QuestDefinition GenerateQuestSync(const FString& WorldStateJSON, const FString& ReputationJSON);

    /** Validate a generated quest against constraints (schema + balance checks). */
    UFUNCTION(BlueprintPure, Category = "GTAI|Quests|Generation")
    bool ValidateQuest(const FGTAI_QuestDefinition& Quest, FString& OutError) const;

    // --- Delegates ---

    /** Fired when async quest generation completes. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAI_OnQuestGenerated, FGTAI_QuestDefinition, QuestDef);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Quests|Generation")
    FGTAI_OnQuestGenerated OnQuestGenerated;

    /** Fired when quest generation fails. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAI_OnQuestGenerationFailed, FString, Error);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Quests|Generation")
    FGTAI_OnQuestGenerationFailed OnGenerationFailed;

protected:
    /** DeepSeek API endpoint. */
    UPROPERTY(EditDefaultsOnly, Category = "GTAI|Quests|Generation")
    FString APIEndpoint = "https://api.deepseek.com/v1/chat/completions";

    /** Model to use for quest generation. */
    UPROPERTY(EditDefaultsOnly, Category = "GTAI|Quests|Generation")
    FString ModelName = "deepseek-chat";

    /** System prompt for quest generation. */
    UPROPERTY(EditDefaultsOnly, Category = "GTAI|Quests|Generation")
    FString SystemPrompt =
        TEXT("You are a quest generator for an open-world crime game set in NYC. "
             "Given world state and player reputation, generate ONE quest as JSON. "
             "Rules: 3-5 objectives max, balanced rewards, include dialogue, "
             "respect faction relationships, output valid JSON only.");

    /** Max tokens for quest generation. */
    UPROPERTY(EditDefaultsOnly, Category = "GTAI|Quests|Generation")
    int32 MaxTokens = 800;

    /** Parse LLM JSON response into quest definition. */
    bool ParseQuestJSON(const FString& JsonResponse, FGTAI_QuestDefinition& OutQuest) const;

    /** Build the full prompt from inputs. */
    FString BuildPrompt(const FString& WorldState, const FString& Reputation, const FString& Constraints) const;

    /** Make HTTP request to DeepSeek API. */
    void MakeAPIRequest(const FString& Prompt, bool bAsync);
};
