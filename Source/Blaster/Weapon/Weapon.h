// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponTypes.h"
#include "Sound/SoundCue.h"
#include "Weapon.generated.h"

class UTexture2D;
class ABlasterCharacter;
class ABlasterPlayerController;
UENUM(BlueprintType)
enum class EWeaponState : uint8
{
	EWS_Initial UMETA(DisplayName = "Initial State"),
	EWS_Equipped UMETA(DisplayName = "Equipped"),
	EWS_Dropped UMETA(DisplayName = "Dropped"),
	EWS_MAX UMETA(DisplayName = "DefaultMAX")
};

UENUM(BlueprintType)
enum class EFireType : uint8
{
	EFT_HitScan UMETA(DisplayName = "Hit Scan Weapon"),
	EFT_Projectile UMETA(DisplayName = "Projectile Weapon"),
	EFT_Shotgun UMETA(DisplayName = "Shotgun Weapon"),

	EFT_MAX UMETA(DisplayName = "DefaultMAX")
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
	void SetHUDAmmo();
	void Dropped();
	// 从备弹转移指定数量到弹匣（换弹时服务器调用）
	void ReloadFromSpare(int32 Amount);
	// 从拾取物添加备弹（拾取弹药时调用）
	void AddToSpare(int32 Amount);
	virtual void OnRep_Owner() override;
	UPROPERTY(EditAnywhere, Category = Combat)
	float FireDelay = .15f;

	UPROPERTY(EditAnywhere, Category = Combat)
	bool bAutomatic = true;

	UPROPERTY(EditAnywhere, Category = Combat)
	bool bUseScatter = false;

	// 后坐力 — 每发子弹的摄像机视角偏移（纯视觉表现，不影响子弹落点）
	UPROPERTY(EditAnywhere, Category = "Visual Recoil")
	float RecoilPitchMin = 0.3f;   // 上跳角度下限 (度)
	UPROPERTY(EditAnywhere, Category = "Visual Recoil")
	float RecoilPitchMax = 0.8f;   // 上跳角度上限 (度)
	UPROPERTY(EditAnywhere, Category = "Visual Recoil")
	float RecoilYawMin = -0.2f;    // 水平偏移下限 (度)
	UPROPERTY(EditAnywhere, Category = "Visual Recoil")
	float RecoilYawMax = 0.2f;     // 水平偏移上限 (度)

	UPROPERTY(EditAnywhere)
	class USoundCue* EquipSound;

	bool bDestroyWeapon = false;

	void EnableCustomDepth(bool bEnable);

	UPROPERTY(EditAnywhere)
	EFireType FireType;

protected:
	
	virtual void BeginPlay() override;

	virtual void OnWeaponStateSet();
	virtual void OnEquipped();
	virtual void OnDropped();

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

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float DistanceToSphere = 800.f;

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float SphereRadius = 75.f;

	// 瞄准时的散布球距离/半径 — 值越小子弹越集中
	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float AimDistanceToSphere = 400.f;

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float AimSphereRadius = 25.f;

	UPROPERTY(EditAnywhere)
	float Damage = 20.f;

	UPROPERTY(EditAnywhere)
	float HeadShotDamage = 40.f;

	UPROPERTY()
	ABlasterCharacter* BlasterCharacterOwner;
	UPROPERTY()
	ABlasterPlayerController* BlasterControllerOwner;

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

	UPROPERTY(EditAnywhere)
	int32 MagCapacity;

	UPROPERTY(ReplicatedUsing=OnRep_Ammo)
	int32 Ammo;

	UFUNCTION()
	void OnRep_Ammo();

	UFUNCTION()
	void SpendRound();

	// 该武器最大备弹量（如 AR=90, 手枪=60），每个武器蓝图单独配置
	UPROPERTY(EditAnywhere, Category = "Weapon Properties")
	int32 MaxSpareAmmo;

	// 当前备弹量，仅同步给持有者
	UPROPERTY(ReplicatedUsing = OnRep_SpareAmmo)
	int32 SpareAmmo;

	UFUNCTION()
	void OnRep_SpareAmmo();

	UPROPERTY(EditAnywhere, Category = "Weapon Properties")
	EWeaponType WeaponType;

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
	// 默认 78°（约 1.15x），接近腰射视野，实现轻微放大效果
	UPROPERTY(EditAnywhere)
	float ZoomedFOV = 78.f;

	// 开镜速度，CombatComponent::InterpFOV 读取作为开镜插值速度
	UPROPERTY(EditAnywhere)
	float ZoomInterpSpeed = 6.f;

	void SetWeaponState(EWeaponState State);
	FORCEINLINE USphereComponent* GetAreaSphere() const {return AreaSphere;}
	FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const {return WeaponMesh;}
	FORCEINLINE float GetZoomedFOV() const { return ZoomedFOV; }
	FORCEINLINE float GetZoomInterpSpeed() const { return ZoomInterpSpeed; }
	FORCEINLINE bool IsEmpty() const { return Ammo <= 0; }
	FORCEINLINE bool IsFull() const { return Ammo >= MagCapacity; }
	FORCEINLINE EWeaponType GetWeaponType() const { return WeaponType; }
	FORCEINLINE EWeaponType GetReloadType() const { return WeaponType; }
	FORCEINLINE int32 GetAmmo() const { return Ammo; }
	FORCEINLINE int32 GetMagCapacity() const { return MagCapacity; }
	FORCEINLINE int32 GetSpareAmmo() const { return SpareAmmo; }
	FORCEINLINE int32 GetMaxSpareAmmo() const { return MaxSpareAmmo; }
	FORCEINLINE bool HasSpareAmmo() const { return SpareAmmo > 0; }
	FORCEINLINE float GetDamage() const { return Damage; }
	FORCEINLINE float GetHeadShotDamage() const { return HeadShotDamage; }
	FORCEINLINE float GetAimDistanceToSphere() const { return AimDistanceToSphere; }
	FORCEINLINE float GetAimSphereRadius() const { return AimSphereRadius; }

	FVector TraceEndWithScatter(const FVector& HitTarget, bool bAiming);
};
