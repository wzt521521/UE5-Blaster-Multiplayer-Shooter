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
#include "CombatComponent.generated.h"

class AWeapon;
class ABlasterCharacter;
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

protected:
	
	virtual void BeginPlay() override;
	void SetAiming(bool bIsAiming);

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bIsAiming);

	UFUNCTION()
	void OnRep_EquippedWeapon();
	
private:
	ABlasterCharacter* Character;//指向拥有这个组件的角色的指针

	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon)
	AWeapon* EquippedWeapon;//存储当前装备武器的变量

	UPROPERTY(Replicated)//会被复制
	bool bAiming;

	UPROPERTY(EditAnywhere)
	float BaseWalkSpeed;

	UPROPERTY(EditAnywhere)
	float AimWalkSpeed;
		
};
