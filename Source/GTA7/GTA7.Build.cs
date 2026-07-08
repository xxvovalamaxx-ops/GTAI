// Copyright GTAI. All Rights Reserved.
//
// GTA7 — primary game module (game mode, game state, player).
// Depends on every feature module so game-level code can wire systems together.

using UnrealBuildTool;

public class GTA7 : ModuleRules
{
	public GTA7(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"PhysicsCore",
			"NetCore",
			"WorldPartition",
			"NavigationSystem",
			"AIModule"
		});

		// Feature modules are exposed publicly so game classes (GameMode,
		// PlayerController, HUD) can reference them from headers.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"GTAI_Core",
			"GTAI_NPC",
			"GTAI_World",
			"GTAI_Vehicles",
			"GTAI_Combat",
			"GTAI_AI",
			"GTAI_UI",
			"GTAI_Audio",
			"GTAI_Quests"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UMG",        // HUD / menu widgets
			"Slate",
			"SlateCore",
			"Niagara"     // ambient gameplay VFX
		});
	}
}
