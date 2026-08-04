#pragma once

#include "CoreMinimal.h"

/**
 * P4 玩家数据持久化 —— 纯数据结构（无 USTRUCT、无生成头）。
 *
 * 设计意图（WHY）：
 * - 后台写线程（FPersistenceWorker）不能触碰任何 UObject，只允许处理普通值类型，
 *   因此比赛统计必须在游戏线程先"快照"为这两个纯 C++ 结构再入队。
 * - 字段刻意只用 FString / int32，不引用游戏枚举（ETeamID/ELogicalTeam）——
 *   保证将来把存储层换成 MySQL store（IMatchStatsStore 的另一实现）时，本层零改动。
 *
 * 模块配合（HOW）：
 * - 生产方：ABombDefusalGameMode::BuildMatchResultRecord()（比赛结算时，游戏线程）。
 * - 消费方：UBlasterPersistenceSubsystem::EnqueueMatchResult() 入队 →
 *           FPersistenceWorker（后台线程）→ IMatchStatsStore 写入 DB。
 */

// 单个玩家在一场比赛中的战绩快照
struct FPlayerMatchRecord
{
	FString PlayerId;       // 客户端本地持久身份（GUID，见 FBlasterPlayerIdentity）——按人归集的键
	FString PlayerName;     // 展示名（OSS 给的临时名，仅显示用，不唯一）
	int32   TeamID       = 0;  // (int32)ETeamID   0=None 1=Attacker 2=Defender
	int32   LogicalTeam  = 0;  // (int32)ELogicalTeam 0=None 1=TeamA 2=TeamB
	int32   Kills        = 0;  // (int32)PS->GetScore()：每击杀 +1（BombDefusalGameMode::OnPlayerKilled）
	int32   Deaths       = 0;  // PS->GetDefeats()
	int32   RoundKills   = 0;  // PS->GetRoundKills()：最后一回合击杀快照（冗余，便于演示经济）
	int32   Money        = 0;  // PS->Money：比赛结束时余额
};

// 一场比赛的完整结算记录（比赛行 + 该场所有玩家的明细行）
struct FMatchResultRecord
{
	FString TimestampUtc;          // FDateTime::UtcNow().ToIso8601()——入队时（游戏线程）生成
	FString MapName;               // GetWorld()->GetMapName()
	FString WinnerTeam;            // TEXT("TeamA") / TEXT("TeamB") / TEXT("None")
	int32   TeamARoundWins   = 0;  // 逻辑队 A 赢下的回合数
	int32   TeamBRoundWins   = 0;  // 逻辑队 B 赢下的回合数
	int32   RoundsPlayed     = 0;  // 本场实际进行的回合数
	TArray<FPlayerMatchRecord> Players;
};
