// Blaster SSR：帧历史录制器实现

#include "SSR_FrameHistory.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

// ════════════════════════════════════════════════════════════════
// Console Variables：运行时通过控制台 ~ 调节 SSR 行为
// ════════════════════════════════════════════════════════════════

TAutoConsoleVariable<int32> CVarSSREnabled(
	TEXT("ssr.Enabled"),
	1,
	TEXT("Server-Side Rewind 延迟补偿\n0=禁用  1=启用"),
	ECVF_Default
);

TAutoConsoleVariable<float> CVarSSRMaxHistorySeconds(
	TEXT("ssr.MaxHistorySeconds"),
	0.5f,
	TEXT("历史快照保留时长（秒）\n0.5s ≈ 30帧 @60Hz，覆盖 ~250ms Ping"),
	ECVF_Default
);

TAutoConsoleVariable<float> CVarSSRMaxPingCompensation(
	TEXT("ssr.MaxPingCompensation"),
	0.25f,
	TEXT("最大 Ping 补偿上限（秒）\n超过此值的单向延迟不回退，直接走当前帧射线"),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarSSRValidateWithCurrent(
	TEXT("ssr.ValidateWithCurrent"),
	1,
	TEXT("双保险：回退帧命中或当前帧命中都算命中\n0=只用回退帧  1=回退或当前任一命中即算"),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarSSRDrawDebug(
	TEXT("ssr.DrawDebug"),
	0,
	TEXT("SSR Debug 可视化\n0=关闭  1=绘制回退射线  2=绘制射线+胶囊体线框"),
	ECVF_Default
);

// ════════════════════════════════════════════════════════════════
// 静态骨骼列表：UE5 Mannequin 标准碰撞相关骨骼
// 只追踪对命中判定有意义的骨骼，不追踪 IK 骨骼和末端效应器
// ════════════════════════════════════════════════════════════════

TArray<FName> USSR_FrameHistory::RelevantBoneNames;

void USSR_FrameHistory::BuildRelevantBoneList()
{
	if (RelevantBoneNames.Num() > 0) return; // 已构建，跳过

	// 头部 + 躯干链（爆头判定 + 身体命中判定核心）
	RelevantBoneNames.Add(FName("head"));
	RelevantBoneNames.Add(FName("neck_01"));
	RelevantBoneNames.Add(FName("spine_01"));
	RelevantBoneNames.Add(FName("spine_02"));
	RelevantBoneNames.Add(FName("spine_03"));
	RelevantBoneNames.Add(FName("pelvis"));

	// 左臂链（持枪手）
	RelevantBoneNames.Add(FName("upperarm_l"));
	RelevantBoneNames.Add(FName("lowerarm_l"));

	// 右臂链（扳机手）
	RelevantBoneNames.Add(FName("upperarm_r"));
	RelevantBoneNames.Add(FName("lowerarm_r"));

	// 左腿链
	RelevantBoneNames.Add(FName("thigh_l"));
	RelevantBoneNames.Add(FName("calf_l"));

	// 右腿链
	RelevantBoneNames.Add(FName("thigh_r"));
	RelevantBoneNames.Add(FName("calf_r"));
}

// ════════════════════════════════════════════════════════════════
// 初始化：根据 CVar + 服务器 Tick Rate 计算环形缓冲区容量
// ════════════════════════════════════════════════════════════════

void USSR_FrameHistory::Initialize(ABlasterGameState* InGameState)
{
	GameState = InGameState;

	// 从 NetServerMaxTickRate 获取服务器 Tick 频率
	// NetServerMaxTickRate=60 → 每秒 60 帧 → 0.5s 历史 = 30 帧
	float TickRate = 60.f; // 默认值
	if (UWorld* World = GetWorld())
	{
		// UNetDriver::NetServerMaxTickRate 可通过 GEngine->GetMaxTickRate() 近似获取
		TickRate = FMath::Max(20.f, World->GetNetDriver() ? 60.f : 60.f);
	}

	const float MaxHistorySeconds = CVarSSRMaxHistorySeconds.GetValueOnGameThread();
	MaxCapacity = FMath::Max(1, FMath::CeilToInt(TickRate * MaxHistorySeconds));

	// 预分配环形缓冲区内存，避免运行时动态扩容
	RingBuffer.SetNum(MaxCapacity);

	// 构建骨骼追踪列表（全局共享，只执行一次）
	BuildRelevantBoneList();

	UE_LOG(LogTemp, Log, TEXT("[SSR] FrameHistory initialized: MaxCapacity=%d frames (%.2fs @ %.0fHz) | %d bones tracked"),
		MaxCapacity, MaxHistorySeconds, TickRate, RelevantBoneNames.Num());
}

// ════════════════════════════════════════════════════════════════
// 每帧录制：遍历所有 PlayerController → 有效 Pawn → 拍快照 → 写入环形缓冲区
// 必须在所有角色移动完成后调用（GameState::Tick 末尾）
// ════════════════════════════════════════════════════════════════

void USSR_FrameHistory::RecordFrame()
{
	if (!CVarSSREnabled.GetValueOnGameThread()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 定位环形缓冲区当前写入槽位
	FSSR_FrameSnapshot& CurrentSnapshot = RingBuffer[HeadIndex];
	CurrentSnapshot.PlayerEntries.Reset();
	CurrentSnapshot.Timestamp = World->GetTimeSeconds();
	CurrentSnapshot.FrameNumber = CurrentFrameNumber;

	// 遍历所有 PlayerController 获取有效玩家角色
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		ABlasterCharacter* BlasterChar = Cast<ABlasterCharacter>(PC->GetPawn());
		// 跳过无效 Pawn / 已死亡角色 / 等待下回合的玩家
		if (!BlasterChar || BlasterChar->IsElimmed()) continue;

		FSSR_PlayerFrameEntry& Entry = CurrentSnapshot.PlayerEntries.AddDefaulted_GetRef();
		CapturePlayerEntry(BlasterChar, Entry);
	}

	// 推进环形缓冲区指针
	HeadIndex = (HeadIndex + 1) % MaxCapacity;
	FrameCounter++;
	CurrentFrameNumber++;

	// 前 5 帧 + 每 60 帧输出录制状态，用 Log 级别确保能看到
	if (CurrentFrameNumber <= 5 || CurrentFrameNumber % 60 == 0)
	{
		const int32 Count = GetSnapshotCount();
		UE_LOG(LogTemp, Log, TEXT("[SSR] FrameHistory | Frame #%d | Snapshot pool: %d/%d | %d players recorded"),
			CurrentFrameNumber, Count, MaxCapacity, CurrentSnapshot.PlayerEntries.Num());
	}
}

// ════════════════════════════════════════════════════════════════
// 单个玩家碰撞体快照：胶囊体 + 关键骨骼的世界空间 Transform
// ════════════════════════════════════════════════════════════════

void USSR_FrameHistory::CapturePlayerEntry(ABlasterCharacter* Player, FSSR_PlayerFrameEntry& OutEntry) const
{
	OutEntry.Character = Player;

	// 1. 胶囊体碰撞体
	const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
	if (Capsule)
	{
		OutEntry.CapsuleLocation   = Capsule->GetComponentLocation();
		OutEntry.CapsuleRotation   = Capsule->GetComponentQuat();
		OutEntry.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		OutEntry.CapsuleRadius     = Capsule->GetScaledCapsuleRadius();
	}

	// 2. 关键骨骼的世界空间 Transform
	OutEntry.BoneSnapshots.Reset();
	USkeletalMeshComponent* Mesh = Player->GetMesh();
	if (!Mesh) return;

	const FTransform MeshWorldTM = Mesh->GetComponentTransform();

	// 保存 Mesh Component 世界 Transform（恢复时直接移动 Component 来带动骨骼物理体）
	OutEntry.MeshWorldLocation = MeshWorldTM.GetLocation();
	OutEntry.MeshWorldRotation = MeshWorldTM.GetRotation();

	for (const FName& BoneName : RelevantBoneNames)
	{
		const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE) continue;

		// GetBoneTransform 返回 Component Space → 乘上 Mesh 的 World Transform 得世界空间
		const FTransform BoneCS = Mesh->GetBoneTransform(BoneIndex);
		const FTransform BoneWS = BoneCS * MeshWorldTM;

		FSSR_BoneSnapshot BoneSnapshot;
		BoneSnapshot.BoneName = BoneName;
		BoneSnapshot.Location = BoneWS.GetLocation();
		BoneSnapshot.Rotation = BoneWS.GetRotation();

		OutEntry.BoneSnapshots.Add(BoneSnapshot);
	}
}

// ════════════════════════════════════════════════════════════════
// 二分查找：在环形缓冲区中查找 Timestamp ≤ TargetTime 的最新快照
// 返回 nullptr 表示历史不足
// ════════════════════════════════════════════════════════════════

const FSSR_FrameSnapshot* USSR_FrameHistory::FindSnapshot(float TargetTime) const
{
	const int32 Count = GetSnapshotCount();
	if (Count == 0) return nullptr;

	// 环形缓冲区中最早帧的索引
	const int32 OldestIndex = (FrameCounter > MaxCapacity)
		? HeadIndex                          // 缓冲区已满，HeadIndex 指向的下一个就是最老的
		: 0;                                  // 缓冲区未满，索引 0 是最老的

	// 环形缓冲区按时间严格递增（HeadIndex → 最新, OldestIndex → 最老）
	// 但由于环形回绕，"物理索引 0" 不一定是最老的，需要做环形二分查找

	// 简化方案：先检查最早和最晚的边界
	const FSSR_FrameSnapshot& OldestSnap = RingBuffer[OldestIndex];
	const int32 NewestPhysIndex = (HeadIndex == 0) ? MaxCapacity - 1 : HeadIndex - 1;
	const FSSR_FrameSnapshot& NewestSnap = RingBuffer[NewestPhysIndex];

	// 目标时间比最早快照还早 → 无可用历史
	if (TargetTime < OldestSnap.Timestamp) return nullptr;

	// 目标时间比最新快照还晚 → 返回最新（极低延迟的情况）
	if (TargetTime >= NewestSnap.Timestamp) return &NewestSnap;

	// 线性查找（环形缓冲区只有 ~30 帧，没必要二分，循环遍历即可）
	// 找 Timestamp ≤ TargetTime 且 Timestamp 最大的帧
	const FSSR_FrameSnapshot* BestMatch = nullptr;
	for (int32 i = 0; i < Count; i++)
	{
		const int32 PhysIdx = (OldestIndex + i) % MaxCapacity;
		const FSSR_FrameSnapshot& Snap = RingBuffer[PhysIdx];

		if (Snap.Timestamp <= TargetTime)
		{
			BestMatch = &Snap;
		}
		else
		{
			break; // 时间递增，之后的帧都大于 TargetTime
		}
	}

	return BestMatch;
}
