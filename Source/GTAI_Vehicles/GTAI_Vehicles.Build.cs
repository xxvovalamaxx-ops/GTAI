// Copyright GTAI. All Rights Reserved.
//
// GTAI_Vehicles module build script.
// Depends on the experimental ChaosVehicles plugin for UE 5.8 vehicle simulation.
// NOTE: does NOT depend on GTAI_World — shared road/traffic enums live in
// GTAI_Core, so Vehicles and World remain independent (no cycle).

using UnrealBuildTool;

public class GTAI_Vehicles : ModuleRules
{
	public GTAI_Vehicles(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",
			"ChaosVehicles",        // UChaosWheeledVehicleMovementComponent, UChaosVehicleWheel
			"ChaosVehiclesCore",
			"ChaosVehiclesEngine",
			"Chaos",
			"Niagara",
			"GameplayTags",
			"EnhancedInput",
			"ChaosVehiclesEditor"   // only when WITH_EDITOR; safe to list publicly
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GTAI_Core",
			"NetCore",
			"RenderCore",
			"Projects"
		});

		// Experimental plugin is required; ensure it is enabled in the .uproject.
	}
}
