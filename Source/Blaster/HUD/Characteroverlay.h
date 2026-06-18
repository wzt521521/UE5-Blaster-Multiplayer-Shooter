// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Characteroverlay.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * 角色状态覆盖层（血条、子弹数等），直接绑定了蓝图中的同名控件
 *
 * meta = (BindWidget) 的工作原理：
 *   你在蓝图中创建一个 UMG 控件，命名为 "HealthBar"
 *   UE 会自动把 C++ 里的这个 UProgressBar* 指针绑定到蓝图里那个同名的 ProgressBar 控件
 *   不需要手动写 GetWidgetFromName 或 FindWidget
 *   但前提是：C++ 变量名 == 蓝图控件名，且类型匹配（UProgressBar 对应 ProgressBar）
 */
UCLASS()
class BLASTER_API UCharacteroverlay : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthBar;     // 血量进度条，绑定了蓝图中的 ProgressBar 控件

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HealthText;      // 血量数字文本，绑定了蓝图中的 TextBlock 控件

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ScoreAmount;     // 分数数字文本，绑定了蓝图中的 TextBlock 控件
};
