// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "Blaster/BlasterTypes/EconomyTypes.h"
#include "BlasterPlayerState.generated.h"


class ABlasterCharacter;
class ABlasterPlayerController;
/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_Score() override;
	void AddToScore(float ScoreAmount);

	UFUNCTION()
	virtual void OnRep_Defeats();
	void AddToDefeats(int32 DefeatsAmount);

	// 读取死亡次数（Defeats 私有字段唯一读取入口，比赛结束时 GameMode 统计持久化用）
	int32 GetDefeats() const { return Defeats; }

	// ── 持久身份（P4 玩家数据持久化）──
	// 客户端上报的持久玩家 ID（GUID，见 FBlasterPlayerIdentity），不复制——
	// 仅服务端保存，比赛结算时写入 player_stats.player_id 列（按人归集的键）
	void SetPlayerId(const FString& InPlayerId) { PlayerId = InPlayerId; }
	const FString& GetPlayerId() const { return PlayerId; }

	// 当前所属阵营（服务器权威修改，复制到所有客户端）
	UPROPERTY(ReplicatedUsing = OnRep_TeamID)
	ETeamID TeamID = ETeamID::ETI_None;

	UFUNCTION()
	void OnRep_TeamID();

	// 服务器权威 Setter：更新 TeamID → 触发 OnRep 通知 Controller/OverheadWidget
	void SetTeamID(ETeamID NewTeamID);

	// ────────────────────────────────────────────
	// 经济系统属性
	// ────────────────────────────────────────────

	// 当前持有金额（服务器权威修改，复制到所有客户端）
	UPROPERTY(ReplicatedUsing = OnRep_Money)
	int32 Money = 0;

	// 逻辑队伍（比赛开始时分配，半场交换后不变）
	UPROPERTY(ReplicatedUsing = OnRep_LogicalTeam)
	ELogicalTeam LogicalTeam = ELogicalTeam::ELT_None;

	// Money 变化委托（BlueprintAssignable → 蓝图绑定驱动 BuyMenu/HUD 刷新）
	// 参数: NewMoney（变化后金额）, Delta（变动量，正加负扣）
	UPROPERTY(BlueprintAssignable)
	FOnMoneyChanged OnMoneyChanged;

	// ── 经济操作方法（服务器权威）──

	// 增加金额（MaxMoney=-1 时跳过上限裁剪），内部广播 OnMoneyChanged
	void AddMoney(int32 Amount);

	// 设置逻辑队伍（仅在 AssignTeamsOnce 中调用一次，永不改变）
	void SetLogicalTeam(ELogicalTeam NewTeam);

	// 归零回合击杀计数（StartRoundInProgress 时调用）
	void ResetRoundKills();

	// 回合击杀计数 +1（OnPlayerKilled 中调用，不立即发钱）
	void IncrementRoundKills();

	// 读取当前回合击杀数（DistributeRoundEconomy 结算用）
	int32 GetRoundKills() const { return RoundKills; }

	// ── OnRep 回调 ──
	UFUNCTION()
	void OnRep_Money();

	UFUNCTION()
	void OnRep_LogicalTeam();

private:
	UPROPERTY()
	ABlasterCharacter* Character;//玩家角色——UPROPERTY 防止 Pawn 销毁后变成野指针
	UPROPERTY()
	ABlasterPlayerController* Controller;//玩家控制器——同上

	UPROPERTY(ReplicatedUsing = OnRep_Defeats)
	int32 Defeats;

	// 当前回合击杀数（仅服务端使用，不复制。回合结束时按此值 × KillReward 结算后归零）
	int32 RoundKills = 0;

	// 客户端上报的持久玩家 ID（GUID，不复制）——比赛结算时随战绩写入 SQLite
	FString PlayerId;
};
