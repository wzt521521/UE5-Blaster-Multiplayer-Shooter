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

		// SQLiteCore：原生 sqlite3 C API（IncludeSQLite.h），P4 玩家数据持久化（服务端后台线程写库）。
		// 放 PrivateDependency：仅 Blaster 模块自身引用 sqlite3 符号，不对外暴露。
		// 不按 Server 加条件——客户端只是运行时不启动 worker（IsRunningDedicatedServer 门），
		// 保持将来换 MySQL store 时构建配置不变。
		PrivateDependencyModuleNames.AddRange(new string[] { "SQLiteCore" });
	}
}
