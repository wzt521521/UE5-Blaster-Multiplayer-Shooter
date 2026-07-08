// Fill out your copyright notice in the Description page of Project Settings.


#include "RocketMovementComponent.h"

// 碰到物体后继续移动（不停止、不反弹），交由 CollisionBox OnHit 触发爆炸
URocketMovementComponent::EHandleBlockingHitResult URocketMovementComponent::HandleBlockingHit(const FHitResult& Hit, float TimeTick, const FVector& MoveDelta, float& SubTickTimeRemaining)
{
	Super::HandleBlockingHit(Hit, TimeTick, MoveDelta, SubTickTimeRemaining);
	return EHandleBlockingHitResult::AdvanceNextSubstep;
}

// 空实现，阻止默认的撞击停止行为
void URocketMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	// 火箭不在此处停止，爆炸由 CollisionBox OnHit 处理
}
