// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon.generated.h"

class UTexture2D;
UENUM(BlueprintType)
enum class EWeaponState : uint8
{
	EWS_Initial UMETA(DisplayName = "Initial State"),
	EWS_Equipped UMETA(DisplayName = "Equipped"),
	EWS_Dropped UMETA(DisplayName = "Dropped"),
	EWS_MAX UMETA(DisplayName = "DefaultMAX")
};

UCLASS()
class BLASTER_API AWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	
	AWeapon();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void ShowPickupWidget(bool bShowWidget);
	virtual void Fire(const FVector& HitTarget);
	void Dropped();
	UPROPERTY(EditAnywhere, Category = Combat)
	float FireDelay = .15f;

	UPROPERTY(EditAnywhere, Category = Combat)
	bool bAutomatic = true;

protected:
	
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnSphereOverlap(
		UPrimitiveComponent* OverlappedComponent,	// 触发重叠的自身组件（即AreaSphere）
		AActor* OtherActor,							// 与之重叠的另一个Actor
		UPrimitiveComponent* OtherComp,				// OtherActor上参与重叠的组件
		int32 OtherBodyIndex,						// OtherComp的Body索引（用于骨骼网格体的多Body情况）
		bool bFromSweep,							// 是否由扫描移动（Sweep Movement）触发
		const FHitResult& SweepResult				// 扫描命中结果（碰撞点、法线、冲击方向等）
	);
	UFUNCTION()
	void OnSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

private:
	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	class USkeletalMeshComponent* WeaponMesh;
	
	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	class USphereComponent* AreaSphere;
	UPROPERTY(ReplicatedUsing=OnRep_WeaponState, VisibleAnywhere, Category = "Weapon Properties")
	EWeaponState WeaponState = EWeaponState::EWS_Initial;

	UFUNCTION()
	void OnRep_WeaponState();

	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties")
	class UWidgetComponent* PickupWidget;

	UPROPERTY(EditAnywhere, Category = "Weapon Properties")
	class UAnimationAsset* FireAnimation;

	UPROPERTY(EditAnywhere)
	TSubclassOf<class ACasing> CasingClass;


public:
	// 准星五方向纹理，CombactComponent 每帧读取并打包传给 HUD 绘制
	UPROPERTY(EditAnywhere,Category="Crosshairs")
	UTexture2D* CrosshairsCenter;
	UPROPERTY(EditAnywhere,Category="Crosshairs")
	UTexture2D* CrosshairsLeft;
	UPROPERTY(EditAnywhere,Category="Crosshairs")
	UTexture2D* CrosshairsRight;
	UPROPERTY(EditAnywhere,Category="Crosshairs")
	UTexture2D* CrosshairsBottom;
	UPROPERTY(EditAnywhere, Category = "Crosshairs")
	UTexture2D* CrosshairsTop;

	// 瞄准时的相机视野，CombatComponent::InterpFOV 读取作为开镜插值目标
	UPROPERTY(EditAnywhere)
	float ZoomedFOV = 30.f;

	// 开镜速度，CombatComponent::InterpFOV 读取作为开镜插值速度
	UPROPERTY(EditAnywhere)
	float ZoomInterpSpeed = 20.f;

	void SetWeaponState(EWeaponState State);
	FORCEINLINE USphereComponent* GetAreaSphere() const {return AreaSphere;}
	FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const {return WeaponMesh;}
	FORCEINLINE float GetZoomedFOV() const { return ZoomedFOV; }
	FORCEINLINE float GetZoomInterpSpeed() const { return ZoomInterpSpeed; }
};
