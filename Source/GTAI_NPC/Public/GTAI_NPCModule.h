// GTAI_NPCModule.h
// Module entry point for the GTAI::NPC C++ module.
// Core systems are plain C++ classes inside namespace GTAI::NPC (UE reflection
// cannot place a UCLASS inside a namespace). The module exposes factory helpers.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

namespace GTAI::NPC
{
	class FLLMManager;
	class FDeepSeekClient;
	class FOnDeviceLLM;
	class FGTAIAdvisorCore;

	class GTAI_NPCMODULE_API IGTAI_NPCModule : public IModuleInterface
	{
	public:
		static inline IGTAI_NPCModule& Get()
		{
			return FModuleManager::LoadModuleChecked<IGTAI_NPCModule>("GTAI_NPC");
		}

		virtual TSharedRef<FLLMManager> CreateLLMManager() = 0;
		virtual TSharedRef<FDeepSeekClient> CreateDeepSeekClient() = 0;
		virtual TSharedRef<FOnDeviceLLM> CreateOnDeviceLLM() = 0;
		virtual TSharedRef<FGTAIAdvisorCore> CreateAdvisorCore() = 0;
	};
}

class GTAI_NPCMODULE_API FGTAI_NPCModule : public GTAI::NPC::IGTAI_NPCModule
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual TSharedRef<GTAI::NPC::FLLMManager> CreateLLMManager() override;
	virtual TSharedRef<GTAI::NPC::FDeepSeekClient> CreateDeepSeekClient() override;
	virtual TSharedRef<GTAI::NPC::FOnDeviceLLM> CreateOnDeviceLLM() override;
	virtual TSharedRef<GTAI::NPC::FGTAIAdvisorCore> CreateAdvisorCore() override;
};
