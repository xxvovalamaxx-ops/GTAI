// Copyright GTAI. All Rights Reserved.
//
// GTAI_Core — shared data types, interfaces, and configuration used by every
// other GTAI module. Keep this module free of gameplay logic so it can be the
// root of the dependency graph (nothing should depend on it cyclically).

using UnrealBuildTool;

public class GTAI_Core : ModuleRules
{
	public GTAI_Core(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",    // shared tag vocabulary (factions, states, etc.)
			"StructUtils",     // shared fragment/struct backing for Mass
			"Json",            // shared serialization helpers
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Projects"         // plugin / feature-flag lookups
		});
	}
}
