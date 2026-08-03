// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Blaster : ModuleRules
{
	public Blaster(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		// UMG / Niagara：仅客户端目标链接（Server 无渲染管线，不需要 UI/特效模块）
		if (Target.Type != TargetType.Server)
		{
			PublicDependencyModuleNames.AddRange(new string[] { "UMG", "Niagara" });
		}

		// Dedicated Server：需要 OnlineSubsystem + Null OSS 做直连（不走 Steam 认证）
		if (Target.Type == TargetType.Server)
		{
			PublicDependencyModuleNames.AddRange(new string[] { "OnlineSubsystem", "OnlineSubsystemUtils" });
		}

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
