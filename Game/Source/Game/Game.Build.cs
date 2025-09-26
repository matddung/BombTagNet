using UnrealBuildTool;

public class Game : ModuleRules
{
	public Game(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "NetCore",
            "Networking",
            "EnhancedInput"
        });

        if (Target.Type != TargetType.Server)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UMG",
                "Slate",
                "SlateCore"
            });

            PublicDefinitions.Add("BT_CLIENT=1");
        }
        else
        {
            PublicDefinitions.Add("BT_SERVER=1");
        }

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}