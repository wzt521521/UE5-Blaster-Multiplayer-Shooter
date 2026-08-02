#include "Blaster/BombMode/BombSite.h"
#include "Components/BoxComponent.h"

ABombSite::ABombSite()
{
	// 服务器和客户端都需要碰撞检测 → bReplicates 不需要（纯空间数据，不变化）
	// 但位置信息由关卡静态确定，所有客户端本地渲染即可
	PrimaryActorTick.bCanEverTick = false;

	// 根组件：Box 碰撞体，关卡中拖放后手动调大小适配下包区域
	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap); // 只响应 Pawn 进入
	SetRootComponent(TriggerVolume);
}

void ABombSite::BeginPlay()
{
	Super::BeginPlay();
}

#if WITH_EDITOR
void ABombSite::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 细节面板修改 SiteName 后立即反映到 Actor Label，方便场景大纲视图识别
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ABombSite, SiteName))
	{
		SetActorLabel(FString::Printf(TEXT("BombSite_%s"), *SiteName));
	}
}
#endif
