using UnrealBuildTool;
using System.Collections.Generic;

public class MiniGameEditorTarget : TargetRules
{
	public MiniGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MiniGame");
	}
}
