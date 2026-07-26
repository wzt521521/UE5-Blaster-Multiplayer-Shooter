// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OverheadWidget.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API UOverheadWidget : public UUserWidget
{
	GENERATED_BODY()
public:

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DisplayText;


	void SetDisplayText(FString TextToDisplay);
	// 保留旧函数名（蓝图兼容），内部改为阵营规则
	UFUNCTION(BlueprintCallable)
	void ShowPlayerNetRole(APawn* InPawn);
	// 阵营规则：自己/队友可见，敌方隐藏
	void ShowPlayerTeamRole(APawn* InPawn);
protected:
	virtual void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld) override;
};
