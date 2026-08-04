#pragma once

#include "CoreMinimal.h"

/**
 * P4 玩家数据持久化 —— 客户端本地持久身份（无账号系统）。简历点 2。
 *
 * 设计意图（WHY）：
 * - 游戏没有登录系统，PlayerName 是 OSS 给的临时名（不唯一、不跨局稳定），
 *   仅按名字记录 = 按次记录而非按人记录。这里为每台客户端生成一个 GUID 作为
 *   "持久玩家编号"：首次启动生成并存到本地文件，之后每次进游戏复用，
 *   服务器按此编号归集战绩，实现真·跨局持久化，且无需账号系统。
 *
 * 模块配合（HOW）：
 * - 生成/读取：仅客户端（BeginPlay 时 ABlasterPlayerController 调用本类）。
 * - 上报：客户端 ServerSetPlayerId RPC → 服务器写入 ABlasterPlayerState::PlayerId
 *   → 比赛结算时写进 player_stats.player_id 列。
 * - 同机多客户端测试：命令行 -BlasterPlayerId=<GUID> 强制区分（本地文件会被共享）。
 */
class FBlasterPlayerIdentity
{
public:
	// 返回当前客户端的持久 PlayerId（GUID 字符串）
	static FString GetPlayerId();

private:
	// 本地文件路径：Saved/PlayerIdentity/PlayerId.txt（按安装持久）
	static FString GetPlayerIdFile();
};
