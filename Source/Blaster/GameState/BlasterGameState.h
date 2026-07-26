// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "BlasterGameState.generated.h"
class ABlasterPlayerState;


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

	// 当前回合号
	UPROPERTY(Replicated)
	int32 CurrentRoundNumber = 0;

	// 攻击方/防守方回合胜场
	UPROPERTY(Replicated)
	int32 AttackerWins = 0;
	UPROPERTY(Replicated)
	int32 DefenderWins = 0;

	// 上一回合胜者
	UPROPERTY(Replicated)
	ETeamID LastRoundWinner = ETeamID::ETI_None;

	// 比赛最终胜者
	UPROPERTY(Replicated)
	ETeamID LastMatchWinner = ETeamID::ETI_None;

	// 各阶段时长配置（服务器 GameMode BeginPlay 时写入一次，复制给客户端用于本地倒计时推算）
	UPROPERTY(Replicated)
	float RoundPrepareDuration = 5.f;
	UPROPERTY(Replicated)
	float RoundEndDuration = 4.f;
	UPROPERTY(Replicated)
	float MatchEndDuration = 8.f;

	// 存活人数（由 GameMode 事件驱动更新，复制到客户端供 RoundOverlay 显示）
	UPROPERTY(Replicated)
	int32 AttackerAliveCount = 0;
	UPROPERTY(Replicated)
	int32 DefenderAliveCount = 0;
};
