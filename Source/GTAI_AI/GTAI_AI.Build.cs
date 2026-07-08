// Copyright GTAI. All Rights Reserved.
//
// GTAI_AI.Build.cs
// LLM integration, quest scaffolding helpers, police tactics.
// Depends on GTAI_Core, GTAI_NPC, GTAI_World (matches GTA7.uproject).

using UnrealBuildTool;

public class GTAI_AI : ModuleRules
{
	public GTAI_AI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",           // BehaviorTree / EQS / AI Perception (police tactics)
			"GameplayTags",
			"Json",
			"JsonUtilities",
			"HTTP"                // DeepSeek / cloud LLM REST calls
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GTAI_Core",         // shared data types / interfaces
			"GTAI_NPC",          // NPC dialogue + memory types
			"GTAI_World",        // world/faction state for AI reactivity
			"UMG"                // AI-driven UI prompts
		});
	}
}
