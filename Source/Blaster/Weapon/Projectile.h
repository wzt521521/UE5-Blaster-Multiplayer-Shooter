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

	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, 
		FVector NormalImpulse, const FHitResult& Hit);
		//HitComp 是你自己的碰撞体，OtherActor/OtherComp 是被你撞到的目标，NormalImpulse 是碰撞冲击力，Hit 是所有细节的合集。
private:
	UPROPERTY(EditAnywhere)
	class UBoxComponent* CollisionBox;

	UPROPERTY(VisibleAnywhere)
	class UProjectileMovementComponent* ProjectileMovementComponent;

	UPROPERTY(EditAnywhere)
	class UParticleSystem* Tracer;

	class UParticleSystemComponent* TracerComponent;
	
	UPROPERTY(EditAnywhere)
	UParticleSystem* ImpactParticles;//子弹命中时播放的粒子特效

	UPROPERTY(EditAnywhere)
	class USoundCue* ImpactSound;//子弹命中时播放的音效
protected:
	UPROPERTY(EditAnywhere)
	float Damage = 20.f;
public:	



};
