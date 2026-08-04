#include "PersistenceWorker.h"

#include "IMatchStatsStore.h"
#include "HAL/Event.h"            // FEvent
#include "HAL/PlatformProcess.h"  // FPlatformProcess
#include "HAL/PlatformTLS.h"      // FPlatformTLS::GetCurrentThreadId

FPersistenceWorker::FPersistenceWorker(IMatchStatsStore* InStore, const FString& InConnectionSpec,
                                       TQueue<FMatchResultRecord, EQueueMode::Mpsc>* InQueue,
                                       TAtomic<bool>* InStopFlag, FEvent* InWakeEvent)
	: Store(InStore)
	, ConnectionSpec(InConnectionSpec)
	, Queue(InQueue)
	, StopFlag(InStopFlag)
	, WakeEvent(InWakeEvent)
{
}

bool FPersistenceWorker::Init()
{
	return true;
}

uint32 FPersistenceWorker::Run()
{
	// ── 线程顶部独占建立连接（线程亲和）──
	// 连接在 Run() 里创建、之后所有读写都在本线程、退出前由本线程 Close()，
	// 保证 sqlite 连接不会被两个线程交叉使用。
	if (!Store->Open(ConnectionSpec))
	{
		UE_LOG(LogTemp, Error, TEXT("[Persistence] Worker aborted: store open failed"));
		return 1;
	}

	// ── 主循环：先取后等（Trigger 不丢）──
	// 先 Dequeue 再 Wait：若等待期间有 Trigger，醒来后先处理完队列里的记录再睡；
	// Wait(100) 超时兼作 StopFlag 轮询，保证停止请求最迟 100ms 内被响应。
	while (!StopFlag->Load())
	{
		FMatchResultRecord Item;
		if (Queue->Dequeue(Item))
		{
			WriteAndLog(Item);
			continue;
		}
		WakeEvent->Wait(100);
	}

	// ── 优雅停机：排空剩余队列，不丢已入队的比赛记录 ──
	FMatchResultRecord Item;
	while (Queue->Dequeue(Item))
	{
		WriteAndLog(Item);
	}

	Store->Close();
	UE_LOG(LogTemp, Log, TEXT("[Persistence] Worker exiting"));
	return 0;
}

void FPersistenceWorker::WriteAndLog(const FMatchResultRecord& Record)
{
	const bool bOk = Store->WriteMatchResult(Record);
	if (bOk)
	{
		// 本日志的线程 ID 应 ≠ 入队日志的线程 ID → 证明写库发生在后台线程
		UE_LOG(LogTemp, Log, TEXT("[Persistence] Match WRITTEN | players=%d | winner=%s | thread=%u"),
			Record.Players.Num(), *Record.WinnerTeam, FPlatformTLS::GetCurrentThreadId());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Persistence] Match WRITE FAILED | thread=%u"),
			FPlatformTLS::GetCurrentThreadId());
	}
}

void FPersistenceWorker::Stop()
{
	// 本实现用 StopFlag 原子标志控制退出（子系统 Deinitialize 置位），无需额外动作
}

void FPersistenceWorker::Exit()
{
}
