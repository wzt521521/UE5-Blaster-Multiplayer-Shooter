#include "SQLiteMatchStatsStore.h"

// SQLiteCore 插件的公共头：暴露原生 sqlite3 C API（sqlite3.h 被打包在插件内）
#include "IncludeSQLite.h"

#include "Containers/StringConv.h"   // FTCHARToUTF8：FString(UTF-16) → UTF-8
#include "HAL/FileManager.h"         // IFileManager::MakeDirectory
#include "Misc/Paths.h"              // FPaths::GetPath

// 建表语句（v1 首次运行自动建库；表已存在则跳过）
// player_id 为客户端持久身份（GUID），是"按人归集"的键；
// player_name 仅作展示名，不唯一。
static const char* kSchemaStatements[] =
{
	"CREATE TABLE IF NOT EXISTS matches ("
	"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"  timestamp_utc TEXT NOT NULL,"
	"  map_name TEXT NOT NULL,"
	"  winner TEXT NOT NULL,"
	"  team_a_round_wins INTEGER NOT NULL DEFAULT 0,"
	"  team_b_round_wins INTEGER NOT NULL DEFAULT 0,"
	"  rounds_played INTEGER NOT NULL DEFAULT 0"
	");",

	"CREATE TABLE IF NOT EXISTS player_stats ("
	"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"  match_id INTEGER NOT NULL REFERENCES matches(id) ON DELETE CASCADE,"
	"  player_id TEXT NOT NULL DEFAULT '',"
	"  player_name TEXT NOT NULL DEFAULT '',"
	"  team_id INTEGER NOT NULL DEFAULT 0,"
	"  logical_team INTEGER NOT NULL DEFAULT 0,"
	"  kills INTEGER NOT NULL DEFAULT 0,"
	"  deaths INTEGER NOT NULL DEFAULT 0,"
	"  round_kills INTEGER NOT NULL DEFAULT 0,"
	"  money INTEGER NOT NULL DEFAULT 0"
	");",

	"CREATE INDEX IF NOT EXISTS idx_player_stats_match ON player_stats(match_id);",
	"CREATE INDEX IF NOT EXISTS idx_player_stats_pid ON player_stats(player_id);"
};

// 预编译两条 INSERT（worker 线程 Open 时 prepare，之后反复复用）
static const char* kInsertMatchSql =
	"INSERT INTO matches (timestamp_utc, map_name, winner, team_a_round_wins, team_b_round_wins, rounds_played)"
	" VALUES (?, ?, ?, ?, ?, ?);";

static const char* kInsertPlayerSql =
	"INSERT INTO player_stats (match_id, player_id, player_name, team_id, logical_team, kills, deaths, round_kills, money)"
	" VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

bool SQLiteMatchStatsStore::Open(const FString& InConnectionSpec)
{
	// ① 确保 .db 所在目录存在（sqlite 不会自动建目录）
	const FString Dir = FPaths::GetPath(InConnectionSpec);
	if (!Dir.IsEmpty())
	{
		IFileManager::Get().MakeDirectory(*Dir, true);
	}

	// ② 打开连接（不存在则自动创建文件）
	const int OpenFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
	if (sqlite3_open_v2(TCHAR_TO_UTF8(*InConnectionSpec), &Db, OpenFlags, nullptr) != SQLITE_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("[Persistence] SQLite Open FAILED: %s"), Db ? UTF8_TO_TCHAR(sqlite3_errmsg(Db)) : TEXT("?"));
		Close();
		return false;
	}

	// ③ 外键约束按连接生效（SQLite 默认关闭）
	ExecuteNonQuery("PRAGMA foreign_keys=ON;");

	// ④ 建表 + 建索引（幂等）
	if (!ExecuteSchema())
	{
		Close();
		return false;
	}

	// ⑤ 预编译两条 INSERT，后续写入只 rebind + step
	const int PrepResult = sqlite3_prepare_v2(Db, kInsertMatchSql, -1, &MatchStmt, nullptr);
	if (PrepResult != SQLITE_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("[Persistence] Prepare match stmt FAILED: %s"), UTF8_TO_TCHAR(sqlite3_errmsg(Db)));
		Close();
		return false;
	}
	if (sqlite3_prepare_v2(Db, kInsertPlayerSql, -1, &PlayerStmt, nullptr) != SQLITE_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("[Persistence] Prepare player stmt FAILED: %s"), UTF8_TO_TCHAR(sqlite3_errmsg(Db)));
		Close();
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[Persistence] SQLite store ready at %s"), *InConnectionSpec);
	return true;
}

bool SQLiteMatchStatsStore::WriteMatchResult(const FMatchResultRecord& InRecord)
{
	if (!Db || !MatchStmt || !PlayerStmt)
	{
		UE_LOG(LogTemp, Error, TEXT("[Persistence] WriteMatchResult skipped: store not open"));
		return false;
	}

	// ① 开启事务：match 行 + 全部玩家行原子落盘，任一步失败整体回滚
	if (!ExecuteNonQuery("BEGIN;"))
	{
		return false;
	}

	bool bSuccess = true;

	// ② 写入 match 行
	{
		sqlite3_reset(MatchStmt);
		sqlite3_clear_bindings(MatchStmt);
		bSuccess &= BindText(MatchStmt, 1, InRecord.TimestampUtc);
		bSuccess &= BindText(MatchStmt, 2, InRecord.MapName);
		bSuccess &= BindText(MatchStmt, 3, InRecord.WinnerTeam);
		bSuccess &= sqlite3_bind_int(MatchStmt, 4, InRecord.TeamARoundWins) == SQLITE_OK;
		bSuccess &= sqlite3_bind_int(MatchStmt, 5, InRecord.TeamBRoundWins) == SQLITE_OK;
		bSuccess &= sqlite3_bind_int(MatchStmt, 6, InRecord.RoundsPlayed) == SQLITE_OK;
		bSuccess &= sqlite3_step(MatchStmt) == SQLITE_DONE;
	}

	// ③ 用 AUTOINCREMENT 生成的 match_id 关联玩家行（同一连接内读取）
	const sqlite3_int64 MatchId = sqlite3_last_insert_rowid(Db);

	// ④ 逐玩家写入明细行
	for (const FPlayerMatchRecord& P : InRecord.Players)
	{
		sqlite3_reset(PlayerStmt);
		sqlite3_clear_bindings(PlayerStmt);
		bSuccess &= sqlite3_bind_int64(PlayerStmt, 1, MatchId) == SQLITE_OK;
		bSuccess &= BindText(PlayerStmt, 2, P.PlayerId);
		bSuccess &= BindText(PlayerStmt, 3, P.PlayerName);
		bSuccess &= sqlite3_bind_int(PlayerStmt, 4, P.TeamID) == SQLITE_OK;
		bSuccess &= sqlite3_bind_int(PlayerStmt, 5, P.LogicalTeam) == SQLITE_OK;
		bSuccess &= sqlite3_bind_int(PlayerStmt, 6, P.Kills) == SQLITE_OK;
		bSuccess &= sqlite3_bind_int(PlayerStmt, 7, P.Deaths) == SQLITE_OK;
		bSuccess &= sqlite3_bind_int(PlayerStmt, 8, P.RoundKills) == SQLITE_OK;
		bSuccess &= sqlite3_bind_int(PlayerStmt, 9, P.Money) == SQLITE_OK;
		bSuccess &= sqlite3_step(PlayerStmt) == SQLITE_DONE;

		if (!bSuccess)
		{
			break;
		}
	}

	// ⑤ 提交或回滚
	if (bSuccess)
	{
		bSuccess = ExecuteNonQuery("COMMIT;");
	}
	else
	{
		ExecuteNonQuery("ROLLBACK;");
		UE_LOG(LogTemp, Error, TEXT("[Persistence] Match write FAILED (rolled back): %s"), UTF8_TO_TCHAR(sqlite3_errmsg(Db)));
	}

	return bSuccess;
}

void SQLiteMatchStatsStore::Close()
{
	if (MatchStmt) { sqlite3_finalize(MatchStmt); MatchStmt = nullptr; }
	if (PlayerStmt) { sqlite3_finalize(PlayerStmt); PlayerStmt = nullptr; }
	if (Db) { sqlite3_close(Db); Db = nullptr; }
}

bool SQLiteMatchStatsStore::ExecuteNonQuery(const char* Sql)
{
	if (!Db) return false;
	char* ErrMsg = nullptr;
	const int Rc = sqlite3_exec(Db, Sql, nullptr, nullptr, &ErrMsg);
	if (Rc != SQLITE_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("[Persistence] Exec '%s' FAILED: %s"), UTF8_TO_TCHAR(Sql), ErrMsg ? UTF8_TO_TCHAR(ErrMsg) : TEXT("?"));
		if (ErrMsg) sqlite3_free(ErrMsg);
		return false;
	}
	return true;
}

bool SQLiteMatchStatsStore::ExecuteSchema()
{
	for (const char* Sql : kSchemaStatements)
	{
		if (!ExecuteNonQuery(Sql))
		{
			return false;
		}
	}
	return true;
}

bool SQLiteMatchStatsStore::BindText(sqlite3_stmt* Stmt, int Index, const FString& Value)
{
	// FTCHARToUTF8：FString(UTF-16) → UTF-8；SQLITE_TRANSIENT 让 sqlite 立即拷贝缓冲区
	// （转出来的临时 buffer 在表达式结束即析构，不能延迟使用）
	FTCHARToUTF8 Converted(*Value);
	return sqlite3_bind_text(Stmt, Index, Converted.Get(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}
