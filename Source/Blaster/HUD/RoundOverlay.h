#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "RoundOverlay.generated.h"

class UTextBlock;

// 回合信息面板：回合数、存活人数、大比分、阵营标识
// 通过 GameState 委托自动更新，不再依赖 PlayerController 搬运数据
UCLASS()
class BLASTER_API URoundOverlay : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* RoundNumberText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* AttackerAliveText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DefenderAliveText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ScoreText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TeamLabel;

private:
	// 委托回调：GameState 数据变化时自动刷新对应控件
	UFUNCTION()
	void RefreshAliveCount(int32 AttackerAlive, int32 DefenderAlive);
	UFUNCTION()
	void RefreshRoundInfo(int32 RoundNumber, int32 AttackerWins, int32 DefenderWins);
	UFUNCTION()
	void RefreshRoundResult(ETeamID Winner, int32 AttackerWins, int32 DefenderWins);
	UFUNCTION()
	void RefreshMatchResult(ETeamID Winner, int32 AttackerWins, int32 DefenderWins);

	// 从本地 PlayerState 读取阵营标识更新 TeamLabel（与 Announcement::RefreshRoundInfo 同模式）
	// GameState 委托携带的是全局数据，阵营归属是每玩家数据，需从 PlayerState 直接读取
	void RefreshTeamLabel();
};
