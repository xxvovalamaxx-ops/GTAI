// Copyright GTAI. All Rights Reserved.

#include "GTA7_GameMode.h"
#include "GTAI_World_WorldStateManager.h"
#include "GTAI_World_Wanted.h"
#include "GTAI_World_TrafficSpawner.h"
#include "GTNPCOrchestrator.h"
#include "GTAI_RadioSystem.h"
#include "GTAI_AudioManager.h"

AGTA7_GameMode::AGTA7_GameMode() { PrimaryActorTick.bCanEverTick = true; }

void AGTA7_GameMode::BeginPlay()
{
    Super::BeginPlay();
    InitializeGTAISystems();
}

void AGTA7_GameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    WorldTickTimer += DeltaSeconds;
    if (WorldTickTimer >= WorldTickInterval)
    {
        WorldTickTimer = 0.f;

        if (auto* WS = GetGameInstance()->GetSubsystem<UGTAI_WorldStateManager>())
            WS->WorldTick(WorldTickInterval);
    }
}

void AGTA7_GameMode::InitializeGTAISystems()
{
    auto* WS = GetGameInstance()->GetSubsystem<UGTAI_WorldStateManager>();
    auto* Wanted = NewObject<UGTAI_WorldWantedSystem>();
    auto* Traffic = NewObject<UGTAI_WorldTrafficSpawner>();
    auto* Radio = GetGameInstance()->GetSubsystem<UGTAI_RadioSystem>();
    auto* Audio = GetGameInstance()->GetSubsystem<UGTAI_AudioManager>();

    if (WS)
    {
        if (Wanted) { Wanted->Initialize(WS); WS->RegisterSystem(TEXT("Wanted"), Wanted); }
        if (Traffic) { Traffic->Initialize(WS); WS->RegisterSystem(TEXT("Traffic"), Traffic); }
        if (Radio) Radio->PowerOn();
        if (Audio) Audio->Initialize();

        UE_LOG(LogTemp, Log, TEXT("[GTA7] All GTAI systems initialized."));
    }
}
