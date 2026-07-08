// Copyright GTAI. All Rights Reserved.
//
// GTAI_Audio module build script.
// Audio stack: radio, AI music (Suno import), AI voice (ElevenLabs), ambient,
// SFX, and MetaSounds procedural audio for the GTAI open-world game.

using UnrealBuildTool;

public class GTAI_Audio : ModuleRules
{
	public GTAI_Audio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// UE audio / MetaSounds / spatialization modules.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AudioMixer",
			"SignalProcessing",
			"MetasoundEngine",        // UMetaSoundSource, MetaSound nodes
			"MetasoundFrontend",
			"MetasoundGraphCore",
			"AudioExtensions",
			"Spatialization",         // USpatializationEffect / HRTF plugins
			"SoundFieldRendering",    // Native Soundfield Ambisonics
			"Quartz",                 // UQuartzSubsystem, sample-accurate tempo
			"GameplayTags",
			"Security",               // lip-sync asset types (optional)
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GTAI_Core",
			"GTAI_NPC",               // UGTLLMManager for DJ/news/ad generation
			"GTAI_World",             // weather, TOD, district, crowd density
			"GTAI_Combat",            // weapon SFX hooks
			"GTAI_Vehicles",          // engine/vehicle SFX hooks
			"NetCore",
			"HTTP",                   // Suno/ElevenLabs REST (offline pipeline + runtime streaming)
			"Projects"
		});
	}
}
