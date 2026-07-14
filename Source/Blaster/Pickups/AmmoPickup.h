#pragma once

#include "CoreMinimal.h"
#include "Pickup.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "AmmoPickup.generated.h"

// 前置声明：避免循环引用，BlasterCharacter.h 也引用 AAmmoPickup
class ABlasterCharacter;

UCLASS()
class BLASTER_API AAmmoPickup : public APickup
{
	GENERATED_BODY()

public:
	AAmmoPickup();

	// 与武器拾取同款：服务器 SetOverlappingAmmo/客户端 OnRep 触发此函数
	void ShowPickupWidget(bool bShowWidget);

	// 内联访问器：ServerPickupAmmo RPC 读取 AmmoType/AmmoAmount 传给 CombatComponent
	FORCEINLINE EWeaponType GetWeaponType() const { return WeaponType; }
	FORCEINLINE int32 GetAmmoAmount() const { return AmmoAmount; }

protected:
	// BeginPlay：所有端隐藏 PickupWidget，后续由复制驱动的 OnRep 控制显示
	virtual void BeginPlay() override;

	// 在基类延迟绑定 BeginOverlap 之后，服务器额外绑定 EndOverlap
	virtual void BindOverlapTimerFinished() override;

	// 重叠开始：将自身注册到 Character 的 OverlappingAmmo 追踪变量
	virtual void OnSphereOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	) override;

	// 重叠结束：清除 Character 的 OverlappingAmmo，驱动 OnRep 隐藏拾取 UI
	UFUNCTION()
	void OnSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex
	);

private:
	// 单次拾取提供的弹药量，蓝图可配
	UPROPERTY(EditAnywhere)
	int32 AmmoAmount = 30;

	// 该弹药匹配的武器类型，只有装备了同类型武器才能拾取
	UPROPERTY(EditAnywhere)
	EWeaponType WeaponType;

	// 拾取提示 UI（与武器 AreaSphere 同款，显示"按E拾取"）
	UPROPERTY(VisibleAnywhere)
	class UWidgetComponent* PickupWidget;
};
