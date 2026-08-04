// Blaster SSR：纯数学射线相交判定（HitScan + 霰弹枪共用）
// 不操作 PhysX 碰撞体——直接用 FrameHistory 中的 Capsule/Bone 快照数据
// 做射线-胶囊体/球体数学相交测试，100% 确定性

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
#include "DrawDebugHelpers.h"

// ════════════════════════════════════════════════════════════════
// 纯数学相交辅助函数
// ════════════════════════════════════════════════════════════════

// 骨骼的近似碰撞半径（用于 head / pelvis / spine 等骨骼的射线-球体判定）
static constexpr float BONE_RADIUS = 12.f;

// 射线-球体相交：返回沿射线方向的参数 t（RayDir 必须是单位向量），无交点返回 -1
static float RaySphereIntersect(const FVector& RayOrigin, const FVector& RayDir,
	const FVector& SphereCenter, float SphereRadius)
{
	const FVector OC = RayOrigin - SphereCenter;
	const float b = 2.f * FVector::DotProduct(RayDir, OC);
	const float c = OC.SizeSquared() - SphereRadius * SphereRadius;
	const float Disc = b * b - 4.f * c;
	if (Disc < 0.f) return -1.f;

	const float t = (-b - FMath::Sqrt(Disc)) * 0.5f;
	return t > 0.f ? t : -1.f;
}

// 射线-胶囊体相交：胶囊体中心 C、半高 H（从中心到两端）、半径 R
// 将射线变换到胶囊体局部空间（胶囊体沿 Z 轴），分别检测圆柱段 + 两端半球
// 返回射线参数 t（RayDir 必须是单位向量），无交点返回 -1
static float RayCapsuleIntersect(const FVector& RayOrigin, const FVector& RayDir,
	const FVector& CapsuleCenter, const FQuat& CapsuleRot,
	float CapsuleHalfHeight, float CapsuleRadius)
{
	const FVector O = CapsuleRot.UnrotateVector(RayOrigin - CapsuleCenter);
	const FVector D = CapsuleRot.UnrotateVector(RayDir);

	const float H = CapsuleHalfHeight;
	const float R = CapsuleRadius;
	float BestT = TNumericLimits<float>::Max();
	bool bHit = false;

	const float aXY = D.X * D.X + D.Y * D.Y;

	// ── 圆柱段：|z| ≤ H-R，x² + y² = R² ──
	if (aXY > KINDA_SMALL_NUMBER)
	{
		const float b = 2.f * (O.X * D.X + O.Y * D.Y);
		const float c = O.X * O.X + O.Y * O.Y - R * R;
		const float Disc = b * b - 4.f * aXY * c;

		if (Disc >= 0.f)
		{
			const float SqrtDisc = FMath::Sqrt(Disc);
			for (float t : { (-b - SqrtDisc) / (2.f * aXY), (-b + SqrtDisc) / (2.f * aXY) })
			{
				if (t <= 0.f) continue;
				const float Z = O.Z + t * D.Z;
				if (Z >= -(H - R) && Z <= (H - R))
				{
					BestT = FMath::Min(BestT, t);
					bHit = true;
				}
			}
		}
	}
	else
	{
		const float DistXY = FMath::Sqrt(O.X * O.X + O.Y * O.Y);
		if (DistXY <= R)
		{
			const float ZEntry = FMath::Max(O.Z, -(H - R));
			if (ZEntry <= H - R)
			{
				const float t = (ZEntry - O.Z) / D.Z;
				if (t > 0.f) { BestT = FMath::Min(BestT, t); bHit = true; }
			}
		}
	}

	// ── 底端半球：中心 (0, 0, -(H-R))，半径 R ──
	{
		const float t = RaySphereIntersect(O, D, FVector(0.f, 0.f, -(H - R)), R);
		if (t > 0.f && (O.Z + t * D.Z) < -(H - R))
			{ BestT = FMath::Min(BestT, t); bHit = true; }
	}

	// ── 顶端半球：中心 (0, 0, H-R)，半径 R ──
	{
		const float t = RaySphereIntersect(O, D, FVector(0.f, 0.f, H - R), R);
		if (t > 0.f && (O.Z + t * D.Z) > H - R)
			{ BestT = FMath::Min(BestT, t); bHit = true; }
	}

	return bHit ? BestT : -1.f;
}

// ════════════════════════════════════════════════════════════════
// 单条射线的历史命中检测（HitScan 和霰弹枪共用）
// 遍历历史快照中所有玩家，取 t 最小的胶囊体/骨骼命中
// ════════════════════════════════════════════════════════════════

static FSSR_TraceResult MathTraceSingleRay(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const FSSR_FrameSnapshot& HistoricalFrame,
	ABlasterCharacter* Shooter)
{
	FSSR_TraceResult Result;
	const FVector RayDir = (TraceEnd - TraceStart).GetSafeNormal();
	const float MaxDist = (TraceEnd - TraceStart).Size();

	float BestT = TNumericLimits<float>::Max();
	ABlasterCharacter* BestChar = nullptr;
	FName BestBone = NAME_None;

	for (const FSSR_PlayerFrameEntry& Entry : HistoricalFrame.PlayerEntries)
	{
		if (!Entry.Character.IsValid()) continue;
		ABlasterCharacter* OtherChar = Entry.Character.Get();
		if (OtherChar == Shooter || OtherChar->IsElimmed()) continue;

		// 诊断
		UE_LOG(LogTemp, Warning, TEXT("[SSR]   MATH | Target=%s | HistCaps=(%.0f,%.0f,%.0f) H=%.0f R=%.0f | Bones=%d | RayStart=(%.0f,%.0f,%.0f) | RayDir=(%.3f,%.3f,%.3f)"),
			*GetNameSafe(OtherChar),
			Entry.CapsuleLocation.X, Entry.CapsuleLocation.Y, Entry.CapsuleLocation.Z,
			Entry.CapsuleHalfHeight, Entry.CapsuleRadius,
			Entry.BoneSnapshots.Num(),
			TraceStart.X, TraceStart.Y, TraceStart.Z,
			RayDir.X, RayDir.Y, RayDir.Z);

		// 1. 骨骼球体命中（用于判定爆头/肢体）
		for (const FSSR_BoneSnapshot& Bone : Entry.BoneSnapshots)
		{
			const float t = RaySphereIntersect(TraceStart, RayDir, Bone.Location, BONE_RADIUS);
			if (t > 0.f && t <= MaxDist && t < BestT)
			{
				BestT = t;
				BestChar = OtherChar;
				BestBone = Bone.BoneName;
			}
		}

		// 2. 胶囊体命中（覆盖整个身体，兜底）
		const float tCapsule = RayCapsuleIntersect(TraceStart, RayDir,
			Entry.CapsuleLocation, Entry.CapsuleRotation,
			Entry.CapsuleHalfHeight, Entry.CapsuleRadius);
		if (tCapsule > 0.f && tCapsule <= MaxDist && tCapsule < BestT)
		{
			BestT = tCapsule;
			BestChar = OtherChar;
			BestBone = NAME_None;
		}
	}

	if (BestChar)
	{
		Result.bHit = true;
		Result.ImpactPoint = TraceStart + RayDir * BestT;
		Result.BoneName = BestBone;
		Result.HitActor = BestChar;

		UE_LOG(LogTemp, Log, TEXT("[SSR] ✓ MATH HIT | Target=%s | Bone=%s | Impact=(%.0f, %.0f, %.0f) | t=%.1f"),
			*GetNameSafe(BestChar),
			BestBone.IsNone() ? TEXT("body") : *BestBone.ToString(),
			Result.ImpactPoint.X, Result.ImpactPoint.Y, Result.ImpactPoint.Z,
			BestT);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[SSR] ✗ MATH miss | Trace=(%.0f,%.0f,%.0f)->(%.0f,%.0f,%.0f) | %d entries checked"),
			TraceStart.X, TraceStart.Y, TraceStart.Z,
			TraceEnd.X, TraceEnd.Y, TraceEnd.Z,
			HistoricalFrame.PlayerEntries.Num());
	}

	return Result;
}

// ════════════════════════════════════════════════════════════════
// 初始化
// ════════════════════════════════════════════════════════════════

void USSR_RewindManager::Initialize(ABlasterGameState* InGameState)
{
	GameState = InGameState;
	UE_LOG(LogTemp, Log, TEXT("[SSR] RewindManager initialized"));
}

// ════════════════════════════════════════════════════════════════
// 时间计算 + 历史帧查找（HitScan 和霰弹枪共用）
// ════════════════════════════════════════════════════════════════

static const FSSR_FrameSnapshot* FindRewindFrame(
	UWorld* World, USSR_FrameHistory* FrameHistory,
	float ClientShotServerTime, FStringView CallerName)
{
	const double ServerNow = World->GetTimeSeconds();
	double OneWayDelay = ServerNow - ClientShotServerTime;
	OneWayDelay = FMath::Clamp(OneWayDelay, 0.0, (double)CVarSSRMaxPingCompensation.GetValueOnGameThread());

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] %.*s | ServerNow=%.3f | ClientTime=%.3f | ClampedDelay=%.1fms"),
		CallerName.Len(), CallerName.GetData(), ServerNow, ClientShotServerTime, OneWayDelay * 1000.0);

	const double RewindTargetTime = ServerNow - OneWayDelay;
	const FSSR_FrameSnapshot* Frame = FrameHistory->FindSnapshot(RewindTargetTime);

	if (!Frame)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SSR] → No historical snapshot for t=%.3f (count=%d)"),
			RewindTargetTime, FrameHistory->GetSnapshotCount());
	}

	return Frame;
}

// ════════════════════════════════════════════════════════════════
// 单发 HitScan 的 SSR 处理
// ════════════════════════════════════════════════════════════════

FSSR_TraceResult USSR_RewindManager::ProcessHitScanShot(
	ABlasterCharacter* Shooter,
	AHitScanWeapon* Weapon,
	const FVector& TraceStart,
	const FVector& HitTarget,
	float ClientShotServerTime)
{
	FSSR_TraceResult Result;
	if (!Shooter || !Weapon || !GameState.IsValid()) return Result;
	if (!CVarSSREnabled.GetValueOnGameThread()) return Result;

	UWorld* World = GetWorld();
	if (!World) return Result;

	USSR_FrameHistory* FrameHistory = GameState->GetSSRFrameHistory();
	if (!FrameHistory) return Result;

	const FSSR_FrameSnapshot* HistoricalFrame = FindRewindFrame(World, FrameHistory, ClientShotServerTime, TEXT("ProcessHitScan"));
	if (!HistoricalFrame) return Result;

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] → Rewind to frame #%d (t=%.3f), %d player entries"),
		HistoricalFrame->FrameNumber, HistoricalFrame->Timestamp, HistoricalFrame->PlayerEntries.Num());

	const FVector TraceEnd = TraceStart + (HitTarget - TraceStart) * 1.25f;
	Result = MathTraceSingleRay(TraceStart, TraceEnd, *HistoricalFrame, Shooter);

	// Debug 可视化
	if (CVarSSRDrawDebug.GetValueOnGameThread() > 0)
	{
		const FColor RayColor = Result.bHit ? FColor::Green : FColor::Red;
		DrawDebugLine(World, TraceStart, TraceEnd, RayColor, false, 2.f, 0, 1.f);
		if (Result.bHit)
			DrawDebugSphere(World, Result.ImpactPoint, 12.f, 12, FColor::Green, false, 2.f);
	}

	return Result;
}

// ════════════════════════════════════════════════════════════════
// 霰弹枪多弹丸 SSR 处理（每颗弹丸独立数学判定）
// ════════════════════════════════════════════════════════════════

TArray<FSSR_TraceResult> USSR_RewindManager::ProcessShotgunPellets(
	ABlasterCharacter* Shooter,
	AShotgun* Shotgun,
	const FVector& TraceStart,
	const TArray<FVector_NetQuantize>& HitTargets,
	float ClientShotServerTime)
{
	TArray<FSSR_TraceResult> Results;
	if (!Shooter || !Shotgun || !GameState.IsValid()) return Results;
	if (!CVarSSREnabled.GetValueOnGameThread()) return Results;

	UWorld* World = GetWorld();
	if (!World) return Results;

	USSR_FrameHistory* FrameHistory = GameState->GetSSRFrameHistory();
	if (!FrameHistory) return Results;

	const FSSR_FrameSnapshot* HistoricalFrame = FindRewindFrame(World, FrameHistory, ClientShotServerTime, TEXT("Shotgun"));
	if (!HistoricalFrame) return Results;

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] Shotgun | frame #%d | %d pellets | %d history entries"),
		HistoricalFrame->FrameNumber, HitTargets.Num(), HistoricalFrame->PlayerEntries.Num());

	int32 HitCount = 0;
	for (const FVector& HitTarget : HitTargets)
	{
		const FVector TraceEnd = TraceStart + (HitTarget - TraceStart) * 1.25f;
		FSSR_TraceResult PelletResult = MathTraceSingleRay(TraceStart, TraceEnd, *HistoricalFrame, Shooter);
		if (PelletResult.bHit) HitCount++;
		Results.Add(PelletResult);
	}

	UE_LOG(LogTemp, Log, TEXT("[SSR] Shotgun result | %d/%d pellets hit"), HitCount, HitTargets.Num());
	return Results;
}

// ════════════════════════════════════════════════════════════════
// SSR 命中伤害应用
// ════════════════════════════════════════════════════════════════

bool USSR_RewindManager::ApplySSRDamage(
	ABlasterCharacter* Shooter,
	AWeapon* Weapon,
	const FSSR_TraceResult& Result)
{
	if (!Result.bHit || !Result.HitActor.IsValid() || !Shooter || !Weapon) return false;

	ABlasterCharacter* HitChar = Cast<ABlasterCharacter>(Result.HitActor.Get());
	if (!HitChar) return false;

	const bool bHeadShot = (Result.BoneName == FName("head"));
	const float Damage = bHeadShot ? Weapon->GetHeadShotDamage() : Weapon->GetDamage();

	UE_LOG(LogTemp, Log, TEXT("[SSR] DAMAGE | Shooter=%s → Victim=%s | %s | %.0f dmg | HitPos=(%.0f, %.0f, %.0f)"),
		*GetNameSafe(Shooter),
		*GetNameSafe(HitChar),
		bHeadShot ? TEXT("HEADSHOT") : TEXT("body"),
		Damage,
		Result.ImpactPoint.X, Result.ImpactPoint.Y, Result.ImpactPoint.Z);

	AController* InstigatorController = Shooter->GetController();
	UGameplayStatics::ApplyDamage(
		HitChar, Damage, InstigatorController, Weapon, UDamageType::StaticClass());

	return true;
}
