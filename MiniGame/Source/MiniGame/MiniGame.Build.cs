using UnrealBuildTool;

public class MiniGame : ModuleRules
{
	public MiniGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDefinitions.Add("MINIGAME_DEFER_CONSOLE_COMMANDS=0");
		PublicDefinitions.Add("MINIGAME_DEFER_AUTOMATION_TESTS=0");

#pragma region K2 moonyfli
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Niagara",
			"MoviePlayer",
			"ProceduralMeshComponent"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AssetRegistry",
				"AssetTools",
				"KismetCompiler",
				"UnrealEd",
				"UMGEditor"
			});
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("Launch");
			AdditionalPropertiesForReceipt.Add(
				"AndroidPlugin",
				System.IO.Path.Combine(ModuleDirectory, "NightAndroidHaptics_UPL.xml"));
		}
#pragma endregion K2 moonyfli
	}
}
