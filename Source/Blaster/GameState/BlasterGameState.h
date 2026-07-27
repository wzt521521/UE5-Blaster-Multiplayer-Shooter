// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "BlasterGameState.generated.h"
class ABlasterPlayerState;

// 存活人数变化委托：GameMode 修改 AliveCount 后广播，Widget 绑定此委托自动刷新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAliveCountChanged, int32, AttackerAlive, int32, DefenderAlive);

// 回合信息变化委托：回合号或比分变化时广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRoundInfoChanged, int32, RoundNumber, int32, AttackerWins, int32, DefenderWins);

// 回合结果委托：EndRound 时广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRoundResultChanged, ETeamID, Winner, int32, AttackerWins, int32, DefenderWins);

// 比赛结果委托：ConcludeMatch 时广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMatchResultChanged, ETeamID, Winner, int32, AttackerWins, int32, DefenderWins);


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

	// 攻击方/防守方回合胜场（ReplicatedUsing：客户端收到复制时广播 OnRoundInfoChanged）
	UPROPERTY(ReplicatedUsing = OnRep_AttackerWins)
	int32 AttackerWins = 0;
	UPROPERTY(ReplicatedUsing = OnRep_DefenderWins)
	int32 DefenderWins = 0;

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

	// 广播函数：GameMode 在修改数据后手动调用（服务器端）
	//         OnRep 里自动调用（客户端收到复制时）
	void BroadcastAliveCount() { OnAliveCountChanged.Broadcast(AttackerAliveCount, DefenderAliveCount); }
	void BroadcastRoundInfo()  { OnRoundInfoChanged.Broadcast(CurrentRoundNumber, AttackerWins, DefenderWins); }
	void BroadcastRoundResult(){ OnRoundResultChanged.Broadcast(LastRoundWinner, AttackerWins, DefenderWins); }
	void BroadcastMatchResult(){ OnMatchResultChanged.Broadcast(LastMatchWinner, AttackerWins, DefenderWins); }

private:
	// OnRep 回调：客户端收到 AliveCount 复制时广播委托
	UFUNCTION()
	void OnRep_AliveCount();

	// OnRep 回调：客户端收到回合信息复制时广播对应委托
	// 每个属性的 OnRep 广播其所属的委托，确保客户端 Widget 与服务端同步更新
	UFUNCTION()
	void OnRep_CurrentRoundNumber();
	UFUNCTION()
	void OnRep_AttackerWins();
	UFUNCTION()
	void OnRep_DefenderWins();
	UFUNCTION()
	void OnRep_LastRoundWinner();
	UFUNCTION()
	void OnRep_LastMatchWinner();
};
