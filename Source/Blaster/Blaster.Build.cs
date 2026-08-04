// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Blaster : ModuleRules
{
	public Blaster(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// OnlineSubsystem：LobbyGameMode 的 DS 端建会话直接使用 IOnlineSubsystem，所有目标都需要
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "MultiplayerSessions", "OnlineSubsystem" });

		// UMG / Niagara：仅客户端目标链接（Server 无渲染管线，不需要 UI/特效模块）
		if (Target.Type != TargetType.Server)
		{
			PublicDependencyModuleNames.AddRange(new string[] { "UMG", "Niagara" });
		}

		// Dedicated Server：需要 OnlineSubsystemUtils（Null OSS 做直连，不走 Steam 认证）
		if (Target.Type == TargetType.Server)
		{
			PublicDependencyModuleNames.AddRange(new string[] { "OnlineSubsystemUtils" });
		}

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
