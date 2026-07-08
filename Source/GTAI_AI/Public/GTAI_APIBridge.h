// Copyright GTAI. All Rights Reserved.
// GTAI_AI — Integration layer between game systems and LLM pipeline

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTAI_APIBridge.generated.h"

class UGTAI_WorldStateManager;
class UGTDeepSeekClient;
class UGTOnDeviceLLM;
class UGTLLMManager;

/**
 * Central AI bridge. Routes game-state queries to the LLM pipeline.
 * This is the thin layer between gameplay C++ and the AI backend.
 */
UCLASS()
class GTAI_AI_API UGTAI_APIBridge : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Send a natural-language query about the game world. Returns LLM response. */
    UFUNCTION(BlueprintCallable, Category = "GTAI|AI")
    FString QueryAI(const FString& Prompt, bool bUseCloud = true);

    /** Get city state as JSON for LLM context injection. */
    UFUNCTION(BlueprintPure, Category = "GTAI|AI")
    FString GetCityStateJSON() const;

    /** Get player state as JSON for LLM context injection. */
    UFUNCTION(BlueprintPure, Category = "GTAI|AI")
    FString GetPlayerStateJSON() const;

private:
    UPROPERTY() TObjectPtr<UGTLLMManager> LLMManager;
    UPROPERTY() TObjectPtr<UGTAI_WorldStateManager> WorldState;
};
