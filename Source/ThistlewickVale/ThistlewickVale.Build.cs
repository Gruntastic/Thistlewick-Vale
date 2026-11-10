using UnrealBuildTool;

public class ThistlewickVale : ModuleRules
{
	public ThistlewickVale(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"GameplayAbilities", "GameplayTags", "GameplayTasks",
			"AIModule", "NavigationSystem", "NetCore",
			"UMG", "Slate", "SlateCore",
			"Json", "JsonUtilities", "DeveloperSettings"
		});

		// Headers are included as "Core/TVTypes.h", "Combat/...", "Systems/...", "Net/...", "UI/...".
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
