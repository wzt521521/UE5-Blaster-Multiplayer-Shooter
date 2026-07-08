// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterGameState.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Net/UnrealNetwork.h"

void ABlasterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABlasterGameState, TopScoringPlayers);
}

// ------------------------------------------------------------
// 最高分排行榜维护（服务器执行）：每次玩家分数变动时调用，
// 更新 TopScoringPlayers 数组 —— 始终保持当前最高分玩家（支持并列）
// 数组通过 DOREPLIFETIME 复制到所有客户端，HandleCooldown 读取显示胜者
// ------------------------------------------------------------
void ABlasterGameState::UpdateTopScore(ABlasterPlayerState *ScoringPlayer)
{
	if (ScoringPlayer == nullptr) return;

	// 关键：先把得分者从榜上移除，再用剩余玩家的分数作为"旧最高分"比较
	// 否则如果得分者已在榜上，TopScoringPlayers[0] 就是他自己的新分数，永远和自己比
	// 导致比他分低的旧并列者赖在榜上不被清除
	TopScoringPlayers.Remove(ScoringPlayer);

	if (TopScoringPlayers.Num() == 0)
	{
		// 榜上只剩他自己（或本来是空的），直接放回去
		TopScoringPlayers.Add(ScoringPlayer);
		return;
	}

	const float NewScore = ScoringPlayer->GetScore();
	// 取剩余玩家中任意一人的分数作为旧最高分（榜上所有人分数相同）
	const float CurrentTopScore = TopScoringPlayers[0]->GetScore();

	if (NewScore > CurrentTopScore)
	{
		// 打破旧纪录：清空旧榜，独占第一
		TopScoringPlayers.Empty();
		TopScoringPlayers.Add(ScoringPlayer);
	}
	else if (NewScore == CurrentTopScore)
	{
		// 追平纪录：加入并列
		TopScoringPlayers.AddUnique(ScoringPlayer);
	}
	// NewScore < CurrentTopScore：已被超越，不加入（Remove 已将其移除）
}
