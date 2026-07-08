// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "GTAI_QuestTypes.generated.h"

/** Quest objective types. */
UENUM(BlueprintType)
enum class EGTAI_QuestObjectiveType : uint8
{
    Goto        UMETA(DisplayName = "Go To Location"),
    Collect     UMETA(DisplayName = "Collect Item"),
    Deliver     UMETA(DisplayName = "Deliver Item"),
    Assassinate UMETA(DisplayName = "Eliminate Target"),
    Escort      UMETA(DisplayName = "Escort NPC"),
    Hack        UMETA(DisplayName = "Hack Terminal"),
    Race        UMETA(DisplayName = "Reach in Time"),
    Survival    UMETA(DisplayName = "Survive Waves"),
    Investigate UMETA(DisplayName = "Investigate"),
    Talk        UMETA(DisplayName = "Talk to NPC"),
};

/** Quest overall state. */
UENUM(BlueprintType)
enum class EGTAI_QuestState : uint8
{
    Available   UMETA(DisplayName = "Available"),
    Active      UMETA(DisplayName = "Active"),
    Completed   UMETA(DisplayName = "Completed"),
    Failed      UMETA(DisplayName = "Failed"),
    Abandoned   UMETA(DisplayName = "Abandoned"),
};

/** Quest priority (affects UI + notification). */
UENUM(BlueprintType)
enum class EGTAI_QuestPriority : uint8
{
    Low,
    Normal,
    High,
    Urgent,
};

/** A single objective within a quest. */
USTRUCT(BlueprintType)
struct GTAI_QUESTS_API FGTAI_QuestObjective
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    EGTAI_QuestObjectiveType Type = EGTAI_QuestObjectiveType::Goto;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString Description;

    /** Target location in world space (for goto/deliver/race). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FVector TargetLocation = FVector::ZeroVector;

    /** Target NPC or item tag. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FGameplayTag TargetTag;

    /** Time limit in seconds (for race/survival). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    float TimeLimit = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    bool bCompleted = false;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    bool bFailed = false;
};

/** Reward structure. */
USTRUCT(BlueprintType)
struct GTAI_QUESTS_API FGTAI_QuestReward
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
    int32 Money = 0;

    /** Faction reputation changes (faction tag -> delta). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
    TMap<FString, int32> ReputationChanges;

    /** Items awarded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
    TArray<FString> Items;

    /** Unlock flags set on completion. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward")
    TArray<FString> Unlocks;
};

/** A complete quest definition. */
USTRUCT(BlueprintType)
struct GTAI_QUESTS_API FGTAI_QuestDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString GiverId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    EGTAI_QuestPriority Priority = EGTAI_QuestPriority::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    TArray<FGTAI_QuestObjective> Objectives;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FGTAI_QuestReward Rewards;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    TArray<FString> FailConditions;

    /** Branch map: outcome key -> next quest ID. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    TMap<FString, FString> Branching;

    /** Dialogue lines (AI-generated). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString IntroDialogue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString SuccessDialogue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FString FailDialogue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    float TimeLimit = 0.f;

    /** Minimum reputation required to accept. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    TMap<FString, int32> MinReputation;
};

/** Active quest state (runtime tracking). */
USTRUCT(BlueprintType)
struct GTAI_QUESTS_API FGTAI_ActiveQuest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    FString QuestId;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    EGTAI_QuestState State = EGTAI_QuestState::Available;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    int32 CurrentObjectiveIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    float ElapsedTime = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Quest")
    FDateTime AcceptedAt;
};
