#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BombSite.generated.h"

class UBoxComponent;

// 埋包点标记：纯空间数据，定义地图上合法的安包区域。不包含游戏逻辑。
// 关卡中拖放 → 调碰撞体大小 → 设 SiteName = "A" / "B"
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API ABombSite : public AActor
{
	GENERATED_BODY()

public:
	ABombSite();

	// 点位名称，用于 UI 显示和日志。默认 "A"，细节面板可改
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb Site")
	FString SiteName = TEXT("A");

	// 安包有效区域：角色进入此碰撞体后才能下包
	// InteractionComponent 用此碰撞体检测"是否在下包区域内"
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomb Site")
	UBoxComponent* TriggerVolume;

	// 炸弹是否已在此处安放（由 ABombActor::CompletePlant 设置/清除）
	UPROPERTY(BlueprintReadOnly, Category = "Bomb Site")
	bool bIsBombPlantedHere = false;

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	// 编辑器中可视化碰撞体范围
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
