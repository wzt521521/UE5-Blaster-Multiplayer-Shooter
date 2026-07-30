// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterGameState.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Net/UnrealNetwork.h"

void ABlasterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABlasterGameState, TopScoringPlayers);
    DOREPLIFETIME(ABlasterGameState, RemainingCountdown);
    DOREPLIFETIME(ABlasterGameState, CurrentRoundNumber);
    DOREPLIFETIME(ABlasterGameState, AttackerWins);
    DOREPLIFETIME(ABlasterGameState, DefenderWins);
    DOREPLIFETIME(ABlasterGameState, LastRoundWinner);
    DOREPLIFETIME(ABlasterGameState, LastMatchWinner);
    DOREPLIFETIME(ABlasterGameState, RoundPrepareDuration);
    DOREPLIFETIME(ABlasterGameState, RoundEndDuration);
    DOREPLIFETIME(ABlasterGameState, MatchEndDuration);
    DOREPLIFETIME(ABlasterGameState, AttackerAliveCount);
    DOREPLIFETIME(ABlasterGameState, DefenderAliveCount);
}

// ------------------------------------------------------------
// 最高分排行榜维护（服务器执行）：每次玩家分数变动时调用，
// 更新 TopScoringPlayers 数组 —— 始终保持当前最高分玩家（支持并列）
// 数组通过 DOREPLIFETIME 复制到所有客户端，HandleCooldown 读取显示胜者
// ------------------------------------------------------------
void ABlasterGameState::OnRep_AliveCount()
{
	OnAliveCountChanged.Broadcast(AttackerAliveCount, DefenderAliveCount);
}

// ------------------------------------------------------------
// OnRep 回调：客户端收到 GameState 属性复制时，广播对应的本地委托
// 服务端由 BombDefusalGameMode 直接调用 BroadcastXxx()；
// 客户端通过 OnRep → BroadcastXxx() 链获得相同的通知，
// 确保 RoundOverlay / Announcement 委托绑定在两端行为一致
// ------------------------------------------------------------
void ABlasterGameState::OnRep_CurrentRoundNumber()
{
	BroadcastRoundInfo();  // 回合号变化 → 广播回合信息
}
void ABlasterGameState::OnRep_AttackerWins()
{
	// ① 广播回合信息（比分更新属于回合信息变化）
	BroadcastRoundInfo();
	// ② 同时广播回合结果：比分变化只发生在 EndRound，
	//    此时 LastRoundWinner 已在同一批次中更新为正确值。
	//    当同队连胜时 OnRep_LastRoundWinner 不会触发（值未变），
	//    此处兜底确保客户端 Widget 能收到 RefreshRoundResult
	BroadcastRoundResult();
}
void ABlasterGameState::OnRep_DefenderWins()
{
	BroadcastRoundInfo();
	// ② 同上：兜底广播回合结果，解决同队连胜时 OnRep_LastRoundWinner 不触发的问题
	BroadcastRoundResult();
}
void ABlasterGameState::OnRep_LastRoundWinner()
{
	BroadcastRoundResult(); // 回合胜者变化 → 广播回合结果
}
void ABlasterGameState::OnRep_LastMatchWinner()
{
	BroadcastMatchResult(); // 比赛胜者变化 → 广播比赛结果
}

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
