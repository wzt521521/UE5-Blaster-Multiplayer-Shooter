// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "Announcement.generated.h"

UCLASS()
class BLASTER_API UAnnouncement : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* WarmupTime;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* AnnouncementText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* InfoText;

private:
	// 委托回调：GameState 数据变化时自动刷新公告内容
	UFUNCTION()
	void RefreshRoundInfo(int32 RoundNumber, int32 AttackerWins, int32 DefenderWins);
	UFUNCTION()
	void RefreshRoundResult(ETeamID Winner, int32 AttackerWins, int32 DefenderWins);
	UFUNCTION()
	void RefreshMatchResult(ETeamID Winner, int32 AttackerWins, int32 DefenderWins);

	// 延迟绑定委托：NativeConstruct 时 GameState 可能尚未复制到客户端，
	// 若 GS 为空则下一帧重试，确保委托一定被绑定
	void TryBindAndRefresh();
	bool bDelegatesBound = false;
	FTimerHandle RetryTimerHandle;
};
