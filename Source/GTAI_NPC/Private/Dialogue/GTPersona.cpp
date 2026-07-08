// GTPersona.cpp
// Persona data loading for GTAI::NPC.
// Implements the built-in advisor personas (Dispatcher, Fixer, City Analyst)
// and a data-driven loader that can override / extend them from a JSON file
// (mirrors the Unity AdvisorPersonaSO authoring surface).

#include "Dialogue/GTPersona.h"
#include "Advisor/GTAdvisorTypes.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace GTAI::NPC
{
	// ---------------------------------------------------------------------
	// Built-in advisor personas ("one engine, three voices")
	// ---------------------------------------------------------------------

	FPersona MakeAdvisorPersona(EAdvisorPersona Kind)
	{
		FPersona P;
		P.PrimaryColor = FLinearColor::Cyan;
		P.BackgroundColor = FLinearColor(0.1f, 0.1f, 0.15f, 0.95f);

		switch (Kind)
		{
		case EAdvisorPersona::Dispatcher:
			{
				P.PersonaId = TEXT("dispatcher");
				P.DisplayName = FText::FromString(TEXT("Dispatcher"));
				P.AvatarPath = TEXT("UI/Advisor/Dispatcher");
				P.Greeting = FText::FromString(TEXT("Dispatch, go ahead. What's your situation?"));
				P.Personality = FText::FromString(TEXT(
					"Calm, professional police dispatcher. Prioritises public safety, "
					"officer welfare and lawful resolution. Speaks in clipped radio cadence."));
				P.SpeechStyle = FName(TEXT("professional"));
				P.SystemPromptTemplate = TEXT(
					"You are the Liberty City Police Department DISPATCHER, speaking to an officer on the radio. "
					"Be calm, professional and concise. Give actionable, lawful guidance. "
					"Reference district statuses, police presence and active threats from the city state. "
					"Do NOT invent factions or districts that are not in the data. "
					"When citing a district or event, wrap it as [DISTRICT:<name>] or [EVENT:<id>].\n"
					"CITY STATE JSON:\n{city_state_json}\n");
				P.Access.bCrimeData = true;
				P.Access.bFactionData = true;
				P.Access.bMissionData = true;
				P.Access.bPlayerInventory = false;
				P.Access.bEconomicData = false;
			}
			break;

		case EAdvisorPersona::Fixer:
			{
				P.PersonaId = TEXT("fixer");
				P.DisplayName = FText::FromString(TEXT("The Fixer"));
				P.AvatarPath = TEXT("UI/Advisor/Fixer");
				P.Greeting = FText::FromString(TEXT("Hey, it's your Fixer. Need a hookup or some dirt?"));
				P.Personality = FText::FromString(TEXT(
					"Street-wise informant. Knows everyone, owes no one. Talks in slang, "
					"values reputation, cash and favours over the law."));
				P.SpeechStyle = FName(TEXT("street_slang"));
				P.SystemPromptTemplate = TEXT(
					"You are THE FIXER, a street informant in Liberty City talking to your client. "
					"Use street slang, be pragmatic and self-interested. Help the player exploit "
					"faction rivalries, score jobs and stay alive. Reference gangs, territories and "
					"available work from the city state. Never quote police procedure. "
					"When citing a faction or district, wrap it as [FACTION:<name>] or [DISTRICT:<name>].\n"
					"CITY STATE JSON:\n{city_state_json}\n");
				P.Access.bCrimeData = false;
				P.Access.bFactionData = true;
				P.Access.bMissionData = true;
				P.Access.bPlayerInventory = true;
				P.Access.bEconomicData = false;
			}
			break;

		case EAdvisorPersona::CityAnalyst:
		default:
			{
				P.PersonaId = TEXT("analyst");
				P.DisplayName = FText::FromString(TEXT("City Analyst"));
				P.AvatarPath = TEXT("UI/Advisor/Analyst");
				P.Greeting = FText::FromString(TEXT("City Analyst online. Reviewing live indicators now."));
				P.Personality = FText::FromString(TEXT(
					"Data-driven civic analyst. Neutral, quantitative, explains trends with numbers "
					"and recommends optimisations for crime, economy and traffic."));
				P.SpeechStyle = FName(TEXT("analytical"));
				P.SystemPromptTemplate = TEXT(
					"You are the CITY ANALYST, a neutral quantitative advisor for Liberty City. "
					"Be precise and evidence-based. Summarise crime index, economy index, police "
					"presence, district status and recent events from the city state. Use numbers. "
					"Suggest optimisations. Do not take sides between factions. "
					"When citing a metric or district, wrap it as [METRIC:<name>] or [DISTRICT:<name>].\n"
					"CITY STATE JSON:\n{city_state_json}\n");
				P.Access.bCrimeData = true;
				P.Access.bFactionData = true;
				P.Access.bMissionData = false;
				P.Access.bPlayerInventory = false;
				P.Access.bEconomicData = true;
			}
			break;
		}

		return P;
	}

	// ---------------------------------------------------------------------
	// Data-driven persona loading (JSON overrides / extras)
	// ---------------------------------------------------------------------

	namespace
	{
		// Parse a single persona object from a JSON value into FPersona.
		FPersona ParsePersona(const TSharedPtr<FJsonObject>& Obj)
		{
			FPersona P;
			P.PersonaId = FName(*Obj->GetStringField(TEXT("persona_id")));
			if (Obj->HasField(TEXT("display_name")))
			{
				P.DisplayName = FText::FromString(Obj->GetStringField(TEXT("display_name")));
			}
			if (Obj->HasField(TEXT("avatar_path")))
			{
				P.AvatarPath = Obj->GetStringField(TEXT("avatar_path"));
			}
			if (Obj->HasField(TEXT("greeting")))
			{
				P.Greeting = FText::FromString(Obj->GetStringField(TEXT("greeting")));
			}
			if (Obj->HasField(TEXT("personality")))
			{
				P.Personality = FText::FromString(Obj->GetStringField(TEXT("personality")));
			}
			if (Obj->HasField(TEXT("speech_style")))
			{
				P.SpeechStyle = FName(*Obj->GetStringField(TEXT("speech_style")));
			}
			if (Obj->HasField(TEXT("system_prompt")))
			{
				P.SystemPromptTemplate = Obj->GetStringField(TEXT("system_prompt"));
			}
			if (Obj->HasField(TEXT("primary_color")))
			{
				const TSharedPtr<FJsonObject>* C = nullptr;
				if (Obj->TryGetObjectField(TEXT("primary_color"), C))
				{
					P.PrimaryColor = FLinearColor(
						static_cast<float>((*C)->GetNumberField(TEXT("r"))),
						static_cast<float>((*C)->GetNumberField(TEXT("g"))),
						static_cast<float>((*C)->GetNumberField(TEXT("b"))),
						(*C)->HasField(TEXT("a")) ? static_cast<float>((*C)->GetNumberField(TEXT("a"))) : 1.f);
				}
			}
			const TSharedPtr<FJsonObject>* Access = nullptr;
			if (Obj->TryGetObjectField(TEXT("access"), Access))
			{
				(*Access)->TryGetBoolField(TEXT("crime"), P.Access.bCrimeData);
				(*Access)->TryGetBoolField(TEXT("faction"), P.Access.bFactionData);
				(*Access)->TryGetBoolField(TEXT("mission"), P.Access.bMissionData);
				(*Access)->TryGetBoolField(TEXT("inventory"), P.Access.bPlayerInventory);
				(*Access)->TryGetBoolField(TEXT("economic"), P.Access.bEconomicData);
			}
			return P;
		}
	}

	// Loads personas from a JSON file. The returned map is keyed by PersonaId.
	// Any entry whose PersonaId matches a built-in advisor persona overrides it;
	// new ids are added. Returns the number of personas loaded.
	int32 LoadPersonasFromFile(const FString& FilePath, TMap<FName, FPersona>& OutPersonas)
	{
		FString Raw;
		if (!FFileHelper::LoadFileToString(Raw, *FilePath))
		{
			UE_LOG(LogTemp, Warning, TEXT("GTPersona: failed to read persona file '%s'"), *FilePath);
			return 0;
		}

		TSharedPtr<FJsonObject> Root;
		TSharedRef<FJsonReader> Reader = FJsonReaderFactory::Create(Raw);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("GTPersona: invalid JSON in '%s'"), *FilePath);
			return 0;
		}

		int32 Count = 0;
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Root->TryGetArrayField(TEXT("personas"), Array))
		{
			for (const TSharedPtr<FJsonValue>& Val : *Array)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				if (Val->TryGetObject(Obj) && (*Obj)->HasField(TEXT("persona_id")))
				{
					const FPersona P = ParsePersona(*Obj);
					OutPersonas.Add(P.PersonaId, P);
					++Count;
				}
			}
		}

		return Count;
	}

	// Convenience: load from the default project path Content/AI/Personas/Advisor.json
	int32 LoadAdvisorPersonas(TMap<FName, FPersona>& OutPersonas)
	{
		const FString DefaultPath = FPaths::ProjectContentDir() / TEXT("AI/Personas/Advisor.json");
		return LoadPersonasFromFile(DefaultPath, OutPersonas);
	}
}
