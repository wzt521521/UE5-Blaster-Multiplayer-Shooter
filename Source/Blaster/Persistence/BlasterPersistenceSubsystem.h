#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"
#include "Templates/Atomic.h"
#include "MatchResultRecord.h"
#include "BlasterPersistenceSubsystem.generated.h"

class IMatchStatsStore;
class FPersistenceWorker;
class FEvent;
class FSubsystemCollectionBase;

/**
 * P4 玩家数据持久化 —— 引擎级门面（UEngineSubsystem）。
 *
 * 设计意图（WHY）：
 * - 选 UEngineSubsystem 而非 WorldSubsystem：持久化 worker 必须跨 ServerTravel 存活
 *   （比赛结束 → 返回大厅的 ServerTravel 会销毁世界，但 worker 仍在后台写库）。
 * - 只在 Dedicated Server 进程启动：客户端不运行 GameMode（没有结算点），
 *   也避免在客户端机器建出无意义的 .db。
 * - 存储实现（IMatchStatsStore）由本子系统构造——将来换 MySQL 只改这一行。
 *
 * 模块配合（HOW）：
 * - 生产者：ABombDefusalGameMode::ConcludeMatch() 比赛结算时调用 EnqueueMatchResult()。
 * - 消费者：FPersistenceWorker（后台线程）排空队列 → IMatchStatsStore 写库。
 * - 停止：Deinitialize()（引擎退出）置 StopFlag → Trigger → 有界 join → 释放 store。
 */
UCLASS()
class BLASTER_API UBlasterPersistenceSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 比赛结束调用（游戏线程）：快照数据入队 + 唤醒 worker，立即返回、绝不等待。
	void EnqueueMatchResult(const FMatchResultRecord& Record);

	// 服务端控制台命令 BlasterDumpStats 的实现：开独立只读连接打印全部记录。
	void DumpStats() const;

	static UBlasterPersistenceSubsystem* Get();

private:
	bool bWorkerRunning = false;

	FPersistenceWorker*  Worker      = nullptr;   // FRunnable（不拥有，仅持有指针）
	FRunnableThread*     WorkerThread = nullptr;  // 后台线程句柄
	IMatchStatsStore*    Store       = nullptr;   // 具体存储：new SQLiteMatchStatsStore（换 MySQL 改这行）

	// 无锁 MPSC 队列：游戏线程单生产、worker 单消费；Trigger/Wait 配 FEvent 唤醒
	TQueue<FMatchResultRecord, EQueueMode::Mpsc> Queue;
	TAtomic<bool>        StopFlag;   // 停止请求（worker 轮询）
	FEvent*              WakeEvent = nullptr;     // 唤醒信号（auto-reset）

	FString DbPath;
};
