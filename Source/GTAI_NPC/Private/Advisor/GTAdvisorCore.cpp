// GTAdvisorCore.cpp
// PORT of Unity AdvisorCore + AIGateway. Plain C++ controller (no UObject)
// that manages the active advisor persona, chat history, prompt assembly and
// dispatches through the shared GTAI::NPC LLMManager (Tier 3 for deep analysis).

#include "Advisor/GTAdvisorCore.h"
#include "Advisor/GTAdvisorTypes.h"
#include "Dialogue/GTPersona.h"
#include "LLM/GTLLMManager.h"
#include "LLM/GTLLMTypes.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"

namespace GTAI::NPC
{
	// =====================================================================
	// FAdvisorCityState JSON (declared in GTAdvisorTypes.h, defined here)
	// =====================================================================

	namespace
	{
		TSharedPtr<FJsonObject> DistrictToJson(const FAdvisorDistrict& D)
		{
			TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
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
			TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
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
			TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("id"), M.Id);
			O->SetStringField(TEXT("title"), M.Title);
			O->SetStringField(TEXT("status"), M.Status);
			O->SetStringField(TEXT("giver"), M.Giver);
			O->SetStringField(TEXT("district"), M.District);
			O->SetStringField(TEXT("description"), M.Description);
			TArray<TSharedPtr<FJsonValue>> Objs;
			for (const FString& Obj : M.Objectives) { Objs.Add(MakeShared<FJsonValueString>(Obj)); }
			O->SetArrayField(TEXT("objectives"), Objs);
			return O;
		}

		TSharedPtr<FJsonObject> EventToJson(const FAdvisorEvent& E)
		{
			TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("id"), E.Id);
			O->SetStringField(TEXT("type"), E.Type);
			O->SetStringField(TEXT("district"), E.District);
			O->SetStringField(TEXT("description"), E.Description);
			O->SetStringField(TEXT("timestamp"), E.Timestamp);
			O->SetNumberField(TEXT("severity"), E.Severity);
			return O;
		}

		TSharedPtr<FJsonObject> PlayerToJson(const FAdvisorPlayer& P)
		{
			TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("name"), P.Name);
			O->SetNumberField(TEXT("health"), P.Health);
			O->SetNumberField(TEXT("armor"), P.Armor);
			O->SetNumberField(TEXT("money"), P.Money);
			O->SetNumberField(TEXT("wanted_level"), P.WantedLevel);
			O->SetStringField(TEXT("current_district"), P.CurrentDistrict);
			O->SetStringField(TEXT("current_vehicle"), P.CurrentVehicle);
			TArray<TSharedPtr<FJsonValue>> Inv;
			for (const FString& I : P.Inventory) { Inv.Add(MakeShared<FJsonValueString>(I)); }
			O->SetArrayField(TEXT("inventory"), Inv);
			return O;
		}
	}

	FString FAdvisorCityState::ToJson() const
	{
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("city_name"), CityName);
		Root->SetNumberField(TEXT("population"), Population);
		Root->SetNumberField(TEXT("crime_index"), CrimeIndex);
		Root->SetNumberField(TEXT("economy_index"), EconomyIndex);
		Root->SetNumberField(TEXT("police_presence"), PolicePresence);
		Root->SetStringField(TEXT("current_time"), CurrentTime);
		Root->SetStringField(TEXT("weather"), Weather);

		TArray<TSharedPtr<FJsonValue>> Districts;
		for (const FAdvisorDistrict& D : Districts) { Districts.Add(MakeShared<FJsonValueObject>(DistrictToJson(D))); }
		Root->SetArrayField(TEXT("districts"), Districts);

		TArray<TSharedPtr<FJsonValue>> Factions;
		for (const FAdvisorFaction& F : Factions) { Factions.Add(MakeShared<FJsonValueObject>(FactionToJson(F))); }
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
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return Out;
	}

	namespace
	{
		void JsonToDistrict(const TSharedPtr<FJsonObject>& O, FAdvisorDistrict& D)
		{
			if (!O.IsValid()) { return; }
			D.Name = O->GetStringField(TEXT("name"));
			D.Type = O->GetStringField(TEXT("type"));
			O->TryGetNumberField(TEXT("crime_level"), D.CrimeLevel);
			O->TryGetNumberField(TEXT("police_activity"), D.PoliceActivity);
			O->TryGetNumberField(TEXT("traffic_density"), D.TrafficDensity);
			D.ControllingFaction = O->GetStringField(TEXT("controlling_faction"));
			D.Status = O->GetStringField(TEXT("status"));
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (O->TryGetArrayField(TEXT("landmarks"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr) { D.Landmarks.Add(V->AsString()); }
			}
		}

		void JsonToFaction(const TSharedPtr<FJsonObject>& O, FAdvisorFaction& F)
		{
			if (!O.IsValid()) { return; }
			F.Name = O->GetStringField(TEXT("name"));
			F.Type = O->GetStringField(TEXT("type"));
			O->TryGetNumberField(TEXT("influence"), F.Influence);
			O->TryGetNumberField(TEXT("player_standing"), F.PlayerStanding);
			F.Territory = O->GetStringField(TEXT("territory"));
			F.CurrentActivity = O->GetStringField(TEXT("current_activity"));
		}

		void JsonToMission(const TSharedPtr<FJsonObject>& O, FAdvisorMission& M)
		{
			if (!O.IsValid()) { return; }
			M.Id = O->GetStringField(TEXT("id"));
			M.Title = O->GetStringField(TEXT("title"));
			M.Status = O->GetStringField(TEXT("status"));
			M.Giver = O->GetStringField(TEXT("giver"));
			M.District = O->GetStringField(TEXT("district"));
			M.Description = O->GetStringField(TEXT("description"));
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (O->TryGetArrayField(TEXT("objectives"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr) { M.Objectives.Add(V->AsString()); }
			}
		}

		void JsonToEvent(const TSharedPtr<FJsonObject>& O, FAdvisorEvent& E)
		{
			if (!O.IsValid()) { return; }
			E.Id = O->GetStringField(TEXT("id"));
			E.Type = O->GetStringField(TEXT("type"));
			E.District = O->GetStringField(TEXT("district"));
			E.Description = O->GetStringField(TEXT("description"));
			E.Timestamp = O->GetStringField(TEXT("timestamp"));
			O->TryGetNumberField(TEXT("severity"), E.Severity);
		}

		void JsonToPlayer(const TSharedPtr<FJsonObject>& O, FAdvisorPlayer& P)
		{
			if (!O.IsValid()) { return; }
			P.Name = O->GetStringField(TEXT("name"));
			O->TryGetNumberField(TEXT("health"), P.Health);
			O->TryGetNumberField(TEXT("armor"), P.Armor);
			O->TryGetNumberField(TEXT("money"), P.Money);
			O->TryGetNumberField(TEXT("wanted_level"), P.WantedLevel);
			P.CurrentDistrict = O->GetStringField(TEXT("current_district"));
			P.CurrentVehicle = O->GetStringField(TEXT("current_vehicle"));
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (O->TryGetArrayField(TEXT("inventory"), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : *Arr) { P.Inventory.Add(V->AsString()); }
			}
		}
	}

	FAdvisorCityState FAdvisorCityState::FromJson(const FString& Json)
	{
		FAdvisorCityState State;

		TSharedPtr<FJsonObject> Root;
		TSharedRef<FJsonReader> Reader = FJsonReaderFactory::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("GTAdvisorCore: failed to parse city state JSON"));
			return State;
		}

		State.CityName = Root->GetStringField(TEXT("city_name"));
		Root->TryGetNumberField(TEXT("population"), State.Population);
		Root->TryGetNumberField(TEXT("crime_index"), State.CrimeIndex);
		Root->TryGetNumberField(TEXT("economy_index"), State.EconomyIndex);
		Root->TryGetNumberField(TEXT("police_presence"), State.PolicePresence);
		State.CurrentTime = Root->GetStringField(TEXT("current_time"));
		State.Weather = Root->GetStringField(TEXT("weather"));

		const TArray<TSharedPtr<FJsonValue>>* Districts = nullptr;
		if (Root->TryGetArrayField(TEXT("districts"), Districts))
		{
			for (const TSharedPtr<FJsonValue>& V : *Districts)
			{
				FAdvisorDistrict D;
				JsonToDistrict(V->AsObject(), D);
				State.Districts.Add(D);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Factions = nullptr;
		if (Root->TryGetArrayField(TEXT("factions"), Factions))
		{
			for (const TSharedPtr<FJsonValue>& V : *Factions)
			{
				FAdvisorFaction F;
				JsonToFaction(V->AsObject(), F);
				State.Factions.Add(F);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Missions = nullptr;
		if (Root->TryGetArrayField(TEXT("active_missions"), Missions))
		{
			for (const TSharedPtr<FJsonValue>& V : *Missions)
			{
				FAdvisorMission M;
				JsonToMission(V->AsObject(), M);
				State.ActiveMissions.Add(M);
			}
		}

		const TSharedPtr<FJsonObject>* Player = nullptr;
		if (Root->TryGetObjectField(TEXT("player"), Player))
		{
			JsonToPlayer(*Player, State.Player);
		}

		const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
		if (Root->TryGetArrayField(TEXT("recent_events"), Events))
		{
			for (const TSharedPtr<FJsonValue>& V : *Events)
			{
				FAdvisorEvent E;
				JsonToEvent(V->AsObject(), E);
				State.RecentEvents.Add(E);
			}
		}

		return State;
	}

	// =====================================================================
	// FGTAIAdvisorCore
	// =====================================================================

	void FGTAIAdvisorCore::Initialize(FLLMManager& InLLM, const FAdvisorCityState& InState)
	{
		LLM = &InLLM;
		CityState = InState;
		Active = EAdvisorPersona::Dispatcher;
		ActivePersona = MakeAdvisorPersona(Active);
		History.Empty();
		SessionId = FGuid::NewGuid().ToString();

		// Seed the greeting as the first advisor turn.
		AddMessage(TEXT("advisor"), ActivePersona.Greeting.ToString());
	}

	void FGTAIAdvisorCore::SetPersona(EAdvisorPersona Kind)
	{
		if (Active == Kind) { return; }
		Active = Kind;
		ActivePersona = MakeAdvisorPersona(Kind);
		// Re-seed with the new persona's greeting so the voice switch is explicit.
		AddMessage(TEXT("advisor"), ActivePersona.Greeting.ToString());
	}

	void FGTAIAdvisorCore::SubmitQuery(const FString& UserText)
	{
		if (!LLM)
		{
			OnError.Broadcast(TEXT("Advisor not initialized (no LLM manager)."));
			return;
		}

		AddMessage(TEXT("user"), UserText);

		FLLMRequest Req;
		Req.NPC = 0; // advisor is not a physical NPC in the crowd
		Req.PersonaId = ActivePersona.PersonaId;
		Req.Prompt = BuildPrompt(UserText);
		Req.MaxTokens = 640;
		Req.Temperature = (Active == EAdvisorPersona::Fixer) ? 0.9f
			: (Active == EAdvisorPersona::CityAnalyst) ? 0.3f
			: 0.4f;
		Req.bAllowCloud = true; // Tier 3 deep analysis
		Req.CacheBucket = FString::Printf(TEXT("advisor:%s"), *ActivePersona.PersonaId.ToString());

		// Capture the user text for the result closure.
		const FString UserTurn = UserText;
		LLM->Generate(Req, FOnLLMResult::CreateLambda(
			[this, UserTurn](const FLLMRequest&, const FLLMResult& Res)
			{
				if (Res.bSuccess)
				{
					HandleLLMResult(!Res.RawText.IsEmpty() ? Res.RawText : Res.Response.Line);
				}
				else
				{
					OnError.Broadcast(Res.Error.IsEmpty() ? TEXT("Advisor LLM failed.") : Res.Error);
				}
				(void)UserTurn;
			}));
	}

	void FGTAIAdvisorCore::UpdateCityState(const FAdvisorCityState& NewState)
	{
		CityState = NewState;
	}

	FString FGTAIAdvisorCore::BuildPrompt(const FString& UserQuery) const
	{
		// 1. System / persona voice with the city-state snapshot injected.
		FString CityJson = CityState.ToJson();
		if (CityJson.Len() > MaxCityStateChars)
		{
			CityJson = CityJson.Left(MaxCityStateChars) + TEXT("…");
		}

		FString System = ActivePersona.SystemPromptTemplate.Replace(
			TEXT("{city_state_json}"), *CityJson, ESearchCase::CaseIgnored);

		FString P = System;
		P += TEXT("\n");

		// 2. Recent conversation (working memory).
		if (History.Num() > 0)
		{
			P += TEXT("CONVERSATION:\n");
			for (const FAdvisorMessage& M : History)
			{
				const FString RoleLabel = (M.Role == TEXT("user")) ? TEXT("PLAYER") : TEXT("ADVISOR");
				P += FString::Printf(TEXT("%s: %s\n"), *RoleLabel, *M.Content);
			}
		}

		// 3. Current player turn.
		P += FString::Printf(TEXT("PLAYER: %s\n"), *UserQuery);
		P += TEXT("Respond in character. Keep it focused and useful.\n");

		return P;
	}

	void FGTAIAdvisorCore::AddMessage(const FString& Role, const FString& Content)
	{
		FAdvisorMessage M;
		M.Role = Role;
		M.Content = Content;
		M.Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%SZ"));
		M.PersonaId = ActivePersona.PersonaId;
		History.Add(M);

		while (History.Num() > MaxChatHistory)
		{
			History.RemoveAt(0);
		}
	}

	void FGTAIAdvisorCore::HandleLLMResult(const FString& RawResponse)
	{
		const FString Clean = ProcessCitations(RawResponse);
		AddMessage(TEXT("advisor"), Clean);

		FAdvisorMessage Out;
		Out.Role = TEXT("advisor");
		Out.Content = Clean;
		Out.Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y-%m-%dT%H:%M:%SZ"));
		Out.PersonaId = ActivePersona.PersonaId;
		OnMessage.Broadcast(Out);
	}

	FString FGTAIAdvisorCore::ProcessCitations(const FString& Text) const
	{
		// The LLM is instructed to emit bracketed citations like
		// [DISTRICT:x], [FACTION:x], [EVENT:x], [METRIC:x]. We keep those and
		// strip stray markdown code fences so the UI can render plain text.
		FString Out = Text;
		Out.ReplaceInline(TEXT("```json"), TEXT(""), ESearchCase::CaseIgnored);
		Out.ReplaceInline(TEXT("```"), TEXT(""), ESearchCase::CaseIgnored);
		Out.TrimStartAndEndInline();
		return Out;
	}
}
