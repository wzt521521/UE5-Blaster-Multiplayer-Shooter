// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blaster/BlasterTypes/TurningInPlace.h"
#include "GameFramework/Character.h"
#include "BlasterCharacter.generated.h"

UCLASS()
class BLASTER_API ABlasterCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	ABlasterCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	//函数内部注册要复制的变量
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;//在这个函数中访问combat组件


protected:

	virtual void BeginPlay() override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void EquipButtonPressed();
	void CrouchButtonPressed();
	void AimButtonPressed();
	void AimButtonReleased();
	void AimOffset(float DeltaTime);

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	class UCameraComponent* FollowCamera;


	UPROPERTY(EditAnywhere,BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWidgetComponent* OverheadWidget;

	UPROPERTY(ReplicatedUsing=OnRep_OverlappingWeapon)
	class AWeapon* OverlappingWeapon;//可以复制


	//LastWeapon 是 Unreal Engine 复制系统自动管理的，不需要你自己更新。

	// 工作原理：当 ReplicatedUsing 标记的属性通过网络复制到客户端时，引擎会：

	// 先保存该属性复制前的旧值
	// 用新值覆盖属性
	// 调用 OnRep 回调，把旧值作为参数传入（即 LastWeapon）
	// 所以你只需要在 GetLifetimeReplicatedProps 中注册好：


	// DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
	// 然后引擎会自动追踪旧值并在复制发生时传给你的 OnRep_OverlappingWeapon。
	UFUNCTION()
	void OnRep_OverlappingWeapon(AWeapon* LastWeapon);//被复制时才会调用的函数，服务器永远不会被复制，所以服务器永远不会调用
	UPROPERTY(VisibleAnywhere, Category = "Combat")
	class UCombatComponent* Combat;

	UFUNCTION(Server, Reliable)
	void ServerEquipWeapon(AWeapon* WeaponToEquip);
	float AO_Pitch;
	float AO_Yaw;
	float InterpAO_Yaw;
	FRotator StartAimRotation;//开始瞄准时的旋转值

	ETurningInPlace TurningInPlace;//翻转状态
	void TurnInPlace(float DeltaTime);

public:
	void SetOverlappingWeapon(AWeapon* Weapon) ;

	bool IsWeaponEquipped();
	bool IsAiming();
	FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; }
	FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; }
	FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; }
	
};
