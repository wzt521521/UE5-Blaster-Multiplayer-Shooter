#pragma once

#include "CoreMinimal.h"
#include "MatchResultRecord.h"

/**
 * P4 玩家数据持久化 —— 存储抽象接口。
 *
 * 设计意图（WHY）：
 * - 把"写哪里/怎么写"与"何时入队/谁入队"解耦：异步队列 + 后台线程模型
 *   （UBlasterPersistenceSubsystem / FPersistenceWorker）只依赖本接口，
 *   换存储实现（v1 SQLite → 进阶 MySQL 连接池 + 异步驱动）只改一处构造代码。
 *
 * 模块配合（HOW）：
 * - 实现：FSQLiteMatchStatsStore（v1）；未来 FMySQLMatchStatsStore 替换。
 * - 调用线程：全部方法**只允许 worker 线程调用**（连接线程亲和），
 *   游戏线程通过入队间接触发，绝不直接触碰本接口。
 */
class IMatchStatsStore
{
public:
	virtual ~IMatchStatsStore() = default;

	// 建立连接并初始化（建表/预编译语句）。
	// SQLite：InConnectionSpec 为 .db 文件路径；MySQL：为连接字符串。
	virtual bool Open(const FString& InConnectionSpec) = 0;

	// 写入一场比赛（match 行 + 每个玩家的 player_stats 行），在 BEGIN/COMMIT 事务内完成。
	virtual bool WriteMatchResult(const FMatchResultRecord& InRecord) = 0;

	// 关闭连接、释放预编译语句。
	virtual void Close() = 0;
};
