using UnrealBuildTool;
using System.Collections.Generic;

// Dedicated server target. Only builds with a SOURCE engine; the installed 5.8.3 launcher build cannot compile
// Server targets (DATA_SCHEMAS 13). With the launcher engine use Tools/run_server.bat, which runs the editor binary
// with -server.
public class ThistlewickValeServerTarget : TargetRules
{
	public ThistlewickValeServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ThistlewickVale");
	}
}
