// Copyright GTAI. All Rights Reserved.
//
// GTA7.target.cs — Game (packaged) target for the GTA7 project (UE 5.8).
// Produces a standalone Windows build (no editor).

using UnrealBuildTool;

public class GTA7Target : TargetRules
{
	public GTA7Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		ExtraModuleNames.AddRange(new string[] { "GTA7" });

		// Shipping/Test/Development all build from this target.
		// bUseUnityBuild is enabled by default for faster full builds.
		bBuildAllModules = false;

		// Keep the runtime binary lean — exclude editor-only code.
		bBuildWithEditorOnlyData = false;
	}
}
