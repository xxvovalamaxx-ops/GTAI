// GTAdvisorCore.h
// PORT of Unity AdvisorCore + AIGateway. A plain C++ controller (no UObject)
// that manages the active advisor persona, chat history, prompt assembly, and
// dispatches through the shared GTAI::NPC LLMManager (Tier 3 for deep analysis).
#pragma once

#include "CoreMinimal.h"
#include "NPC/GTNPCDefines.h"
#include "Advisor/GTAdvisorTypes.h"
#include "LLM/GTLLMManager.h"

namespace GTAI::NPC
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAdvisorMessage, const FAdvisorMessage&);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAdvisorError, const FString&);

	class GTAI_NPC_API FGTAIAdvisorCore
	{
	public:
		void Initialize(FLLMManager& InLLM, const FAdvisorCityState& InState);

		void SetPersona(EAdvisorPersona Kind);
		EAdvisorPersona GetActivePersona() const { return Active; }

		// Player asks the advisor a question.
		void SubmitQuery(const FString& UserText);

		// Call when city state changes (Collector pushes a new snapshot).
		void UpdateCityState(const FAdvisorCityState& NewState);

		FOnAdvisorMessage OnMessage;
		FOnAdvisorError OnError;

		int32 MaxChatHistory = 20;
		int32 MaxCityStateChars = 4000;   // truncate to fit context budget

	private:
		FString BuildPrompt(const FString& UserQuery) const;
		void AddMessage(const FString& Role, const FString& Content);
		void HandleLLMResult(const FString& RawResponse);
		FString ProcessCitations(const FString& Text) const; // keeps [DISTRICT:x] etc.

		FLLMManager* LLM = nullptr;
		FAdvisorCityState CityState;
		EAdvisorPersona Active = EAdvisorPersona::Dispatcher;
		FPersona ActivePersona;
		TArray<FAdvisorMessage> History;
		FString SessionId;
	};
}
