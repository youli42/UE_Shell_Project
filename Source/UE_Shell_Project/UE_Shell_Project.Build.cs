// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE_Shell_Project : ModuleRules
{
	public UE_Shell_Project(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 源码按场景分组放在子文件夹（Menu/Screen/Gameplay），
		// 加入头文件搜索路径后 include 仍用平铺名称（如 "ShellWorldScreen.h"）。
		// 注意：相对路径按 Source/ 解析，故用 ModuleDirectory 拼模块内绝对路径。
		PublicIncludePaths.AddRange(new string[]
		{
			ModuleDirectory + "/Menu",
			ModuleDirectory + "/Screen",
			ModuleDirectory + "/Gameplay",
		});

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Shell_UE", "GameplayAbilities", "GameplayTags", "GameplayTasks" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
