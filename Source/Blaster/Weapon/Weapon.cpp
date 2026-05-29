// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "../Character/BlasterCharacter.h"
#include "Animation/AnimationAsset.h"
#include "Net/UnrealNetwork.h"

AWeapon::AWeapon()
{
 	
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;//启用复制，让枪支能够在网络中同步

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);


	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);//所有碰撞通道都阻止，让枪支能够顶墙上或者丢地上
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);//忽略玩家碰撞通道，让枪支能够穿过玩家
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);//默认不启用碰撞，等到玩家捡起枪支后再启用碰撞

	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));//创建一个球形碰撞组件，用于检测玩家是否进入枪支的拾取范围
	AreaSphere->SetupAttachment(RootComponent);
	AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);//默认所有碰撞通道都忽略
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);//在begin play中启用碰撞，按照需求开启碰撞

	//拾取
	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(RootComponent);
}


void AWeapon::BeginPlay()
{
	Super::BeginPlay();
	if(PickupWidget)
	{
		PickupWidget->SetVisibility(false);//默认隐藏拾取提示UI
	}
	if(GetLocalRole() == ENetRole::ROLE_Authority)//只有服务器才启用碰撞，能够检测玩家进入枪支的拾取范围
	{
		
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);//启用碰撞

		AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);//对pawn设置为开启碰撞，检测重叠事件
		AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &AWeapon::OnSphereOverlap);//绑定重叠事件

		AreaSphere->OnComponentEndOverlap.AddDynamic(this, &AWeapon::OnSphereEndOverlap);//绑定结束重叠事件
	}
}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeapon, WeaponState);
}

void AWeapon::OnSphereOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult)
{
	
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);//获取目标actor
	if(BlasterCharacter)
	{
		BlasterCharacter->SetOverlappingWeapon(this);
	}
}

void AWeapon::OnSphereEndOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor, UPrimitiveComponent *OtherComp, int32 OtherBodyIndex)
{
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);//获取目标actor
	if(BlasterCharacter)
	{
		BlasterCharacter->SetOverlappingWeapon(nullptr);
	}
}

void AWeapon::OnRep_WeaponState()
{
	switch ((WeaponState))
	{
		case EWeaponState::EWS_Equipped:
		    ShowPickupWidget(false);
			//AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);//只能在服务器端禁用？
			break;
	}
}

void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeapon::SetWeaponState(EWeaponState State){
	WeaponState=State;
	switch ((WeaponState))
	{
		case EWeaponState::EWS_Equipped:
		    ShowPickupWidget(false);//服务器隐藏
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
	}


}

void AWeapon::ShowPickupWidget(bool bShowWidget)
{
	if(PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}
}

void AWeapon::Fire()
{
	if(FireAnimation)
	{
		WeaponMesh->PlayAnimation(FireAnimation, false);
	}
}
