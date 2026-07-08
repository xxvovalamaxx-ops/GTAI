// GTPersona.h
// NPC/Advisor persona: voice, system prompt, data-access scope.
// Ported from Unity AdvisorPersonaSO; plain struct for GTAI::NPC.
#pragma once

#include "CoreMinimal.h"

namespace GTAI::NPC
{
	// Data access scope — mirrors the Unity canAccess* flags.
	struct FPersonaDataAccess
	{
		bool bCrimeData = false;
		bool bFactionData = false;
		bool bMissionData = false;
		bool bPlayerInventory = false;
		bool bEconomicData = false;
	};

	struct FPersona
	{
		FName PersonaId;            // "dispatcher", "fixer", "analyst"
		FText DisplayName;          // "Dispatcher", "The Fixer", "City Analyst"
		FString AvatarPath;         // UI asset path (optional)
		FText Greeting;             // first message
		FText Personality;          // human description (debug/authoring)
		FName SpeechStyle;          // "professional", "street_slang", "analytical"
		FString SystemPromptTemplate; // may contain "{city_state_json}"
		FPersonaDataAccess Access;
		FLinearColor PrimaryColor = FLinearColor::Cyan;
		FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.15f, 0.95f);
	};
}
