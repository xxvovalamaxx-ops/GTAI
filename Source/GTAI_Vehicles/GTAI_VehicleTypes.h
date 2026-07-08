// Copyright GTAI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Shared enums and lightweight structs for the GTAI vehicle system.
 * Kept header-only and dependency-light so both C++ and (via reflection) Blueprint can consume it.
 */

/** High-level vehicle class. Drives which tuning DataAsset / behavior profile is selected. */
UENUM(BlueprintType)
enum class EGTAI_VehicleClass : uint8
{
	Invalid		UMETA(DisplayName = "Invalid"),
	Sedan		UMETA(DisplayName = "Sedan (balanced)"),
	SportsCar	UMETA(DisplayName = "Sports Car (fast, twitchy)"),
	Truck		UMETA(DisplayName = "Truck (heavy, slow)"),
	Traffic		UMETA(DisplayName = "Traffic (ambient)"),
};

/** Who is currently controlling the vehicle. */
UENUM(BlueprintType)
enum class EGTAI_VehicleControlMode : uint8
{
	Idle		UMETA(DisplayName = "Idle / Abandoned"),
	Player		UMETA(DisplayName = "Player Controlled"),
	AI			UMETA(DisplayName = "AI Controlled (traffic / companion)"),
	Remote		UMETA(DisplayName = "Network Remote"),
};

/** Mechanical assist features exposed as gameplay systems (not just sim flags). */
UENUM(BlueprintType)
enum class EGTAI_VehicleAssist : uint8
{
	TractionControl,
	ABS,
	StabilityControl,
	SteerAssist,
	DriftAssist,
};

/** Seat identification for entry/exit and passenger NPCs. */
UENUM(BlueprintType)
enum class EGTAI_VehicleSeat : uint8
{
	Driver,
	PassengerFL,
	PassengerFR,
	PassengerRL,
	PassengerRR,
};

/**
 * Compact mechanical-health snapshot replicated/serialized for damage state.
 * 0 = healthy, 1 = destroyed.
 */
USTRUCT(BlueprintType)
struct GTAI_VEHICLES_API FGTAI_VehicleHealth
{
	GENERATED_BODY()

	/** Engine torque output scaling (0 = no power). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage")
	float EngineHealth = 1.f;

	/** Transmission efficiency / shift quality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage")
	float TransmissionHealth = 1.f;

	/** Per-wheel tire grip (index matches WheelSetups order). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage")
	TArray<float> TireHealth = { 1.f, 1.f, 1.f, 1.f };

	/** Per-wheel suspension integrity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage")
	TArray<float> SuspensionHealth = { 1.f, 1.f, 1.f, 1.f };

	/** Per-wheel braking power. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage")
	TArray<float> BrakeHealth = { 1.f, 1.f, 1.f, 1.f };

	/** Cosmetic only, never affects handling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage")
	float VisualDamage = 0.f;

	/** Structural integrity; >=1 means wrecked (undrivable). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Damage")
	float StructuralDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Damage")
	bool bOnFire = false;

	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Damage")
	bool bWrecked = false;
};

/** Delegate fired when a vehicle is entered or exited. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGTAI_OnVehicleOccupantChanged, APawn*, Occupant, EGTAI_VehicleSeat, Seat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAI_OnVehicleWrecked, class AGTAI_BaseVehicle*, Vehicle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTAI_OnVehicleDamaged, float, NewStructuralDamage);
