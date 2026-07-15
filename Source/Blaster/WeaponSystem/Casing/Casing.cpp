// Fill out your copyright notice in the Description page of Project Settings.


#include "Casing.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"
ACasing::ACasing()
{
	PrimaryActorTick.bCanEverTick = false;
	CasingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CasingMesh"));
	SetRootComponent(CasingMesh);
	CasingMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	
	
	// SetSimulatePhysics(true) — 启用物理模拟，弹壳会受力和碰撞影响而运动
	// SetEnableGravity(true) — 启用重力，弹壳弹出后会受重力影响自然下落
	// SetNotifyRigidBodyCollision(true) — 启用碰撞通知，使得 OnComponentHit 事件能被触发
	//（配合 AddDynamic 绑定），
	// 这样弹壳碰到地面或其他物体时才会播放音效并延迟销毁
	CasingMesh->SetSimulatePhysics(true);
	CasingMesh->SetEnableGravity(true);
	CasingMesh->SetNotifyRigidBodyCollision(true);
	ShellEjectionImpulse = 10.f;//默认弹壳弹出冲量
}
void ACasing::BeginPlay()
{
	Super::BeginPlay();
	CasingMesh->OnComponentHit.AddDynamic(this, &ACasing::OnHit);//绑定碰撞事件
	CasingMesh->AddImpulse(GetActorForwardVector()*ShellEjectionImpulse);//给弹壳一个向前的冲量，让它能够弹出枪口
}

void ACasing::OnHit(UPrimitiveComponent *HitComp, AActor *OtherActor, UPrimitiveComponent *OtherComp, FVector NormalImpulse, const FHitResult &Hit)
{
	if(ShellSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ShellSound, GetActorLocation());
	}
	SetLifeSpan(2.f);//当弹壳碰撞到其他物体时销毁弹壳
}

void ACasing::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

