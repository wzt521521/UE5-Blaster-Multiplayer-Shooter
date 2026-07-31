// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "Blaster/BlasterTypes/EconomyTypes.h"
#include "Engine/DataTable.h"
#include "BlasterGameState.generated.h"
class ABlasterPlayerState;
class UEconomyConfig;
struct FShopItemRow;
class UDataTable;

// 存活人数变化委托：GameMode 修改 AliveCount 后广播，Widget 绑定此委托自动刷新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAliveCountChanged, int32, AttackerAlive, int32, DefenderAlive);

// 回合信息变化委托：回合号或比分变化时广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRoundInfoChanged, int32, RoundNumber, int32, TeamAWins, int32, TeamBWins);

// 回合结果委托：EndRound 时广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRoundResultChanged, ETeamID, Winner, int32, TeamAWins, int32, TeamBWins);

// 比赛结果委托：ConcludeMatch 时广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMatchResultChanged, ETeamID, Winner, int32, TeamAWins, int32, TeamBWins);


UCLASS()
class BLASTER_API ABlasterGameState : public AGameState
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void UpdateTopScore(ABlasterPlayerState* ScoringPlayer);
	UPROPERTY(Replicated)
	TArray<ABlasterPlayerState*> TopScoringPlayers;

	// ========================================================================
	// 回合制倒计时 & 回合信息（GameMode 推送 → 客户端通过 GameState 读取）
	// ========================================================================

	// 当前阶段剩余倒计时（秒），由 BombDefusalGameMode::Tick 每帧更新并复制到所有客户端
	UPROPERTY(Replicated)
	float RemainingCountdown = 0.f;

	// 当前回合号（ReplicatedUsing：客户端收到复制时广播 OnRoundInfoChanged）
	UPROPERTY(ReplicatedUsing = OnRep_CurrentRoundNumber)
	int32 CurrentRoundNumber = 0;

	// ── 逻辑队伍比分 + 经济状态（Phase 5 新增，替代旧 AttackerWins/DefenderWins）──

	// 逻辑队 A/B 回合胜场
	UPROPERTY(ReplicatedUsing = OnRep_TeamAWins)
	int32 TeamARoundWins = 0;
	UPROPERTY(ReplicatedUsing = OnRep_TeamBWins)
	int32 TeamBRoundWins = 0;

	// 连败计数（归属逻辑队，半场交换归零）
	UPROPERTY(Replicated)
	int32 TeamALossStreak = 0;
	UPROPERTY(Replicated)
	int32 TeamBLossStreak = 0;

	// 连胜计数
	UPROPERTY(Replicated)
	int32 TeamAWinStreak = 0;
	UPROPERTY(Replicated)
	int32 TeamBWinStreak = 0;

	// 半场状态
	UPROPERTY(Replicated)
	bool bIsSecondHalf = false;

	// 半场交换回合数（BeginPlay 时写入一次）
	UPROPERTY(Replicated)
	int32 HalftimeRound = 0;

	// 经济配置指针（BeginPlay 时写入一次，客户端通过此指针 AddMoney 裁剪）
	UPROPERTY(Replicated)
	UEconomyConfig* EconomyConfig = nullptr;

	// 比赛最终胜者（ELogicalTeam 维度）
	UPROPERTY(ReplicatedUsing = OnRep_LastMatchWinnerLT)
	ELogicalTeam LastMatchWinnerLT = ELogicalTeam::ELT_None;

	// 上一回合胜者（ReplicatedUsing：客户端收到复制时广播 OnRoundResultChanged）
	UPROPERTY(ReplicatedUsing = OnRep_LastRoundWinner)
	ETeamID LastRoundWinner = ETeamID::ETI_None;

	// 比赛最终胜者（ReplicatedUsing：客户端收到复制时广播 OnMatchResultChanged）
	UPROPERTY(ReplicatedUsing = OnRep_LastMatchWinner)
	ETeamID LastMatchWinner = ETeamID::ETI_None;

	// 各阶段时长配置（服务器 GameMode BeginPlay 时写入一次，复制给客户端用于本地倒计时推算）
	UPROPERTY(Replicated)
	float RoundPrepareDuration = 5.f;
	UPROPERTY(Replicated)
	float RoundEndDuration = 4.f;
	UPROPERTY(Replicated)
	float MatchEndDuration = 8.f;

	// 存活人数（由 GameMode 事件驱动更新，复制到客户端供 RoundOverlay 显示）
	UPROPERTY(ReplicatedUsing = OnRep_AliveCount)
	int32 AttackerAliveCount = 0;
	UPROPERTY(ReplicatedUsing = OnRep_AliveCount)
	int32 DefenderAliveCount = 0;

	// ---- 委托：Widget 绑定后自动响应数据变化，不再需要 PC 轮询 ----
	UPROPERTY(BlueprintAssignable)
	FOnAliveCountChanged OnAliveCountChanged;
	UPROPERTY(BlueprintAssignable)
	FOnRoundInfoChanged OnRoundInfoChanged;
	UPROPERTY(BlueprintAssignable)
	FOnRoundResultChanged OnRoundResultChanged;
	UPROPERTY(BlueprintAssignable)
	FOnMatchResultChanged OnMatchResultChanged;

	// ── 经济辅助方法（Phase 5，GameMode 通过 GameState 读写）──
	int32 GetLossStreakForTeam(ELogicalTeam T) const;
	int32 GetWinStreakForTeam(ELogicalTeam T) const;
	void IncrementLossStreak(ELogicalTeam T);
	void ResetLossStreak(ELogicalTeam T);
	void IncrementWinStreak(ELogicalTeam T);
	void ResetWinStreak(ELogicalTeam T);
	void ResetAllStreaks();
	void AddRoundWin(ELogicalTeam T);
	int32 GetRoundWinsForTeam(ELogicalTeam T) const;

	// 广播函数：GameMode 在修改数据后手动调用（服务器端）
	//         OnRep 里自动调用（客户端收到复制时）
	void BroadcastAliveCount() { OnAliveCountChanged.Broadcast(AttackerAliveCount, DefenderAliveCount); }
	void BroadcastRoundInfo()  { OnRoundInfoChanged.Broadcast(CurrentRoundNumber, TeamARoundWins, TeamBRoundWins); }
	void BroadcastRoundResult(){ OnRoundResultChanged.Broadcast(LastRoundWinner, TeamARoundWins, TeamBRoundWins); }
	void BroadcastMatchResult(){ OnMatchResultChanged.Broadcast(LastMatchWinner, TeamARoundWins, TeamBRoundWins); }

	// ── 商店物品 DataTable（购买系统数据源）──

	// GameMode 在 BeginPlay 时加载，服务端查表获取 Price/Category/Class
	// 客户端可直接读取（Listen Server 同进程），无需 Replicate
	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	class UDataTable* ShopItemTable = nullptr;

	// 通过 ItemID 遍历查找 DataTable 行，返回 nullptr 表示无效 ID
	const FShopItemRow* FindShopItem(int32 ItemID) const;

	// 供蓝图 BuyMenu 查询：按 ItemID 获取 Price（-1 表示无效 ID）
	// 比直接读 DataTable 更简洁，BuyMenu 灰显逻辑只需调此函数
	UFUNCTION(BlueprintCallable, Category = "Shop")
	int32 GetShopItemPrice(int32 ItemID) const;

private:
	// OnRep 回调：客户端收到 AliveCount 复制时广播委托
	UFUNCTION()
	void OnRep_AliveCount();

	// OnRep 回调：客户端收到回合信息复制时广播对应委托
	UFUNCTION()
	void OnRep_CurrentRoundNumber();
	UFUNCTION()
	void OnRep_LastRoundWinner();
	UFUNCTION()
	void OnRep_LastMatchWinner();
	UFUNCTION()
	void OnRep_TeamAWins();
	UFUNCTION()
	void OnRep_TeamBWins();
	UFUNCTION()
	void OnRep_LastMatchWinnerLT();
};
