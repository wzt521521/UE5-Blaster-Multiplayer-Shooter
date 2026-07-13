// Fill out your copyright notice in the Description page of Project Settings.
//UCombatComponent 它的设计意图是：

// 它将来要管什么
// "Combat" = 战斗系统，这个组件负责角色身上所有战斗相关的状态和逻辑，比如：

// 当前装备的武器、武器切换
// 弹药数量、换弹
// 开火、命中检测
// 生命值 / 护盾
// 为什么要被复制
// 战斗数据需要在所有客户端之间同步，因为其他玩家也需要看到：

// 数据	为什么需要复制
// 当前装备的武器	其他玩家需要看到你手里拿什么枪
// 弹药/换弹动作	其他玩家需要看到你换弹动画
// 开火状态	其他玩家需要看到枪口火焰、听到枪声
// 生命值	其他玩家需要看到血条变化
// 如果不复制，这些状态就只存在于服务器，其他客户端根本看不到你在做什么。

// 为什么要独立成 Component
// 而不是直接写在 ABlasterCharacter 里？因为战斗逻辑复杂且独立，拆成 Component 可以：

// 保持角色类不会过于臃肿
// 战斗逻辑可以复用（比如 AI 角色也能挂载同一个 Component）
// 便于单独开启/关闭复制，不用整个 Actor 所有属性都参与同步
// 


#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blaster/HUD/BlasterHud.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "Blaster/BlasterTypes/CombatState.h"
#include "CombatComponent.generated.h"

class AWeapon;
class ABlasterCharacter;
class ABlasterPlayerController;
class ABlasterHud;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	UCombatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;


	friend class ABlasterCharacter;

	void EquipWeapon(AWeapon* WeaponToEquip);
	void Reload();
	UFUNCTION(BlueprintCallable)
	void FinishReloading();
	void ReloadEmptyWeapon();
	void PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount);

protected:
	
	virtual void BeginPlay() override;
	void SetAiming(bool bIsAiming);

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bIsAiming);
	UFUNCTION(Server, Reliable)
	void ServerReload();
	void HandReload();
	int32 AmountToReload();
	UFUNCTION()
	void OnRep_EquippedWeapon();

	void PlayEquipWeaponSound(AWeapon* WeaponToEquip);
	void FireButtonPressed(bool bPressed);
	void Fire();
	void FireProjectileWeapon();
	void FireHitScanWeapon();
	void FireShotgun();
	void LocalFire(const FVector_NetQuantize& TraceHitTarget);
	void ShotgunLocalFire(const TArray<FVector_NetQuantize>& TraceHitTargets);
	void StartFireTimer();
	void FireTimerFinished();
	bool CanFire();

	//当前武器携带的子弹
	UPROPERTY(ReplicatedUsing = OnRep_CarriedAmmo)
	int32 CarriedAmmo;

	UFUNCTION()
	void OnRep_CarriedAmmo();

	void InitializeCarriedAmmo();

	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	ECombatState CombatState=ECombatState::ECS_Unoccupied;

	UFUNCTION()
	void OnRep_CombatState();

	UPROPERTY(EditAnywhere)
	int32 StartingARAmmo=30;

	UPROPERTY(EditAnywhere)
	int32 StartingSMGAmmo=30;

	UPROPERTY(EditAnywhere)
	int32 StartingRocketAmmo=6;

	UPROPERTY(EditAnywhere)
	int32 StartingPistolAmmo=30;

	UPROPERTY(EditAnywhere)
	int32 StartingSniperAmmo=10;

	UPROPERTY(EditAnywhere)
	int32 StartingShotgunAmmo=10;

	UPROPERTY(EditAnywhere)
	int32 StartingGrenadeLauncherAmmo=0;

	//仓库
	TMap<EWeaponType, int32> CarriedAmmoMap;

	UFUNCTION(Server, Reliable)
	void ServerFire(const FVector_NetQuantize& TraceHitTarget, float FireDelay);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& TraceHitTarget);

	UFUNCTION(Server, Reliable)
	void ServerShotgunFire(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastShotgunFire(const TArray<FVector_NetQuantize>& TraceHitTargets);

	void TraceUnderCrosshairs(FHitResult& TraceHitResult);

	void SetHUDCrosshairs(float DeltaTime);

private:
	ABlasterCharacter* Character;//指向拥有这个组件的角色的指针

	ABlasterPlayerController* Controller;
	ABlasterHud* HUD;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
	AWeapon* EquippedWeapon;//存储当前装备武器的变量

	UPROPERTY(Replicated)//会被复制
	bool bAiming;

	UPROPERTY(EditAnywhere)
	float BaseWalkSpeed;

	UPROPERTY(EditAnywhere)
	float AimWalkSpeed;

	bool bFireButtonPressed;



	FVector HitTarget;

	// 构建准星数据包
	FHUDPackage HUDPackage;

	FTimerHandle FireTimer;
	bool bCanFire = true;
	bool bLocallyReloading = false;

	UPROPERTY(EditAnywhere)
	int32 MaxCarriedAmmo = 500;

	void UpdateCarriedAmmo();

	// 根据角色移动速度计算的散布因子（0=静止，1=全速奔跑）
	float CrosshairVelocityFactor;
	float CrosshairInAirFactor;
	float CrosshairAimFactor;
	float CrosshairShootingFactor;
	//不瞄准时的基准视野，BeginPlay 从相机读取，失镜插值恢复到此值
	float DefaultFOV;

	// 收镜速度，InterpFOV 从不瞄准状态恢复到 DefaultFOV 的插值速度
	UPROPERTY(EditAnywhere)
	float ZoomInterpSpeed = 20.f;

	// 当前实际 FOV，每帧被 InterpFOV 平滑更新后写入相机
	float CurrentFOV;

	// 每帧根据 bAiming 状态平滑切换相机视野（开镜/收镜）
	void InterpFOV(float DeltaTime);

};
