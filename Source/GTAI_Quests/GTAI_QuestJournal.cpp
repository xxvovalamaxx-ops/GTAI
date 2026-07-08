// Copyright GTAI. All Rights Reserved.
// SAGA — Quest Journal Implementation

#include "GTAI_QuestJournal.h"
#include "GTAI_QuestTypes.h"
#include "GTAI_ReputationSystem.h"

void UGTAI_QuestJournal::RegisterQuestDefinition(const FGTAI_QuestDefinition& Def)
{
    QuestDefinitions.Add(Def.Id, Def);
}

bool UGTAI_QuestJournal::OfferQuest(const FGTAI_QuestDefinition& QuestDef)
{
    RegisterQuestDefinition(QuestDef);

    FGTAI_ActiveQuest Active;
    Active.QuestId = QuestDef.Id;
    Active.State = EGTAI_QuestState::Available;
    Active.AcceptedAt = FDateTime::UtcNow();

    ActiveQuests.Add(QuestDef.Id, Active);
    return true;
}

bool UGTAI_QuestJournal::AcceptQuest(const FString& QuestId)
{
    if (FGTAI_ActiveQuest* Active = ActiveQuests.Find(QuestId))
    {
        if (Active->State != EGTAI_QuestState::Available) return false;
        Active->State = EGTAI_QuestState::Active;
        Active->CurrentObjectiveIndex = 0;
        OnQuestAccepted.Broadcast(QuestId);
        return true;
    }
    return false;
}

void UGTAI_QuestJournal::AbandonQuest(const FString& QuestId)
{
    if (FGTAI_ActiveQuest* Active = ActiveQuests.Find(QuestId))
    {
        Active->State = EGTAI_QuestState::Abandoned;
    }
}

void UGTAI_QuestJournal::CompleteObjective(const FString& QuestId)
{
    FGTAI_ActiveQuest* Active = ActiveQuests.Find(QuestId);
    FGTAI_QuestDefinition* Def = QuestDefinitions.Find(QuestId);
    if (!Active || !Def || Active->State != EGTAI_QuestState::Active) return;

    int32 ObjIdx = Active->CurrentObjectiveIndex;
    if (ObjIdx >= 0 && ObjIdx < Def->Objectives.Num())
    {
        Def->Objectives[ObjIdx].bCompleted = true;
    }

    OnObjectiveCompleted.Broadcast(QuestId, ObjIdx);

    Active->CurrentObjectiveIndex++;
    if (Active->CurrentObjectiveIndex >= Def->Objectives.Num())
    {
        CompleteQuest(QuestId);
    }
}

void UGTAI_QuestJournal::FailQuest(const FString& QuestId, const FString& FailReason)
{
    if (FGTAI_ActiveQuest* Active = ActiveQuests.Find(QuestId))
    {
        Active->State = EGTAI_QuestState::Failed;
        FailedQuests.Add(QuestId);
        OnQuestFailed.Broadcast(QuestId, FailReason);
    }
}

void UGTAI_QuestJournal::CompleteQuest(const FString& QuestId)
{
    FGTAI_ActiveQuest* Active = ActiveQuests.Find(QuestId);
    FGTAI_QuestDefinition* Def = QuestDefinitions.Find(QuestId);
    if (!Active || !Def) return;

    Active->State = EGTAI_QuestState::Completed;
    CompletedQuests.Add(QuestId);

    GrantRewards(Def->Rewards);
    OnQuestCompleted.Broadcast(QuestId);
}

TArray<FGTAI_ActiveQuest> UGTAI_QuestJournal::GetActiveQuests() const
{
    TArray<FGTAI_ActiveQuest> Result;
    for (const auto& Pair : ActiveQuests)
        if (Pair.Value.State == EGTAI_QuestState::Active)
            Result.Add(Pair.Value);
    return Result;
}

bool UGTAI_QuestJournal::GetActiveQuest(const FString& QuestId, FGTAI_ActiveQuest& OutQuest) const
{
    if (const FGTAI_ActiveQuest* Found = ActiveQuests.Find(QuestId))
    {
        OutQuest = *Found;
        return true;
    }
    return false;
}

bool UGTAI_QuestJournal::GetQuestDefinition(const FString& QuestId, FGTAI_QuestDefinition& OutDef) const
{
    if (const FGTAI_QuestDefinition* Found = QuestDefinitions.Find(QuestId))
    {
        OutDef = *Found;
        return true;
    }
    return false;
}

bool UGTAI_QuestJournal::IsQuestAvailable(const FString& QuestId, const TMap<FString, int32>& PlayerReputation) const
{
    const FGTAI_QuestDefinition* Def = QuestDefinitions.Find(QuestId);
    if (!Def) return false;

    for (const auto& Req : Def->MinReputation)
    {
        if (const int32* Standing = PlayerReputation.Find(Req.Key))
        {
            if (*Standing < Req.Value) return false;
        }
        else if (Req.Value > 0) return false;
    }
    return true;
}

void UGTAI_QuestJournal::GrantRewards(const FGTAI_QuestReward& Rewards)
{
    // TODO: Connect to Economy system for money
    // TODO: Connect to Reputation system for faction changes
    // TODO: Connect to Inventory for items
    // TODO: Set unlock flags
}
