// Blaster SSR：回退管理器（纯数学射线相交判定）
// 不操作 PhysX 碰撞体——直接用 FrameHistory 中的 Capsule/Bone 快照做数学命中检测

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
	void Initialize(ABlasterGameState* InGameState);

	// 单发 HitScan 的 SSR 处理
	FSSR_TraceResult ProcessHitScanShot(
		ABlasterCharacter* Shooter,
		AHitScanWeapon* Weapon,
		const FVector& TraceStart,
		const FVector& HitTarget,
		float ClientShotServerTime);

	// 霰弹枪多弹丸的 SSR 处理（每颗弹丸独立数学判定）
	TArray<FSSR_TraceResult> ProcessShotgunPellets(
		ABlasterCharacter* Shooter,
		AShotgun* Shotgun,
		const FVector& TraceStart,
		const TArray<FVector_NetQuantize>& HitTargets,
		float ClientShotServerTime);

	// 从 SSR 命中结果造成伤害
	bool ApplySSRDamage(ABlasterCharacter* Shooter, AWeapon* Weapon, const FSSR_TraceResult& Result);

private:
	UPROPERTY()
	TWeakObjectPtr<ABlasterGameState> GameState;
};
