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
			"MoviePlayer"
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
#pragma endregion K2 moonyfli
	}
}
