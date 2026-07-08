// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_QuestTypes.h"

#include "GTAI_QuestJournal.generated.h"

/**
 * Central quest tracking system. Manages active, completed, and available quests.
 * Accessible from any system via GetGameInstance()->GetSubsystem<UGTAI_QuestJournal>().
 */
UCLASS()
class GTAI_QUESTS_API UGTAI_QuestJournal : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // --- Quest Lifecycle ---

    /** Offer a quest to the player (returns true if accepted). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Quests")
    bool OfferQuest(const FGTAI_QuestDefinition& QuestDef);

    /** Accept a quest (moves from available to active). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Quests")
    bool AcceptQuest(const FString& QuestId);

    /** Abandon an active quest. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Quests")
    void AbandonQuest(const FString& QuestId);

    /** Complete the current objective of a quest. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Quests")
    void CompleteObjective(const FString& QuestId);

    /** Fail a quest (triggered by fail condition). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Quests")
    void FailQuest(const FString& QuestId, const FString& FailReason);

    /** Complete a quest (all objectives done, grant rewards). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Quests")
    void CompleteQuest(const FString& QuestId);

    // --- Queries ---

    /** Get all active quests. */
    UFUNCTION(BlueprintPure, Category = "GTAI|Quests")
    TArray<FGTAI_ActiveQuest> GetActiveQuests() const;

    /** Get a specific active quest. */
    UFUNCTION(BlueprintPure, Category = "GTAI|Quests")
    bool GetActiveQuest(const FString& QuestId, FGTAI_ActiveQuest& OutQuest) const;

    /** Get quest definition by ID. */
    UFUNCTION(BlueprintPure, Category = "GTAI|Quests")
    bool GetQuestDefinition(const FString& QuestId, FGTAI_QuestDefinition& OutDef) const;

    /** Check if a quest is available given current reputation. */
    UFUNCTION(BlueprintPure, Category = "GTAI|Quests")
    bool IsQuestAvailable(const FString& QuestId, const TMap<FString, int32>& PlayerReputation) const;

    // --- Delegates ---

    /** Fired when a quest is accepted. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAI_OnQuestAccepted, FString, QuestId);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Quests")
    FGTAI_OnQuestAccepted OnQuestAccepted;

    /** Fired when an objective is completed. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGTAI_OnObjectiveCompleted, FString, QuestId, int32, ObjectiveIndex);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Quests")
    FGTAI_OnObjectiveCompleted OnObjectiveCompleted;

    /** Fired when a quest is completed (rewards granted). */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAI_OnQuestCompleted, FString, QuestId);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Quests")
    FGTAI_OnQuestCompleted OnQuestCompleted;

    /** Fired when a quest fails. */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGTAI_OnQuestFailed, FString, QuestId, FString, Reason);
    UPROPERTY(BlueprintAssignable, Category = "GTAI|Quests")
    FGTAI_OnQuestFailed OnQuestFailed;

protected:
    /** All known quest definitions (loaded from data tables or generated). */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Quests")
    TMap<FString, FGTAI_QuestDefinition> QuestDefinitions;

    /** Currently active quests. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Quests")
    TMap<FString, FGTAI_ActiveQuest> ActiveQuests;

    /** Completed quest IDs (for chain unlocking). */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Quests")
    TArray<FString> CompletedQuests;

    /** Failed quest IDs. */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "GTAI|Quests")
    TArray<FString> FailedQuests;

    /** Register a quest definition (from LLM generation or data table). */
    UFUNCTION(BlueprintCallable, Category = "GTAI|Quests")
    void RegisterQuestDefinition(const FGTAI_QuestDefinition& Def);

    /** Grant rewards for a completed quest. */
    void GrantRewards(const FGTAI_QuestReward& Rewards);

    /** Check fail conditions for all active quests (called on tick or event). */
    void CheckFailConditions();
};
