using UnrealBuildTool;
using System.Collections.Generic;

public class GameServerTarget : TargetRules
{
	public GameServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("Game");
	}
}