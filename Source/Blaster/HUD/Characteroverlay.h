// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Characteroverlay.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * 角色状态覆盖层（血条、子弹数等），直接绑定了蓝图中的同名控件
 */
UCLASS()
class BLASTER_API UCharacteroverlay : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HealthText;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* ShieldBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ShieldText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ScoreAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DefeatsAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* AmmoAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CarriedAmmoAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* MatchCountdownText;

	// 弹药类型不匹配提示文本（"枪械与子弹不匹配！"）
	// 由 BlasterPlayerController::SetHUDMismatchNotification 设置，2秒后自动隐藏
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MismatchNotificationText;
};
