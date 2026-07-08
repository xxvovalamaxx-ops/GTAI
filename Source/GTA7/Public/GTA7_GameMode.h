// Copyright GTAI. All Rights Reserved.
// GTA7 — Main game module (GameMode, GameState, PlayerState)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GTA7_GameMode.generated.h"

UCLASS()
class GTA7_API AGTA7_GameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGTA7_GameMode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** Initialize all GTAI subsystems on game start. */
    UFUNCTION(BlueprintCallable, Category = "GTA7")
    void InitializeGTAISystems();

protected:
    /** World state tick interval (seconds). */
    UPROPERTY(EditDefaultsOnly, Category = "GTA7|Settings")
    float WorldTickInterval = 0.5f;

    float WorldTickTimer;
};
