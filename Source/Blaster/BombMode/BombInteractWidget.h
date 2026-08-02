#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "BombInteractWidget.generated.h"

// 安包/拆包交互进度条 Widget：靠近下包点或已安放炸弹时显示。
// 蓝图创建时只需放入 ProgressBar 和 TextBlock，命名一致即自动绑定。
UCLASS(BlueprintType, Blueprintable)
class BLASTER_API UBombInteractWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ── BindWidget：蓝图里控件名必须完全匹配 ──

	// 进度条：蓝图中命名为 ProgressBar（UProgressBar 控件）
	UPROPERTY(meta = (BindWidget))
	UProgressBar* ProgressBar;

	// 提示文字：蓝图中命名为 PromptText（UTextBlock 控件）
	UPROPERTY(meta = (BindWidget))
	UTextBlock* PromptText;

	// ── C++ 直接操作，蓝图无需实现任何事件 ──

	// 更新进度 0.0 ~ 1.0
	void UpdateProgress(float Progress);

	// 更新提示文字："[Q] 安放炸弹" / "[Q] 拆除炸弹"
	void UpdatePromptText(const FString& Text);

	// 显示/隐藏整个 Widget
	void SetInteractVisible(bool bVisible);

	// 单独控制进度条可见性：靠近点位时隐藏进度条，按下 Q 键后显示
	void SetProgressBarVisible(bool bVisible);
};
