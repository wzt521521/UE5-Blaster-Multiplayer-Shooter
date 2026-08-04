// Blaster SSR：延迟补偿（Server-Side Rewind）数据结构和结果类型

#pragma once

#include "CoreMinimal.h"
#include "SSRTypes.generated.h"

// ────────────────────────────────────────────────────────────
// 单个骨骼的世界空间快照（Location + Rotation）
// ────────────────────────────────────────────────────────────
USTRUCT()
struct FSSR_BoneSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FName BoneName;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FQuat Rotation = FQuat::Identity;
};

// ────────────────────────────────────────────────────────────
// 单个玩家在一帧内的完整碰撞体状态（胶囊体 + 关键骨骼）
// ────────────────────────────────────────────────────────────
USTRUCT()
struct FSSR_PlayerFrameEntry
{
	GENERATED_BODY()

	// 弱引用：玩家死后 Character 被销毁，WeakPtr 自动置空，防止野指针
	UPROPERTY()
	TWeakObjectPtr<class ABlasterCharacter> Character;

	// 胶囊体数据
	UPROPERTY()
	FVector CapsuleLocation = FVector::ZeroVector;

	UPROPERTY()
	FQuat CapsuleRotation = FQuat::Identity;

	UPROPERTY()
	float CapsuleHalfHeight = 0.f;

	UPROPERTY()
	float CapsuleRadius = 0.f;

	// SkeletalMeshComponent 世界 Transform
	// 恢复时直接移动 Mesh Component 来带动所有骨骼物理体（SetBodyTransform 对 kinematic articulation link 不生效）
	UPROPERTY()
	FVector MeshWorldLocation = FVector::ZeroVector;

	UPROPERTY()
	FQuat MeshWorldRotation = FQuat::Identity;

	// 关键骨骼快照（head, spine, pelvis, limbs 等 ~14 个骨骼）
	UPROPERTY()
	TArray<FSSR_BoneSnapshot> BoneSnapshots;
};

// ────────────────────────────────────────────────────────────
// 一个完整的服务器帧快照 —— 该帧所有玩家的碰撞体状态合集
// Ring buffer 中每一帧存一份此结构
// ────────────────────────────────────────────────────────────
USTRUCT()
struct FSSR_FrameSnapshot
{
	GENERATED_BODY()

	// 服务器帧时间戳（秒），用于二分查找目标帧
	UPROPERTY()
	float Timestamp = 0.f;

	// 全局递增帧号（Tick 计数器），辅助 Debug 可视化
	UPROPERTY()
	int32 FrameNumber = 0;

	// 该帧所有存活角色的碰撞体快照
	UPROPERTY()
	TArray<FSSR_PlayerFrameEntry> PlayerEntries;
};

// ────────────────────────────────────────────────────────────
// SSR 回退射线判定结果，返回给调用者用于 ApplyDamage
// ────────────────────────────────────────────────────────────
USTRUCT()
struct FSSR_TraceResult
{
	GENERATED_BODY()

	// 是否命中角色
	bool bHit = false;

	// 命中点世界坐标
	FVector ImpactPoint = FVector::ZeroVector;

	// 命中的骨骼名（如 "head"），用于爆头判定
	FName BoneName = NAME_None;

	// 被命中的 Actor（弱引用，避免生命周期问题）
	TWeakObjectPtr<AActor> HitActor = nullptr;
};
