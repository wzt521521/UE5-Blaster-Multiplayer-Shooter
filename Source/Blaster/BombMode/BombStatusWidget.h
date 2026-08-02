#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "BombStatusWidget.generated.h"

// 炸弹状态 HUD Widget：炸弹安放后显示倒计时、点位名、状态文字。
// 蓝图创建时只需放入三个 TextBlock，命名一致即自动绑定。
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API UBombStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ── BindWidget：蓝图里控件名必须完全匹配 ──

	// 倒计时数字：蓝图中命名为 TimerText
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TimerText;

	// 点位名：蓝图中命名为 SiteNameText
	UPROPERTY(meta = (BindWidget))
	UTextBlock* SiteNameText;

	// 状态文字：蓝图中命名为 StatusText
	UPROPERTY(meta = (BindWidget))
	UTextBlock* StatusText;

	// ── C++ 直接操作，蓝图无需实现任何事件 ──

	// 更新倒计时："25.3"
	void UpdateTimer(float RemainingTime, float TotalTime);

	// 更新点位名："A点"
	void UpdateSiteName(const FString& SiteName);

	// 更新状态文字："炸弹已安放" / "炸弹已拆除"
	void UpdateStatusText(const FString& Text);

	// 显示/隐藏整个 Widget
	void SetBombUIVisible(bool bVisible);
};
