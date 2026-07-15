// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Projectile.h"
#include "ProjectileRocket.generated.h"

/**
 * 火箭弹：碰到目标后产生径向爆炸伤害，自定义移动组件保证碰撞后继续飞行不停止
 */
UCLASS()
class BLASTER_API AProjectileRocket : public AProjectile
{
	GENERATED_BODY()
public:
	AProjectileRocket();
	virtual void Destroyed() override;
protected:
	virtual void BeginPlay() override;

	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit) override;

	// 飞行中的循环音效
	UPROPERTY(EditAnywhere)
	USoundCue* ProjectileLoop;

	UPROPERTY()
	UAudioComponent* ProjectileLoopComponent;

	// 音效衰减设置
	UPROPERTY(EditAnywhere)
	USoundAttenuation* LoopingSoundAttenuation;

	// 自定义移动组件（阻止默认停止行为）
	UPROPERTY(VisibleAnywhere)
	class URocketMovementComponent* RocketMovementComponent;
};
