// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "../Character/BlasterCharacter.h"
#include "Animation/AnimationAsset.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Casing.h"
#include "Kismet/KismetMathLibrary.h"

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
	DOREPLIFETIME(AWeapon, Ammo);
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
			WeaponMesh->SetSimulatePhysics(false);
			WeaponMesh->SetEnableGravity(false);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		case EWeaponState::EWS_Dropped:
			WeaponMesh->SetSimulatePhysics(true);
			WeaponMesh->SetEnableGravity(true);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			break;
	}
}


//子弹为武器的属性，这里通过getowner来得到角色，然后把武器属性更新到角色ui上面
void AWeapon::SpendRound()
{
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
	SetHUDAmmo();
}


void AWeapon::OnRep_Ammo()
{
	SetHUDAmmo();
}

void AWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();
	if(Owner==NULL){
		BlasterCharacterOwner = nullptr;
		BlasterControllerOwner = nullptr;
	}
	else{
		BlasterCharacterOwner = BlasterCharacterOwner == nullptr ? Cast<ABlasterCharacter>(Owner) : BlasterCharacterOwner;
		if(BlasterCharacterOwner && BlasterCharacterOwner->GetEquippedWeapon() && BlasterCharacterOwner->GetEquippedWeapon() == this)
		{
			SetHUDAmmo();
		}
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
		    ShowPickupWidget(false);
			AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WeaponMesh->SetSimulatePhysics(false);
			WeaponMesh->SetEnableGravity(false);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			break;
		case EWeaponState::EWS_Dropped:
			if(HasAuthority())//只有服务器才启用碰撞
			{
				AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			}
			WeaponMesh->SetSimulatePhysics(true);
			WeaponMesh->SetEnableGravity(true);
			WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

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

void AWeapon::Fire(const FVector& HitTarget)//此函数每个客户端独自调用
{
	if(FireAnimation)
	{
		//播放动画
		WeaponMesh->PlayAnimation(FireAnimation, false);

	}
	if(CasingClass)
	// 从武器骨骼网格体上找到 AmmoEject 插槽（抛壳口）
	// 获取抛壳口的世界位置和旋转
	// 在此位置生成一个 ACasing（弹壳）Actor
	{
		const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName("AmmoEject");
		if(AmmoEjectSocket)
		{
			FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);
			// FActorSpawnParameters SpawnParams;
			// SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			UWorld* World = GetWorld();
			if(World)
			{
				World->SpawnActor<ACasing>(CasingClass, SocketTransform.GetLocation(), SocketTransform.GetRotation().Rotator());
				//SpawnActor的第四个参数是旋转，SocketTransform.GetRotation()返回
			}
		}
	}
	SpendRound();
}

void AWeapon::SetHUDAmmo()
{
	BlasterCharacterOwner = BlasterCharacterOwner == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : BlasterCharacterOwner;//获取玩家角色
	if(BlasterCharacterOwner){
		BlasterControllerOwner = BlasterControllerOwner == nullptr ? Cast<ABlasterPlayerController>(BlasterCharacterOwner->Controller) : BlasterControllerOwner;//获取玩家控制器
		if(BlasterControllerOwner)
		{
			BlasterControllerOwner->SetHUDWeaponAmmo(Ammo);
		}
	}
}

void AWeapon::Dropped()
{
	SetWeaponState(EWeaponState::EWS_Dropped);
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);//设置分离规则
	WeaponMesh->DetachFromComponent(DetachRules);//分离武器骨骼网格体
	SetOwner(nullptr);//设置武器的拥有者为nullptr
	BlasterControllerOwner = nullptr;//设置玩家控制器为nullptr
	BlasterCharacterOwner = nullptr;//设置玩家角色为nullptr
}

void AWeapon::AddAmmo(int32 AmmoToAdd)
{
	Ammo = FMath::Clamp(Ammo - AmmoToAdd, 0, MagCapacity);
	SetHUDAmmo();
}

FVector AWeapon::TraceEndWithScatter(const FVector& HitTarget)
{
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket == nullptr) return HitTarget;

	FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
	FVector TraceStart = SocketTransform.GetLocation();

	FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
	FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;
	FVector RandVec = UKismetMathLibrary::RandomUnitVector() * SphereRadius;
	FVector EndLoc = SphereCenter + RandVec;
	FVector ToEndLoc = EndLoc - TraceStart;
	FVector Result = FVector(TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size());

	return Result;
}
