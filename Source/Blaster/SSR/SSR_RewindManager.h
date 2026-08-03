// Blaster SSR：回退管理器
// 托管于 BlasterGameState。收到 ServerFire RPC 时，
// 负责"保存当前碰撞体 → 回退到历史帧 → 执行 SSR 射线 → 双保险判定 → 恢复碰撞体"

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SSRTypes.h"
#include "SSR_RewindManager.generated.h"

class ABlasterGameState;
class ABlasterCharacter;
class AHitScanWeapon;
class AShotgun;
class AWeapon;

UCLASS()
class BLASTER_API USSR_RewindManager : public UObject
{
	GENERATED_BODY()

public:
	// ── 初始化：缓存 GameState 引用 ──
	void Initialize(ABlasterGameState* InGameState);

	// ── 单发 HitScan 的 SSR 处理 ──
	// Shooter: 射击者, Weapon: 使用的 HitScan 武器
	// TraceStart: 枪口 muzzle 世界位置, HitTarget: 客户端瞄准点
	// ClientShotServerTime: 客户端开枪时估计的服务器时间
	// 返回命中结果（bHit=true 表示命中角色，调用 ApplySSRDamage 造成伤害）
	FSSR_TraceResult ProcessHitScanShot(
		ABlasterCharacter* Shooter,
		AHitScanWeapon* Weapon,
		const FVector& TraceStart,
		const FVector& HitTarget,
		float ClientShotServerTime);

	// ── 霰弹枪多弹丸的 SSR 处理 ──
	// 每颗弹丸独立判定，备份/恢复只做一次（所有弹丸共用同一历史帧）
	TArray<FSSR_TraceResult> ProcessShotgunPellets(
		ABlasterCharacter* Shooter,
		AShotgun* Shotgun,
		const FVector& TraceStart,
		const TArray<FVector_NetQuantize>& HitTargets,
		float ClientShotServerTime);

	// ── 从 SSR 命中结果造成伤害 ──
	// 根据命中骨骼判定爆头/身体伤害，调用 UGameplayStatics::ApplyDamage
	bool ApplySSRDamage(ABlasterCharacter* Shooter, AWeapon* Weapon, const FSSR_TraceResult& Result);

private:
	// 缓存的 GameState 引用
	UPROPERTY()
	TWeakObjectPtr<ABlasterGameState> GameState;

	// ── 核心回退算法 ──
	FSSR_TraceResult PerformRewindTrace(
		ABlasterCharacter* Shooter,
		const FVector& TraceStart,
		const FVector& HitTarget,
		const FSSR_FrameSnapshot& HistoricalFrame);

	// ── 碰撞体状态备份结构 ──
	struct FCharacterStateBackup
	{
		TWeakObjectPtr<ABlasterCharacter> Character;
		FSSR_PlayerFrameEntry SavedEntry; // 该角色当前帧的真实状态
	};

	// ── 批量保存/恢复 ──
	// 遍历所有非射击者、非死亡玩家，保存当前碰撞体到 Backups
	void SaveAllCharacterStates(TArray<FCharacterStateBackup>& OutBackups, ABlasterCharacter* ExcludeShooter);
	// 将 Backups 中的状态写回各自角色（恢复碰撞体）
	void RestoreAllCharacterStates(const TArray<FCharacterStateBackup>& Backups);

	// ── 单角色操作 ──
	// 将角色的碰撞体设置为指定历史帧的状态
	void ApplyHistoricalEntry(ABlasterCharacter* Character, const FSSR_PlayerFrameEntry& Entry);
	// 将角色的碰撞体恢复到当前状态（从 Backup）
	void ApplyCurrentEntry(ABlasterCharacter* Character, const FSSR_PlayerFrameEntry& Entry);
};
