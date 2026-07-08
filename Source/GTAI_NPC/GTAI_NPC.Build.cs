// Copyright GTAI. All Rights Reserved.
//
// GTAI_NPC.Build.cs
// Build rules for the GTAI::NPC C++ module (LLM dialogue, memory, schedules,
// pedestrians). Mirrors the dependency graph declared in GTA7.uproject.

using UnrealBuildTool;

public class GTAI_NPC : ModuleRules
{
	public GTAI_NPC(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = false;

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Public")
			});

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "Private")
			});

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"MassEntity",        // ECS for 100+ pedestrians
				"MassActors",
				"MassGameplay",      // Mass Avoidance / steering
				"StructUtils",       // fragment/struct backing
				"Json",
				"JsonUtilities",
				"HTTP",              // DeepSeek / cloud LLM REST calls
				"Projects"           // plugin/feature flags
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Slate",
				"SlateCore",
				"GTAI_Core"          // shared data types / interfaces
			});

		// On-device inference (llama.cpp / ACE LLM) is linked conditionally.
		// Add "GTAI_ONDEVICE_LLM=1" to the target's GlobalDefinitions to enable.
		if (Target.GlobalDefinitions.Contains("GTAI_ONDEVICE_LLM=1"))
		{
			PublicDefinitions.Add("GTAI_ONDEVICE_LLM_ENABLED=1");
			// Third-party llama.cpp include/lib paths provided by the build env.
		}
	}
}
