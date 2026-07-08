// Copyright GTAI. All Rights Reserved.
// ORACLE — World State Manager Implementation

#include "GTAI_World_WorldStateManager.h"
#include "Engine/World.h"

void UGTAI_WorldStateManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("[GTAI] WorldStateManager initialized."));
}

void UGTAI_WorldStateManager::Deinitialize()
{
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("[GTAI] WorldStateManager shutdown."));
}

void UGTAI_WorldStateManager::WorldTick(float DeltaSeconds)
{
    // Tick all managed systems
    UpdateTrafficDensity(DeltaSeconds);
    UpdateEconomy(DeltaSeconds);
    ProcessEvents(DeltaSeconds);
}

void UGTAI_WorldStateManager::SetDistrictHeat(const FString& District, float Heat)
{
    DistrictHeatMap.FindOrAdd(District) = Heat;
    OnDistrictHeatChanged.Broadcast(District, Heat);
}

void UGTAI_WorldStateManager::ClearAllHeat()
{
    DistrictHeatMap.Empty();
}

float UGTAI_WorldStateManager::GetDistrictHeat(const FString& District) const
{
    if (const float* Found = DistrictHeatMap.Find(District))
        return *Found;
    return 0.f;
}

int32 UGTAI_WorldStateManager::GetCellIndex(const FVector& WorldLocation) const
{
    // Convert world position to cell index using grid size
    int32 CellSize = 50000; // 500m cells (UE5 uses cm)
    int32 X = FMath::FloorToInt(WorldLocation.X / CellSize);
    int32 Y = FMath::FloorToInt(WorldLocation.Y / CellSize);
    return X * 10000 + Y; // Simple hash — replace with proper spatial hash in production
}

void UGTAI_WorldStateManager::UpdateTrafficDensity(float DeltaSeconds)
{
    // Traffic density varies by time of day and district
    // TODO: Connect to WorldClock for TOD
    // Rush hours: 7-9am, 4-7pm = 100% density
    // Night: 10pm-5am = 30% density
    // Default: 60% density
}

void UGTAI_WorldStateManager::UpdateEconomy(float DeltaSeconds)
{
    // Economy simulation — property values, shop prices, market trends
    // TODO: Connect to EconomySystem subsystem
}

void UGTAI_WorldStateManager::ProcessEvents(float DeltaSeconds)
{
    // World events: random crimes, traffic accidents, weather changes
    // TODO: Event system integration
}

void UGTAI_WorldStateManager::RegisterSystem(const FName& SystemName, UObject* System)
{
    RegisteredSystems.Add(SystemName, System);
}

UObject* UGTAI_WorldStateManager::GetSystem(const FName& SystemName) const
{
    if (const TObjectPtr<UObject>* Found = RegisteredSystems.Find(SystemName))
        return Found->Get();
    return nullptr;
}

void UGTAI_WorldStateManager::PublishEvent(const FName& EventName, const FString& EventData)
{
    OnWorldEvent.Broadcast(EventName, EventData);
}
