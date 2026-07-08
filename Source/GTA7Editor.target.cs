// Copyright GTAI. All Rights Reserved.
//
// GTA7Editor.target.cs — Editor target for the GTA7 project (UE 5.8).
// Enables the editor so content/Blueprints can be created and cooked.

using UnrealBuildTool;

public class GTA7EditorTarget : TargetRules
{
	public GTA7EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		// Editor-only extra modules can be added here if needed.
		ExtraModuleNames.AddRange(new string[] { "GTA7" });

		// Build the engine from source only when required; use prebuilt by default.
		bBuildAllModules = false;

		// Faster iterative editor builds during development.
		bUseUnityBuild = true;
	}
}
