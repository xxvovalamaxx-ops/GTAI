// Copyright GTAI. All Rights Reserved.
//
// GTAI_World module build script (city streaming, traffic, economy, factions,
// wanted system). Shared vehicle enums live in GTAI_Core / GTAI_Vehicles, so
// this module intentionally does NOT depend on GTAI_Vehicles — that avoids a
// module dependency cycle (Vehicles must not depend back on World either).

using UnrealBuildTool;

public class GTAI_World : ModuleRules
{
	public GTAI_World(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",          // BehaviorTree, EQS, AI Perception, PawnSensing
			"NavigationSystem",  // route AI navmesh fallback
			"MassEntity",        // 200+ NPCs / 50+ vehicles at scale
			"MassActors",
			"MassGameplay",
			"StructUtils",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GTAI_Core"         // shared data types / interfaces
		});
	}
}
