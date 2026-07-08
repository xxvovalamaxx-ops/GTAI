// Copyright GTAI. All Rights Reserved.
//
// GTAI_Combat.Build.cs
// Build rules for the GTAI_Combat module (weapons, hit detection, damage,
// cover system). Mirrors the dependency graph declared in GTA7.uproject.

using UnrealBuildTool;

public class GTAI_Combat : ModuleRules
{
	public GTAI_Combat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",   // optional GAS interop (Lyra-style health hooks)
			"GameplayTags",
			"PhysicsCore",
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GTAI_Core",          // shared data types / interfaces
			"UMG",                // HUD callbacks
			"Niagara",            // impact / tracer VFX
			"DeveloperSettings"
		});

		// All code under namespace GTA7::Combat / GTA7::Player (see headers).
	}
}
