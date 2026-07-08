// Copyright GTAI. All Rights Reserved.
// GTAI_Core — Shared types and interfaces used across all modules

#pragma once

#include "CoreMinimal.h"
#include "GTAI_CoreTypes.generated.h"

/** NPC relationship tier. */
UENUM(BlueprintType)
enum class EGTAI_Relationship : uint8
{
    Hostile     UMETA(DisplayName = "Hostile"),
    Unfriendly  UMETA(DisplayName = "Unfriendly"),
    Neutral     UMETA(DisplayName = "Neutral"),
    Friendly    UMETA(DisplayName = "Friendly"),
    Allied      UMETA(DisplayName = "Allied"),
};

/** District status. */
UENUM(BlueprintType)
enum class EGTAI_DistrictStatus : uint8
{
    Calm        UMETA(DisplayName = "Calm"),
    Tense       UMETA(DisplayName = "Tense"),
    Riot        UMETA(DisplayName = "Riot"),
    Lockdown    UMETA(DisplayName = "Lockdown"),
};

/** Time of day. */
UENUM(BlueprintType)
enum class EGTAI_TimeOfDay : uint8
{
    Dawn        UMETA(DisplayName = "Dawn (5-7 AM)"),
    Morning     UMETA(DisplayName = "Morning (7-12 PM)"),
    Afternoon   UMETA(DisplayName = "Afternoon (12-5 PM)"),
    EveningRush UMETA(DisplayName = "Evening Rush (5-7 PM)"),
    Evening     UMETA(DisplayName = "Evening (7-10 PM)"),
    Night       UMETA(DisplayName = "Night (10 PM-5 AM)"),
};

/** Weather type. */
UENUM(BlueprintType)
enum class EGTAI_Weather : uint8
{
    Clear,
    Cloudy,
    Rain,
    Storm,
    Snow,
    Fog,
};

/** Compact location data passed between systems. */
USTRUCT(BlueprintType)
struct GTAI_CORE_API FGTAI_Location
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FVector Position = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite) FRotator Rotation = FRotator::ZeroRotator;
    UPROPERTY(BlueprintReadWrite) FString District;
    UPROPERTY(BlueprintReadWrite) int32 CellIndex = INDEX_NONE;
};

/** World event broadcast to all systems. */
USTRUCT(BlueprintType)
struct GTAI_CORE_API FGTAI_WorldEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FName EventType;
    UPROPERTY(BlueprintReadWrite) FVector Location;
    UPROPERTY(BlueprintReadWrite) float Severity = 0.f;
    UPROPERTY(BlueprintReadWrite) FString Description;
    UPROPERTY(BlueprintReadWrite) AActor* Instigator = nullptr;
};
