// Copyright GTAI. All Rights Reserved.
//
// GTAI_UI.Build.cs
// HUD, phone, minimap, menus. Depends on GTAI_Core (matches GTA7.uproject).

using UnrealBuildTool;

public class GTAI_UI : ModuleRules
{
	public GTAI_UI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UMG",                // widget framework
			"UMGEditor",         // editor widget preview (WITH_EDITOR-safe)
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GTAI_Core",         // shared data types / interfaces
			"Niagara"            // HUD FX (e.g. map ping)
		});
	}
}
