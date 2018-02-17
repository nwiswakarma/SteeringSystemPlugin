/*
 SteeringSystemPlugin 0.0.1
 -----
 
*/
using UnrealBuildTool;

public class SteeringSystemPlugin: ModuleRules
{
	public SteeringSystemPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );

		PrivateDependencyModuleNames.AddRange(
            new string[] {
                "RVOPlugin"
            }
        );
	}
}
