// Blaster SSR：回退管理器实现（Phase 1 骨架，核心算法在 Phase 3 填充）

#include "SSR_RewindManager.h"
#include "SSR_FrameHistory.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/WeaponSystem/Weapon/HitScanWeapon.h"
#include "Blaster/WeaponSystem/Weapon/Shotgun.h"
#include "Blaster/WeaponSystem/Weapon/Weapon.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// ════════════════════════════════════════════════════════════════
// 初始化：缓存 GameState 引用
// ════════════════════════════════════════════════════════════════

void USSR_RewindManager::Initialize(ABlasterGameState* InGameState)
{
	GameState = InGameState;
	UE_LOG(LogTemp, Log, TEXT("[SSR] RewindManager initialized"));
}

// ════════════════════════════════════════════════════════════════
// 单发 HitScan 的 SSR 处理（Phase 3 实现完整算法）
// ════════════════════════════════════════════════════════════════

FSSR_TraceResult USSR_RewindManager::ProcessHitScanShot(
	ABlasterCharacter* Shooter,
	AHitScanWeapon* Weapon,
	const FVector& TraceStart,
	const FVector& HitTarget,
	float ClientShotServerTime)
{
	FSSR_TraceResult Result;

	// ── Phase 3 占位：暂时不执行完整回退，只做基本校验 ──
	if (!Shooter || !Weapon || !GameState.IsValid()) return Result;
	if (!CVarSSREnabled.GetValueOnGameThread()) return Result;

	// TODO Phase 3: 完整实现 PerformRewindTrace
	//   1. 计算回退目标时间 OneWayDelay = ServerNow - ClientShotServerTime
	//   2. 限制 MaxPingCompensation
	//   3. 从 FrameHistory 查找历史快照
	//   4. SaveAllCharacterStates → ApplyHistorical → Trace → Validate → Restore
	//   5. 返回命中结果

	return Result;
}

// ════════════════════════════════════════════════════════════════
// 霰弹枪多弹丸 SSR 处理（Phase 3 实现）
// ════════════════════════════════════════════════════════════════

TArray<FSSR_TraceResult> USSR_RewindManager::ProcessShotgunPellets(
	ABlasterCharacter* Shooter,
	AShotgun* Shotgun,
	const FVector& TraceStart,
	const TArray<FVector_NetQuantize>& HitTargets,
	float ClientShotServerTime)
{
	TArray<FSSR_TraceResult> Results;

	// ── Phase 3 占位 ──
	if (!Shooter || !Shotgun || !GameState.IsValid()) return Results;
	if (!CVarSSREnabled.GetValueOnGameThread()) return Results;

	// TODO Phase 3: 完整实现
	//   与 ProcessHitScanShot 相同，但备份/恢复只做一次
	//   每个弹丸对同一历史帧独立做射线，结果存入数组

	return Results;
}

// ════════════════════════════════════════════════════════════════
// SSR 命中伤害应用：根据命中骨骼判定爆头/身体伤害
// ════════════════════════════════════════════════════════════════

bool USSR_RewindManager::ApplySSRDamage(
	ABlasterCharacter* Shooter,
	AWeapon* Weapon,
	const FSSR_TraceResult& Result)
{
	if (!Result.bHit || !Result.HitActor.IsValid() || !Shooter || !Weapon) return false;

	ABlasterCharacter* HitChar = Cast<ABlasterCharacter>(Result.HitActor.Get());
	if (!HitChar) return false;

	// 根据命中骨骼判定伤害类型
	const bool bHeadShot = (Result.BoneName == FName("head"));
	const float Damage = bHeadShot ? Weapon->GetHeadShotDamage() : Weapon->GetDamage();

	AController* InstigatorController = Shooter->GetController();
	UGameplayStatics::ApplyDamage(
		HitChar,
		Damage,
		InstigatorController,
		Weapon,
		UDamageType::StaticClass()
	);

	return true;
}

// ════════════════════════════════════════════════════════════════
// 核心回退算法（Phase 3 实现）
// ════════════════════════════════════════════════════════════════

FSSR_TraceResult USSR_RewindManager::PerformRewindTrace(
	ABlasterCharacter* Shooter,
	const FVector& TraceStart,
	const FVector& HitTarget,
	const FSSR_FrameSnapshot& HistoricalFrame)
{
	FSSR_TraceResult Result;
	// TODO Phase 3: 完整实现回退→射线→恢复流程
	return Result;
}

// ════════════════════════════════════════════════════════════════
// Phase 2 TODO: 批量保存/恢复碰撞体状态
// 这些方法将在 BlasterCharacter 添加 CaptureHitboxState/ApplyHitboxState 后实现
// ════════════════════════════════════════════════════════════════

void USSR_RewindManager::SaveAllCharacterStates(
	TArray<FCharacterStateBackup>& OutBackups,
	ABlasterCharacter* ExcludeShooter)
{
	// TODO Phase 2: 遍历所有 PlayerController → 获取 Pawn → 调用 CaptureHitboxState
}

void USSR_RewindManager::RestoreAllCharacterStates(const TArray<FCharacterStateBackup>& Backups)
{
	// TODO Phase 2: 遍历 Backups → 调用 ApplyHitboxState 恢复每个角色的碰撞体
}

void USSR_RewindManager::ApplyHistoricalEntry(ABlasterCharacter* Character, const FSSR_PlayerFrameEntry& Entry)
{
	// TODO Phase 2: 调用 Character->ApplyHitboxState(Entry)
}

void USSR_RewindManager::ApplyCurrentEntry(ABlasterCharacter* Character, const FSSR_PlayerFrameEntry& Entry)
{
	// TODO Phase 2: 调用 Character->ApplyHitboxState(Entry)
}
