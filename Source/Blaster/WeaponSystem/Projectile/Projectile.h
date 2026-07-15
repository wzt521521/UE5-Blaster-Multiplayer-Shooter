// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Projectile.generated.h"

UCLASS()
class BLASTER_API AProjectile : public AActor
{
	GENERATED_BODY()

public:
	AProjectile();
	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;

protected:
	virtual void BeginPlay() override;

	// 径向伤害爆炸（火箭/榴弹使用，仅服务器执行）
	// 虚函数：子类可重写以实现不同类型爆炸（碎片/闪光/烟雾）
	virtual void ExplodeDamage();

	// 延迟销毁：碰到目标后不立刻 Destroy，而是等 DestroyTime 秒后再销毁
	void StartDestroyTimer();
	void DestroyTimerFinished();

	// 生成尾迹粒子（火箭飞行拖尾）
	void SpawnTrailSystem();

	// 碰撞回调（服务器+客户端均可绑定，子类重写实现不同行为）
	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	// ===== 组件 =====
	UPROPERTY(EditAnywhere)
	class UBoxComponent* CollisionBox;

	UPROPERTY(VisibleAnywhere)
	class UProjectileMovementComponent* ProjectileMovementComponent;

	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* ProjectileMesh;

	// ===== 粒子/音效 =====
	UPROPERTY(EditAnywhere)
	class UParticleSystem* ImpactParticles;

	UPROPERTY(EditAnywhere)
	class USoundCue* ImpactSound;

	UPROPERTY(EditAnywhere)
	class UParticleSystem* TrailSystem;

	UPROPERTY()
	class UParticleSystemComponent* TrailSystemComponent;

	// ===== 伤害 =====
	UPROPERTY(EditAnywhere)
	float Damage = 20.f;

	UPROPERTY(EditAnywhere)
	float DamageInnerRadius = 200.f;

	UPROPERTY(EditAnywhere)
	float DamageOuterRadius = 500.f;

	// 引信时间 + 定时器句柄（protected：子类 ThrowableProjectile 需自定义引信时长）
	UPROPERTY(EditAnywhere)
	float DestroyTime = 3.f;

	FTimerHandle DestroyTimer;

private:
	UPROPERTY(EditAnywhere)
	UParticleSystem* Tracer;

	UPROPERTY()
	class UParticleSystemComponent* TracerComponent;
};
