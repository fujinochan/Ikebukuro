// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IkebukuroNinjaCity : ModuleRules
{
	public IkebukuroNinjaCity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CesiumRuntime",
			"Json",
			"CoreUObject",
			"Engine",
			"MediaAssets",
			"AudioMixer",
			"RHI",
			"RenderCore",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AssetRegistry", "MeshDescription", "StaticMeshDescription", "ImageCore" });
		}

		PublicIncludePaths.AddRange(new string[] {
			"IkebukuroNinjaCity",
			"IkebukuroNinjaCity/Variant_Platforming",
			"IkebukuroNinjaCity/Variant_Platforming/Animation",
			"IkebukuroNinjaCity/Variant_Combat",
			"IkebukuroNinjaCity/Variant_Combat/AI",
			"IkebukuroNinjaCity/Variant_Combat/Animation",
			"IkebukuroNinjaCity/Variant_Combat/Gameplay",
			"IkebukuroNinjaCity/Variant_Combat/Interfaces",
			"IkebukuroNinjaCity/Variant_Combat/UI",
			"IkebukuroNinjaCity/Variant_SideScrolling",
			"IkebukuroNinjaCity/Variant_SideScrolling/AI",
			"IkebukuroNinjaCity/Variant_SideScrolling/Gameplay",
			"IkebukuroNinjaCity/Variant_SideScrolling/Interfaces",
			"IkebukuroNinjaCity/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
