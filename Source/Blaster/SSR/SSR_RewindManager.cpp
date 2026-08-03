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
#include "DrawDebugHelpers.h"  // DrawDebugLine / DrawDebugSphere (ssr.DrawDebug)

// ════════════════════════════════════════════════════════════════
// 初始化：缓存 GameState 引用
// ════════════════════════════════════════════════════════════════

void USSR_RewindManager::Initialize(ABlasterGameState* InGameState)
{
	GameState = InGameState;
	UE_LOG(LogTemp, Log, TEXT("[SSR] RewindManager initialized"));
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

	// 1. 计算回退目标时间
	const double ServerNow = World->GetTimeSeconds();
	double OneWayDelay = ServerNow - ClientShotServerTime;

	// 限制最大补偿范围（防作弊 + 极端 Ping 下的兜底）
	const float MaxComp = CVarSSRMaxPingCompensation.GetValueOnGameThread();
	OneWayDelay = FMath::Clamp(OneWayDelay, 0.0, (double)MaxComp);

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] ProcessHitScan | Shooter=%s | ServerNow=%.3f | ClientTime=%.3f | RawDelay=%.1fms | ClampedDelay=%.1fms"),
		*GetNameSafe(Shooter), ServerNow, ClientShotServerTime, (ServerNow - ClientShotServerTime) * 1000.0, OneWayDelay * 1000.0);

	// 延迟极小时直接做当前帧射线（听服主机 / 极低 Ping）
	if (OneWayDelay < 0.002)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SSR] → Delay too small, using current-frame trace"));
		FVector End = TraceStart + (HitTarget - TraceStart) * 1.25f;
		FHitResult CurrentHit;
		World->LineTraceSingleByChannel(CurrentHit, TraceStart, End, ECC_Visibility);
		if (ABlasterCharacter* HitChar = Cast<ABlasterCharacter>(CurrentHit.GetActor()))
		{
			Result.bHit = true;
			Result.ImpactPoint = CurrentHit.ImpactPoint;
			Result.BoneName = CurrentHit.BoneName;
			Result.HitActor = HitChar;
		}
		return Result;
	}

	const double RewindTargetTime = ServerNow - OneWayDelay;

	// 2. 查找历史快照
	const FSSR_FrameSnapshot* HistoricalFrame = FrameHistory->FindSnapshot(RewindTargetTime);
	if (!HistoricalFrame)
	{
		// 无可用历史（刚开局等）→ 不做回退
		UE_LOG(LogTemp, Verbose, TEXT("[SSR] → No historical snapshot available for t=%.3f (history count=%d)"),
			RewindTargetTime, FrameHistory->GetSnapshotCount());
		return Result;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] → Rewind to frame #%d (t=%.3f), snapshot has %d player entries"),
		HistoricalFrame->FrameNumber, HistoricalFrame->Timestamp, HistoricalFrame->PlayerEntries.Num());

	// 3. 执行回退射线
	return PerformRewindTrace(Shooter, TraceStart, HitTarget, *HistoricalFrame);
}

// ════════════════════════════════════════════════════════════════
// 霰弹枪多弹丸 SSR 处理
// 关键：备份/恢复只做一次（所有弹丸共用同一个历史帧）
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

	// 1. 计算回退目标时间（与 HitScan 相同逻辑）
	const double ServerNow = World->GetTimeSeconds();
	double OneWayDelay = ServerNow - ClientShotServerTime;
	const float MaxComp = CVarSSRMaxPingCompensation.GetValueOnGameThread();
	OneWayDelay = FMath::Clamp(OneWayDelay, 0.0, (double)MaxComp);

	// 延迟极小：直接当前帧射线（不走回退）
	if (OneWayDelay < 0.002)
	{
		for (const FVector& HitTarget : HitTargets)
		{
			FSSR_TraceResult PelletResult;
			FVector End = TraceStart + (HitTarget - TraceStart) * 1.25f;
			FHitResult CurrentHit;
			World->LineTraceSingleByChannel(CurrentHit, TraceStart, End, ECC_Visibility);
			if (ABlasterCharacter* HitChar = Cast<ABlasterCharacter>(CurrentHit.GetActor()))
			{
				PelletResult.bHit = true;
				PelletResult.ImpactPoint = CurrentHit.ImpactPoint;
				PelletResult.BoneName = CurrentHit.BoneName;
				PelletResult.HitActor = HitChar;
			}
			Results.Add(PelletResult);
		}
		return Results;
	}

	const double RewindTargetTime = ServerNow - OneWayDelay;

	// 2. 查找历史快照
	const FSSR_FrameSnapshot* HistoricalFrame = FrameHistory->FindSnapshot(RewindTargetTime);
	if (!HistoricalFrame) return Results;

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] Shotgun | Rewind to frame #%d | %d pellets | %d history entries | Delay=%.1fms"),
		HistoricalFrame->FrameNumber, HitTargets.Num(), HistoricalFrame->PlayerEntries.Num(), OneWayDelay * 1000.0);

	// 3. 备份所有其他角色当前状态（一次性）
	TArray<FCharacterStateBackup> Backups;
	SaveAllCharacterStates(Backups, Shooter);

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] Shotgun | Backed up %d characters"), Backups.Num());

	// RAII：无论函数怎么退出，碰撞体一定会恢复
	ON_SCOPE_EXIT
	{
		RestoreAllCharacterStates(Backups);
		UE_LOG(LogTemp, Verbose, TEXT("[SSR] Shotgun | Restored %d characters"), Backups.Num());
	};

	// 4. 回退到历史帧：遍历快照中每个角色 → 应用历史碰撞体
	int32 ShotgunRewound = 0;
	for (const FSSR_PlayerFrameEntry& Entry : HistoricalFrame->PlayerEntries)
	{
		if (!Entry.Character.IsValid()) continue;
		ABlasterCharacter* OtherChar = Entry.Character.Get();
		if (OtherChar == Shooter || OtherChar->IsElimmed()) continue;
		ApplyHistoricalEntry(OtherChar, Entry);
		ShotgunRewound++;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] Shotgun | Rewound %d targets to historical frame"), ShotgunRewound);

	// 5. 对每个弹丸独立做射线（共享同一个回退后的世界状态）
	const bool bValidateCurrent = CVarSSRValidateWithCurrent.GetValueOnGameThread() != 0;
	for (const FVector& HitTarget : HitTargets)
	{
		const FVector TraceEnd = TraceStart + (HitTarget - TraceStart) * 1.25f;

		FHitResult RewindHit;
		World->LineTraceSingleByChannel(RewindHit, TraceStart, TraceEnd, ECC_Visibility);

		FSSR_TraceResult PelletResult;
		if (ABlasterCharacter* HitChar = Cast<ABlasterCharacter>(RewindHit.GetActor()))
		{
			PelletResult.bHit = true;
			PelletResult.ImpactPoint = RewindHit.ImpactPoint;
			PelletResult.BoneName = RewindHit.BoneName;
			PelletResult.HitActor = HitChar;
		}

		// 双保险：如果回退未命中，检查当前帧（在恢复之后进行）
		if (!PelletResult.bHit && bValidateCurrent)
		{
			// 临时恢复 → 在当前帧做射线 → 再回退回去（后续弹丸还需要回退状态）
			RestoreAllCharacterStates(Backups);
			FHitResult CurrentHit;
			World->LineTraceSingleByChannel(CurrentHit, TraceStart, TraceEnd, ECC_Visibility);
			if (ABlasterCharacter* HitChar = Cast<ABlasterCharacter>(CurrentHit.GetActor()))
			{
				PelletResult.bHit = true;
				PelletResult.ImpactPoint = CurrentHit.ImpactPoint;
				PelletResult.BoneName = CurrentHit.BoneName;
				PelletResult.HitActor = HitChar;
			}
			// 重新回退（为下一个弹丸恢复历史碰撞体状态）
			for (const FSSR_PlayerFrameEntry& Entry : HistoricalFrame->PlayerEntries)
			{
				if (!Entry.Character.IsValid()) continue;
				ABlasterCharacter* OtherChar = Entry.Character.Get();
				if (OtherChar == Shooter || OtherChar->IsElimmed()) continue;
				ApplyHistoricalEntry(OtherChar, Entry);
			}
		}

		Results.Add(PelletResult);
	}

	// ON_SCOPE_EXIT 自动恢复碰撞体
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

	UE_LOG(LogTemp, Log, TEXT("[SSR] DAMAGE | Shooter=%s → Victim=%s | %s | %.0f dmg | HitPos=(%.0f, %.0f, %.0f)"),
		*GetNameSafe(Shooter),
		*GetNameSafe(HitChar),
		bHeadShot ? TEXT("HEADSHOT") : TEXT("body"),
		Damage,
		Result.ImpactPoint.X, Result.ImpactPoint.Y, Result.ImpactPoint.Z);

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
// 核心回退算法：保存当前 → 回退到历史 → 执行射线 → 双保险 → 恢复
// ════════════════════════════════════════════════════════════════

FSSR_TraceResult USSR_RewindManager::PerformRewindTrace(
	ABlasterCharacter* Shooter,
	const FVector& TraceStart,
	const FVector& HitTarget,
	const FSSR_FrameSnapshot& HistoricalFrame)
{
	FSSR_TraceResult Result;
	UWorld* World = GetWorld();
	if (!World) return Result;

	// 1. 备份所有其他角色的当前碰撞体状态
	TArray<FCharacterStateBackup> Backups;
	SaveAllCharacterStates(Backups, Shooter);

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] → Backed up %d characters (current state)"), Backups.Num());

	// RAII：无论函数如何退出（return / 异常），碰撞体一定会恢复
	ON_SCOPE_EXIT
	{
		RestoreAllCharacterStates(Backups);
		UE_LOG(LogTemp, Verbose, TEXT("[SSR] → Restored %d characters to current state"), Backups.Num());
	};

	// 2. 回退到历史帧：遍历历史快照中的每个玩家，应用历史碰撞体
	int32 RewoundCount = 0;
	for (const FSSR_PlayerFrameEntry& Entry : HistoricalFrame.PlayerEntries)
	{
		if (!Entry.Character.IsValid()) continue;
		ABlasterCharacter* OtherChar = Entry.Character.Get();
		// 跳过射击者自己（只回退目标角色）和已死亡角色
		if (OtherChar == Shooter || OtherChar->IsElimmed()) continue;
		ApplyHistoricalEntry(OtherChar, Entry);
		RewoundCount++;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[SSR] → Applied historical hitbox to %d targets"), RewoundCount);

	// 3. 执行回退射线（从枪口到客户端瞄准点）
	//    此时所有目标角色的碰撞体已在历史位置，射线判定的是"客户端看到的世界"
	const FVector TraceEnd = TraceStart + (HitTarget - TraceStart) * 1.25f;
	FHitResult RewindHit;
	World->LineTraceSingleByChannel(RewindHit, TraceStart, TraceEnd, ECC_Visibility);

	bool bRewindHit = false;
	if (ABlasterCharacter* HitChar = Cast<ABlasterCharacter>(RewindHit.GetActor()))
	{
		bRewindHit = true;
		Result.bHit = true;
		Result.ImpactPoint = RewindHit.ImpactPoint;
		Result.BoneName = RewindHit.BoneName;
		Result.HitActor = HitChar;

		UE_LOG(LogTemp, Log, TEXT("[SSR] ✓ REWIND HIT | Target=%s | Bone=%s | Impact=(%.0f, %.0f, %.0f)"),
			*GetNameSafe(HitChar), *Result.BoneName.ToString(),
			Result.ImpactPoint.X, Result.ImpactPoint.Y, Result.ImpactPoint.Z);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SSR] ✗ Rewind miss"));
	}

	// 4. 双保险：当前帧判定（仅当回退未命中时）
	const bool bValidateCurrent = CVarSSRValidateWithCurrent.GetValueOnGameThread() != 0;
	if (!bRewindHit && bValidateCurrent)
	{
		// 恢复碰撞体 → 在当前帧做射线
		RestoreAllCharacterStates(Backups);

		FHitResult CurrentHit;
		World->LineTraceSingleByChannel(CurrentHit, TraceStart, TraceEnd, ECC_Visibility);

		if (ABlasterCharacter* HitChar = Cast<ABlasterCharacter>(CurrentHit.GetActor()))
		{
			Result.bHit = true;
			Result.ImpactPoint = CurrentHit.ImpactPoint;
			Result.BoneName = CurrentHit.BoneName;
			Result.HitActor = HitChar;

			UE_LOG(LogTemp, Log, TEXT("[SSR] ✓ CURRENT HIT (fallback) | Target=%s | Bone=%s | Impact=(%.0f, %.0f, %.0f)"),
				*GetNameSafe(HitChar), *Result.BoneName.ToString(),
				Result.ImpactPoint.X, Result.ImpactPoint.Y, Result.ImpactPoint.Z);
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("[SSR] ✗ Current-frame also miss → shot completely missed"));
		}

		// ON_SCOPE_EXIT 会再次调用 RestoreAllCharacterStates，但此时已经恢复了，无害
	}

	// 5. Debug 可视化（ssr.DrawDebug 1 绘制回退射线）
	if (CVarSSRDrawDebug.GetValueOnGameThread() > 0)
	{
		const FColor RayColor = Result.bHit ? FColor::Green : FColor::Red;
		DrawDebugLine(World, TraceStart, TraceEnd, RayColor, false, 2.f, 0, 1.f);
		if (Result.bHit)
		{
			DrawDebugSphere(World, Result.ImpactPoint, 12.f, 12, FColor::Green, false, 2.f);
		}
	}

	return Result;
}

// ════════════════════════════════════════════════════════════════
// Phase 2 实现：批量保存/恢复碰撞体状态
// ════════════════════════════════════════════════════════════════

void USSR_RewindManager::SaveAllCharacterStates(
	TArray<FCharacterStateBackup>& OutBackups,
	ABlasterCharacter* ExcludeShooter)
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		ABlasterCharacter* OtherChar = Cast<ABlasterCharacter>(PC->GetPawn());
		if (!OtherChar || OtherChar == ExcludeShooter || OtherChar->IsElimmed()) continue;

		FCharacterStateBackup& Backup = OutBackups.AddDefaulted_GetRef();
		Backup.Character = OtherChar;
		OtherChar->CaptureHitboxState(Backup.SavedEntry);
	}
}

void USSR_RewindManager::RestoreAllCharacterStates(const TArray<FCharacterStateBackup>& Backups)
{
	for (const FCharacterStateBackup& Backup : Backups)
	{
		if (ABlasterCharacter* Char = Backup.Character.Get())
		{
			Char->ApplyHitboxState(Backup.SavedEntry);
		}
	}
}

void USSR_RewindManager::ApplyHistoricalEntry(ABlasterCharacter* Character, const FSSR_PlayerFrameEntry& Entry)
{
	if (Character) Character->ApplyHitboxState(Entry);
}

void USSR_RewindManager::ApplyCurrentEntry(ABlasterCharacter* Character, const FSSR_PlayerFrameEntry& Entry)
{
	if (Character) Character->ApplyHitboxState(Entry);
}
