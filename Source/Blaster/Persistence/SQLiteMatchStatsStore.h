#pragma once

#include "CoreMinimal.h"
#include "IMatchStatsStore.h"

// 前向声明 sqlite3 句柄——头文件不依赖第三方 SQL 头，只有 .cpp include IncludeSQLite.h
struct sqlite3;
struct sqlite3_stmt;

/**
 * P4 玩家数据持久化 —— SQLite 存储实现（IMatchStatsStore 的 v1 实现）。
 *
 * 设计意图（WHY）：
 * - 由后台写线程独占一个 sqlite3 连接（连接线程亲和），预编译两条 INSERT 反复复用，
 *   每场比赛在 BEGIN/COMMIT 事务内原子写入（失败 ROLLBACK，不留半行数据）。
 * - 换 MySQL 时新建一个实现类即可，本类不删（可做离线/本地备库）。
 *
 * 模块配合（HOW）：
 * - 只被 FPersistenceWorker（worker 线程）调用：Open() 在线程 Run() 顶部执行，
 *   之后 WriteMatchResult / Close 均在本线程。游戏线程从不触碰本类。
 */
class SQLiteMatchStatsStore : public IMatchStatsStore
{
public:
	virtual bool Open(const FString& InConnectionSpec) override;
	virtual bool WriteMatchResult(const FMatchResultRecord& InRecord) override;
	virtual void Close() override;

private:
	// BEGIN / COMMIT / ROLLBACK 等无结果语句
	bool ExecuteNonQuery(const char* Sql);

	// 执行一批建表/建索引语句（仅 Open 时调用一次）
	bool ExecuteSchema();

	// 将 FString（UTF-16）转 UTF-8 后绑定到语句（sqlite 只认 UTF-8，中文名必须转）
	bool BindText(sqlite3_stmt* Stmt, int Index, const FString& Value);

	sqlite3*         Db        = nullptr;
	sqlite3_stmt*    MatchStmt = nullptr;   // 预编译：INSERT INTO matches
	sqlite3_stmt*    PlayerStmt = nullptr;  // 预编译：INSERT INTO player_stats
};
