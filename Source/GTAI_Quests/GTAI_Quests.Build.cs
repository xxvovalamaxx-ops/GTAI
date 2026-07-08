// Copyright GTAI. All Rights Reserved.
//
// GTAI_Quests.Build.cs
// Quest framework, procedural missions, reputation.
// Depends on GTAI_Core, GTAI_NPC, GTAI_World (matches GTA7.uproject).

using UnrealBuildTool;

public class GTAI_Quests : ModuleRules
{
	public GTAI_Quests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"Json",
			"JsonUtilities",
			"HTTP"                // procedural quest generation via LLM
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GTAI_Core",         // shared data types / interfaces
			"GTAI_NPC",          // quest givers / NPC state
			"GTAI_World",        // world/faction state driving quests
			"UMG"                // quest tracker / objective UI hooks
		});
	}
}
