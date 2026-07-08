// GTAdvisorTypes.h
// PORT of Unity GTAI Advisor (gta7-gtai-advisor-architecture-2026-06-06.md)
// "One engine, three voices" grounded in a city-state snapshot.
// Plain structs under GTAI::NPC; reflected DataAssets declared at global scope.
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "Dialogue/GTPersona.h"

namespace GTAI::NPC
{
	// ---- City state snapshot (port of Unity CityStateData) ----
	struct FAdvisorDistrict
	{
		FString Name;
		FString Type;          // financial, residential, industrial, waterfront
		float CrimeLevel = 0.f;
		float PoliceActivity = 0.f;
		float TrafficDensity = 0.f;
		FString ControllingFaction;
		TArray<FString> Landmarks;
		FString Status;         // calm, tense, riot, lockdown
	};

	struct FAdvisorFaction
	{
		FString Name;
		FString Type;          // police, gang, corporation, political
		float Influence = 0.f;
		float PlayerStanding = 0.f;  // -100..100
		FString Territory;
		FString CurrentActivity;
	};

	struct FAdvisorMission
	{
		FString Id;
		FString Title;
		FString Status;        // available, active, completed, failed
		FString Giver;
		FString District;
		FString Description;
		TArray<FString> Objectives;
	};

	struct FAdvisorPlayer
	{
		FString Name;
		float Health = 100.f;
		float Armor = 0.f;
		float Money = 0.f;
		int32 WantedLevel = 0;
		FString CurrentDistrict;
		FString CurrentVehicle;
		TArray<FString> Inventory;
	};

	struct FAdvisorEvent
	{
		FString Id;
		FString Type;          // crime, police_raid, traffic, weather, economic
		FString District;
		FString Description;
		FString Timestamp;
		float Severity = 0.f;  // 0..1
	};

	struct FAdvisorCityState
	{
		FString CityName = TEXT("Liberty City");
		int32 Population = 8'400'000;
		float CrimeIndex = 0.f;
		float EconomyIndex = 0.f;
		float PolicePresence = 0.f;
		FString CurrentTime;
		FString Weather;
		TArray<FAdvisorDistrict> Districts;
		TArray<FAdvisorFaction> Factions;
		TArray<FAdvisorMission> ActiveMissions;
		FAdvisorPlayer Player;
		TArray<FAdvisorEvent> RecentEvents;

		FString ToJson() const;             // FJsonObjectConverter-backed
		static FAdvisorCityState FromJson(const FString& Json);
	};

	// ---- Advisor message (port of Unity AdvisorMessage) ----
	struct FAdvisorMessage
	{
		FString Role;          // "system", "user", "advisor"
		FString Content;
		FString Timestamp;
		FName PersonaId;
	};

	// The three canonical advisor personas (see design doc §10).
	enum class EAdvisorPersona : uint8
	{
		Dispatcher,   // police dispatcher voice
		Fixer,        // street informant voice
		CityAnalyst   // data-driven analyst voice
	};

	// Returns the built-in persona definition for a given advisor role.
	GTAI_NPC_API FPersona MakeAdvisorPersona(EAdvisorPersona Kind);
}
