#include "Flashbang.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

AFlashbang::AFlashbang()
{
	// 松手后 2s 引信倒计时，不碰触即爆
}

void AFlashbang::ExplodeDamage()
{
	if (!HasAuthority()) return;

	const FVector ExplosionOrigin = GetActorLocation();

	// 1. 球形碰撞检测：找出爆炸范围内的所有 Pawn
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetInstigator());

	GetWorld()->OverlapMultiByChannel(
		Overlaps,
		ExplosionOrigin,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(FlashRadius),
		QueryParams
	);

	// 2. 视线检测 + 白屏 RPC：被墙挡住的角色不受影响
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ABlasterCharacter* TargetChar = Cast<ABlasterCharacter>(Overlap.GetActor());
		if (!TargetChar || TargetChar->IsElimmed()) continue;

		// 视线检测：爆炸点 → 角色头部
		FHitResult Hit;
		const FVector TargetHead = TargetChar->GetActorLocation() + FVector(0.f, 0.f, 70.f);
		GetWorld()->LineTraceSingleByChannel(
			Hit,
			ExplosionOrigin,
			TargetHead,
			ECC_Visibility,
			QueryParams
		);

		// 视线被阻挡（击中非目标角色/其他几何体）→ 跳过
		if (Hit.bBlockingHit && Hit.GetActor() != TargetChar) continue;

		// 对视线内的角色客户端发送白屏 RPC
		ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(TargetChar->Controller);
		if (PC)
		{
			PC->ClientApplyFlashEffect(FlashMaxDuration);
		}
	}

	// 3. 生成一次性 Niagara 爆发特效
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ExplosionEffect,
			ExplosionOrigin,
			GetActorRotation()
		);
	}
}
