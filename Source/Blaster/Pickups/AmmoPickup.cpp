#include "AmmoPickup.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Components/WidgetComponent.h"
#include "Components/SphereComponent.h"

AAmmoPickup::AAmmoPickup()
{
	// 创建拾取提示 Widget 组件，挂载到根组件（基类 PickupMesh）
	// 与 Weapon 的 PickupWidget 同款机制：默认隐藏，重叠后由复制驱动显示
	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(RootComponent);
}

void AAmmoPickup::BeginPlay()
{
	// 先执行基类逻辑：服务器延迟绑定 BeginOverlap、设置碰撞
	Super::BeginPlay();

	// 所有端初始隐藏拾取提示，后续由 SetOverlappingAmmo → 复制 → OnRep 控制
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(false);
	}
}

void AAmmoPickup::BindOverlapTimerFinished()
{
	// 基类负责：服务器延迟 0.25s 后绑定 OnSphereOverlap（避免生成瞬间误触）
	Super::BindOverlapTimerFinished();

	// 额外绑定 EndOverlap：角色离开范围时清除追踪，隐藏拾取 UI
	if (HasAuthority())
	{
		OverlapSphere->OnComponentEndOverlap.AddDynamic(this, &AAmmoPickup::OnSphereEndOverlap);
	}
}

void AAmmoPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Super::OnSphereOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	// 不再自动拾取+销毁，改为追踪模式：
	// 将自身注册到 Character->OverlappingAmmo，触发复制→客户端 OnRep→显示 PickupWidget
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter)
	{
		BlasterCharacter->SetOverlappingAmmo(this);
	}
}

void AAmmoPickup::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 角色离开拾取范围：清除追踪，OnRep 自动隐藏 Widget
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(OtherActor);
	if (BlasterCharacter)
	{
		BlasterCharacter->SetOverlappingAmmo(nullptr);
	}
}

void AAmmoPickup::ShowPickupWidget(bool bShowWidget)
{
	// 与 Weapon::ShowPickupWidget 完全相同的接口
	// 由 BlasterCharacter::SetOverlappingAmmo 和 OnRep_OverlappingAmmo 调用
	if (PickupWidget)
	{
		PickupWidget->SetVisibility(bShowWidget);
	}
}
