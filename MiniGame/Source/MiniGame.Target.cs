using UnrealBuildTool;
using System.Collections.Generic;

public class MiniGameTarget : TargetRules
{
	public MiniGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MiniGame");
	}
}
