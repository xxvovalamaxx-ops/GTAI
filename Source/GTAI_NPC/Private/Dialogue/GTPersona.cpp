// GTPersona.cpp
// Persona data loading & management for the NPC/Advisor system.
// Implements:
//   * MakeAdvisorPersona()        -> the three canonical advisor voices
//   * FAdvisorCityState::ToJson()  -> city-state snapshot -> JSON (prompt context)
//   * FAdvisorCityState::FromJson()-> JSON -> city-state snapshot
//
// PORT of Unity AdvisorPersonaSO + CityStateData. The Unity side used
// ScriptableObjects; here personas are built as plain FPersona structs and the
// city state is serialised manually (these are plain C++ structs inside the
// GTAI::NPC namespace, so UObject reflection / FJsonObjectConverter cannot be
// used -- we build the JSON with FJsonObject/TJsonWriter directly, matching the
// style used elsewhere in this module, e.g. GTDeepSeekClient).

#include "Dialogue/GTPersona.h"
#include "Advisor/GTAdvisorTypes.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace GTAI::NPC
{
	// ------------------------------------------------------------------
	// Built-in advisor persona definitions
	// ------------------------------------------------------------------

	GTAI_NPC_API FPersona MakeAdvisorPersona(EAdvisorPersona Kind)
	{
		FPersona P;

		switch (Kind)
		{
		case EAdvisorPersona::Dispatcher:
			{
				P.PersonaId        = TEXT("dispatcher");
				P.DisplayName      = FText::FromString(TEXT("Dispatcher"));
				P.AvatarPath       = TEXT("UI/Advisor/Avatar_Dispatcher");
				P.Greeting         = FText::FromString(TEXT("Dispatch, go ahead. What's your situation?"));
				P.Personality      = FText::FromString(
					TEXT("A calm, professional Liberty City Police Department dispatcher. Tactical, concise, "
						 "priority-driven. Relays unit deployments and street conditions without fluff."));
				P.SpeechStyle      = TEXT("professional");
				P.SystemPromptTemplate =
					TEXT("You are the Liberty City Police Department dispatch desk. You coordinate unit "
						 "deployments and relay street conditions to the player. You have access to crime "
						 "data, police and faction activity, and the city's economic readout. Be concise, "
						 "professional, and tactical. When you reference a location or group, tag it with "
						 "[DISTRICT:name] or [FACTION:name] so the game can react. Never break character or "
						 "address the player as a user.\n\n"
						 "CITY STATE:\n{city_state_json}\n\n"
						 "Respond in character as the dispatcher.");
				P.Access.bCrimeData    = true;
				P.Access.bFactionData  = true;
				P.Access.bMissionData  = false;
				P.Access.bPlayerInventory = false;
				P.Access.bEconomicData = true;
				P.PrimaryColor     = FLinearColor(0.20f, 0.45f, 1.0f);
				P.BackgroundColor  = FLinearColor(0.06f, 0.10f, 0.22f, 0.95f);
			}
			break;

		case EAdvisorPersona::Fixer:
			{
				P.PersonaId        = TEXT("fixer");
				P.DisplayName      = FText::FromString(TEXT("The Fixer"));
				P.AvatarPath       = TEXT("UI/Advisor/Avatar_Fixer");
				P.Greeting         = FText::FromString(
					TEXT("Yo. The Fixer's listening. Whatcha need, and what's it worth to ya?"));
				P.Personality      = FText::FromString(
					TEXT("A street-level informant who knows every back-alley deal in Liberty City. "
						 "Opportunistic, loyal only to the highest bidder, always hinting at a deal."));
				P.SpeechStyle      = TEXT("street_slang");
				P.SystemPromptTemplate =
					TEXT("You are 'The Fixer', a street-level informant who knows every corner of Liberty "
						 "City. You have access to faction intel, the mission board, the player's inventory, "
						 "and crime data. Speak in street slang, be opportunistic, and always hint at a deal "
						 "or a score. When you reference a location, group, or job, tag it with "
						 "[DISTRICT:name], [FACTION:name], or [MISSION:id] so the game can react. Stay in "
						 "character; you are talking to a client, not a user.\n\n"
						 "CITY STATE:\n{city_state_json}\n\n"
						 "Respond in character as The Fixer.");
				P.Access.bCrimeData      = true;
				P.Access.bFactionData    = true;
				P.Access.bMissionData    = true;
				P.Access.bPlayerInventory = true;
				P.Access.bEconomicData   = false;
				P.PrimaryColor     = FLinearColor(1.0f, 0.60f, 0.20f);
				P.BackgroundColor  = FLinearColor(0.18f, 0.10f, 0.04f, 0.95f);
			}
			break;

		case EAdvisorPersona::CityAnalyst:
		default:
			{
				P.PersonaId        = TEXT("analyst");
				P.DisplayName      = FText::FromString(TEXT("City Analyst"));
				P.AvatarPath       = TEXT("UI/Advisor/Avatar_Analyst");
				P.Greeting         = FText::FromString(
					TEXT("City Analyst online. Current readout is ready. Which metric shall we examine?"));
				P.Personality      = FText::FromString(
					TEXT("A data-driven intelligence engine for Liberty City. Precise, quantified, "
						 "trend-aware. Reports in percentages and deltas, never in emotion."));
				P.SpeechStyle      = TEXT("analytical");
				P.SystemPromptTemplate =
					TEXT("You are the City Analyst, a data-driven intelligence engine for Liberty City. You "
						 "have access to ALL city data: crime, economy, factions, missions, and the player's "
						 "live status. Provide clear, quantified analysis with percentages and trends. When "
						 "you cite a fact, tag its source with [DISTRICT:name], [FACTION:name], or "
						 "[MISSION:id] so the game can react. Be precise and analytical; do not invent "
						 "numbers that are not implied by the city state.\n\n"
						 "CITY STATE:\n{city_state_json}\n\n"
						 "Respond in character as the City Analyst.");
				P.Access.bCrimeData      = true;
				P.Access.bFactionData    = true;
				P.Access.bMissionData    = true;
				P.Access.bPlayerInventory = true;
				P.Access.bEconomicData   = true;
				P.PrimaryColor     = FLinearColor::Cyan;
				P.BackgroundColor  = FLinearColor(0.06f, 0.14f, 0.16f, 0.95f);
			}
			break;
		}

		return P;
	}

	// ------------------------------------------------------------------
	// FAdvisorCityState JSON serialisation
	// ------------------------------------------------------------------

	namespace
	{
		TSharedPtr<FJsonObject> DistrictToJson(const FAdvisorDistrict& D)
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("name"), D.Name);
			O->SetStringField(TEXT("type"), D.Type);
			O->SetNumberField(TEXT("crime_level"), D.CrimeLevel);
			O->SetNumberField(TEXT("police_activity"), D.PoliceActivity);
			O->SetNumberField(TEXT("traffic_density"), D.TrafficDensity);
			O->SetStringField(TEXT("controlling_faction"), D.ControllingFaction);
			O->SetStringField(TEXT("status"), D.Status);
			TArray<TSharedPtr<FJsonValue>> Landmarks;
			for (const FString& L : D.Landmarks) { Landmarks.Add(MakeShared<FJsonValueString>(L)); }
			O->SetArrayField(TEXT("landmarks"), Landmarks);
			return O;
		}

		TSharedPtr<FJsonObject> FactionToJson(const FAdvisorFaction& F)
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("name"), F.Name);
			O->SetStringField(TEXT("type"), F.Type);
			O->SetNumberField(TEXT("influence"), F.Influence);
			O->SetNumberField(TEXT("player_standing"), F.PlayerStanding);
			O->SetStringField(TEXT("territory"), F.Territory);
			O->SetStringField(TEXT("current_activity"), F.CurrentActivity);
			return O;
		}

		TSharedPtr<FJsonObject> MissionToJson(const FAdvisorMission& M)
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("id"), M.Id);
			O->SetStringField(TEXT("title"), M.Title);
			O->SetStringField(TEXT("status"), M.Status);
			O->SetStringField(TEXT("giver"), M.Giver);
			O->SetStringField(TEXT("district"), M.District);
			O->SetStringField(TEXT("description"), M.Description);
			TArray<TSharedPtr<FJsonValue>> Objectives;
			for (const FString& Obj : M.Objectives) { Objectives.Add(MakeShared<FJsonValueString>(Obj)); }
			O->SetArrayField(TEXT("objectives"), Objectives);
			return O;
		}

		TSharedPtr<FJsonObject> PlayerToJson(const FAdvisorPlayer& P)
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("name"), P.Name);
			O->SetNumberField(TEXT("health"), P.Health);
			O->SetNumberField(TEXT("armor"), P.Armor);
			O->SetNumberField(TEXT("money"), P.Money);
			O->SetNumberField(TEXT("wanted_level"), P.WantedLevel);
			O->SetStringField(TEXT("current_district"), P.CurrentDistrict);
			O->SetStringField(TEXT("current_vehicle"), P.CurrentVehicle);
			TArray<TSharedPtr<FJsonValue>> Inv;
			for (const FString& Item : P.Inventory) { Inv.Add(MakeShared<FJsonValueString>(Item)); }
			O->SetArrayField(TEXT("inventory"), Inv);
			return O;
		}

		TSharedPtr<FJsonObject> EventToJson(const FAdvisorEvent& E)
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("id"), E.Id);
			O->SetStringField(TEXT("type"), E.Type);
			O->SetStringField(TEXT("district"), E.District);
			O->SetStringField(TEXT("description"), E.Description);
			O->SetStringField(TEXT("timestamp"), E.Timestamp);
			O->SetNumberField(TEXT("severity"), E.Severity);
			return O;
		}
	}

	FString FAdvisorCityState::ToJson() const
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("city_name"), CityName);
		Root->SetNumberField(TEXT("population"), Population);
		Root->SetNumberField(TEXT("crime_index"), CrimeIndex);
		Root->SetNumberField(TEXT("economy_index"), EconomyIndex);
		Root->SetNumberField(TEXT("police_presence"), PolicePresence);
		Root->SetStringField(TEXT("current_time"), CurrentTime);
		Root->SetStringField(TEXT("weather"), Weather);

		TArray<TSharedPtr<FJsonValue>> Districts;
		for (const FAdvisorDistrict& D : this->Districts) { Districts.Add(MakeShared<FJsonValueObject>(DistrictToJson(D))); }
		Root->SetArrayField(TEXT("districts"), Districts);

		TArray<TSharedPtr<FJsonValue>> Factions;
		for (const FAdvisorFaction& F : this->Factions) { Factions.Add(MakeShared<FJsonValueObject>(FactionToJson(F))); }
		Root->SetArrayField(TEXT("factions"), Factions);

		TArray<TSharedPtr<FJsonValue>> Missions;
		for (const FAdvisorMission& M : ActiveMissions) { Missions.Add(MakeShared<FJsonValueObject>(MissionToJson(M))); }
		Root->SetArrayField(TEXT("active_missions"), Missions);

		Root->SetObjectField(TEXT("player"), PlayerToJson(Player));

		TArray<TSharedPtr<FJsonValue>> Events;
		for (const FAdvisorEvent& E : RecentEvents) { Events.Add(MakeShared<FJsonValueObject>(EventToJson(E))); }
		Root->SetArrayField(TEXT("recent_events"), Events);

		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}

	FAdvisorCityState FAdvisorCityState::FromJson(const FString& Json)
	{
		FAdvisorCityState S;

		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("GTAdvisorCityState::FromJson: failed to parse city state JSON"));
			return S;
		}

		S.CityName       = Root->GetStringField(TEXT("city_name"));
		S.Population     = Root->GetIntegerField(TEXT("population"));
		S.CrimeIndex     = static_cast<float>(Root->GetNumberField(TEXT("crime_index")));
		S.EconomyIndex   = static_cast<float>(Root->GetNumberField(TEXT("economy_index")));
		S.PolicePresence = static_cast<float>(Root->GetNumberField(TEXT("police_presence")));
		S.CurrentTime    = Root->GetStringField(TEXT("current_time"));
		S.Weather        = Root->GetStringField(TEXT("weather"));

		const auto ReadStringArray = [](const TArray<TSharedPtr<FJsonValue>>& Vals)
		{
			TArray<FString> Out;
			for (const TSharedPtr<FJsonValue>& V : Vals) { Out.Add(V->AsString()); }
			return Out;
		};

		if (Root->HasField(TEXT("districts")))
		{
			for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("districts")))
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) { continue; }
				FAdvisorDistrict D;
				D.Name              = O->GetStringField(TEXT("name"));
				D.Type              = O->GetStringField(TEXT("type"));
				D.CrimeLevel        = static_cast<float>(O->GetNumberField(TEXT("crime_level")));
				D.PoliceActivity    = static_cast<float>(O->GetNumberField(TEXT("police_activity")));
				D.TrafficDensity    = static_cast<float>(O->GetNumberField(TEXT("traffic_density")));
				D.ControllingFaction= O->GetStringField(TEXT("controlling_faction"));
				D.Status            = O->GetStringField(TEXT("status"));
				if (O->HasField(TEXT("landmarks"))) { D.Landmarks = ReadStringArray(O->GetArrayField(TEXT("landmarks"))); }
				S.Districts.Add(D);
			}
		}

		if (Root->HasField(TEXT("factions")))
		{
			for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("factions")))
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) { continue; }
				FAdvisorFaction F;
				F.Name           = O->GetStringField(TEXT("name"));
				F.Type           = O->GetStringField(TEXT("type"));
				F.Influence      = static_cast<float>(O->GetNumberField(TEXT("influence")));
				F.PlayerStanding = static_cast<float>(O->GetNumberField(TEXT("player_standing")));
				F.Territory      = O->GetStringField(TEXT("territory"));
				F.CurrentActivity= O->GetStringField(TEXT("current_activity"));
				S.Factions.Add(F);
			}
		}

		if (Root->HasField(TEXT("active_missions")))
		{
			for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("active_missions")))
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) { continue; }
				FAdvisorMission M;
				M.Id          = O->GetStringField(TEXT("id"));
				M.Title       = O->GetStringField(TEXT("title"));
				M.Status      = O->GetStringField(TEXT("status"));
				M.Giver       = O->GetStringField(TEXT("giver"));
				M.District    = O->GetStringField(TEXT("district"));
				M.Description = O->GetStringField(TEXT("description"));
				if (O->HasField(TEXT("objectives"))) { M.Objectives = ReadStringArray(O->GetArrayField(TEXT("objectives"))); }
				S.ActiveMissions.Add(M);
			}
		}

		if (Root->HasField(TEXT("player")))
		{
			const TSharedPtr<FJsonObject> O = Root->GetObjectField(TEXT("player"));
			if (O.IsValid())
			{
				S.Player.Name            = O->GetStringField(TEXT("name"));
				S.Player.Health          = static_cast<float>(O->GetNumberField(TEXT("health")));
				S.Player.Armor           = static_cast<float>(O->GetNumberField(TEXT("armor")));
				S.Player.Money           = static_cast<float>(O->GetNumberField(TEXT("money")));
				S.Player.WantedLevel     = O->GetIntegerField(TEXT("wanted_level"));
				S.Player.CurrentDistrict = O->GetStringField(TEXT("current_district"));
				S.Player.CurrentVehicle  = O->GetStringField(TEXT("current_vehicle"));
				if (O->HasField(TEXT("inventory"))) { S.Player.Inventory = ReadStringArray(O->GetArrayField(TEXT("inventory"))); }
			}
		}

		if (Root->HasField(TEXT("recent_events")))
		{
			for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("recent_events")))
			{
				const TSharedPtr<FJsonObject> O = V->AsObject();
				if (!O.IsValid()) { continue; }
				FAdvisorEvent E;
				E.Id          = O->GetStringField(TEXT("id"));
				E.Type        = O->GetStringField(TEXT("type"));
				E.District    = O->GetStringField(TEXT("district"));
				E.Description = O->GetStringField(TEXT("description"));
				E.Timestamp   = O->GetStringField(TEXT("timestamp"));
				E.Severity    = static_cast<float>(O->GetNumberField(TEXT("severity")));
				S.RecentEvents.Add(E);
			}
		}

		return S;
	}
}
