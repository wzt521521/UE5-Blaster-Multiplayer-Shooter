#include "TeamDeathmatchGameMode.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/GameMode/BlasterGameMode.h"    // MatchState 命名空间常量
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

ATeamDeathmatchGameMode::ATeamDeathmatchGameMode()
{
	// 推迟角色生成：PostLogin 获得控制器，手动控制角色生成时机
	bDelayedStart = true;
}

void ATeamDeathmatchGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void ATeamDeathmatchGameMode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATeamDeathmatchGameMode, AttackerAliveCount);
	DOREPLIFETIME(ATeamDeathmatchGameMode, DefenderAliveCount);
}

// ========================================================================
// Tick 驱动状态机
// WaitingToStart → AssignTeams(瞬间) → RoundPrepare → RoundInProgress → RoundEnd → ...
// ========================================================================
void ATeamDeathmatchGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MatchState == MatchState::WaitingToStart)
	{
		// 人数达标 → 一次性分配阵营 → 进入第一个回合准备
		if (GetActivePlayers().Num() >= AimPeople)
		{
			AssignTeamsOnce();
			StartRoundPrepare();
		}
	}
	else if (MatchState == MatchState::RoundPrepare)
	{
		CountdownTime -= DeltaTime;
		if (CountdownTime <= 0.f)
		{
			StartRoundInProgress();
		}
	}
	else if (MatchState == MatchState::RoundInProgress)
	{
		// Tick 不遍历 PlayerArray：存活计数由 OnPlayerKilled 事件驱动
		CheckRoundEnd();
	}
	else if (MatchState == MatchState::RoundEnd)
	{
		CountdownTime -= DeltaTime;
		if (CountdownTime <= 0.f)
		{
			CheckMatchEnd(); // 内部判断是继续下一回合还是结束比赛
		}
	}
	else if (MatchState == MatchState::MatchEnd)
	{
		CountdownTime -= DeltaTime;
		if (CountdownTime <= 0.f)
		{
			ReturnToLobby();
		}
	}
}

// ========================================================================
// 阵营分配：比赛开始一次性随机分配，整场不变
// ========================================================================
void ATeamDeathmatchGameMode::AssignTeamsOnce()
{
	if (bTeamsAssigned) return;
	bTeamsAssigned = true;

	TArray<ABlasterPlayerState*> ActivePlayers = GetActivePlayers();

	// Fisher-Yates 打乱：保证随机公平
	for (int32 i = ActivePlayers.Num() - 1; i > 0; i--)
	{
		int32 j = FMath::RandRange(0, i);
		ActivePlayers.Swap(i, j);
	}

	// 奇数 N → 攻击者多一个（ceil(N/2)）
	int32 AttackerCount = FMath::CeilToInt(ActivePlayers.Num() / 2.0f);
	for (int32 i = 0; i < ActivePlayers.Num(); i++)
	{
		ETeamID Team = (i < AttackerCount) ? ETeamID::ETI_Attacker : ETeamID::ETI_Defender;
		ActivePlayers[i]->SetTeamID(Team);
	}
}

// ========================================================================
// 回合生命周期函数
// ========================================================================
void ATeamDeathmatchGameMode::StartRoundPrepare()
{
	RoundNumber++;
	CountdownTime = RoundPrepareTime;
	SetMatchState(MatchState::RoundPrepare);

	// 清尸体、重生所有玩家、重置存活计数
	CleanupBodiesAndRespawn();
}

void ATeamDeathmatchGameMode::StartRoundInProgress()
{
	CountdownTime = 0.f;
	SetMatchState(MatchState::RoundInProgress);
}

void ATeamDeathmatchGameMode::OnPlayerKilled(ABlasterCharacter* DeadCharacter,
	ABlasterPlayerController* VictimController,
	ABlasterPlayerController* AttackerController)
{
	if (!VictimController || !VictimController->PlayerState) return;

	// 加分统计（保留 Deathmatch 计分逻辑，可用于后续经济系统）
	ABlasterPlayerState* AttackerPS = AttackerController
		? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
	ABlasterPlayerState* VictimPS = Cast<ABlasterPlayerState>(VictimController->PlayerState);

	if (AttackerPS && AttackerPS != VictimPS)
	{
		AttackerPS->AddToScore(1.f);
	}
	if (VictimPS)
	{
		VictimPS->AddToDefeats(1);
	}

	// 死亡角色表现处理（播放动画 + 禁用输入/碰撞）
	if (DeadCharacter)
	{
		DeadCharacter->Elim();
	}

	// 事件驱动递减存活计数器（O(1) 判定）
	if (VictimPS)
	{
		if (VictimPS->TeamID == ETeamID::ETI_Attacker)
			AttackerAliveCount--;
		else if (VictimPS->TeamID == ETeamID::ETI_Defender)
			DefenderAliveCount--;
	}

	CheckRoundEnd();
}

void ATeamDeathmatchGameMode::CheckRoundEnd()
{
	// 仅在战斗中检查：回合结束/准备阶段不重复判定
	if (MatchState != MatchState::RoundInProgress) return;

	if (AttackerAliveCount <= 0)
		EndRound(ETeamID::ETI_Defender);
	else if (DefenderAliveCount <= 0)
		EndRound(ETeamID::ETI_Attacker);
}

void ATeamDeathmatchGameMode::EndRound(ETeamID Winner)
{
	if (Winner == ETeamID::ETI_Attacker)
		AttackerRoundWins++;
	else if (Winner == ETeamID::ETI_Defender)
		DefenderRoundWins++;

	CountdownTime = RoundEndTime;
	SetMatchState(MatchState::RoundEnd);
}

void ATeamDeathmatchGameMode::CheckMatchEnd()
{
	if (AttackerRoundWins >= RoundsToWin)
		ConcludeMatch(ETeamID::ETI_Attacker);
	else if (DefenderRoundWins >= RoundsToWin)
		ConcludeMatch(ETeamID::ETI_Defender);
	else
		StartRoundPrepare(); // 继续下一回合
}

void ATeamDeathmatchGameMode::ConcludeMatch(ETeamID Winner)
{
	CountdownTime = MatchEndTime;
	SetMatchState(MatchState::MatchEnd);
}

void ATeamDeathmatchGameMode::ReturnToLobby()
{
	UWorld* World = GetWorld();
	if (World)
	{
		bUseSeamlessTravel = true;
		World->ServerTravel(LobbyMapPath);
	}
}

// ========================================================================
// 复活逻辑：销毁死尸 → 重生所有玩家 → 重置存活计数
// ========================================================================
void ATeamDeathmatchGameMode::CleanupBodiesAndRespawn()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		// 销毁旧 Pawn（死尸/旁观 Pawn）
		if (PC->GetPawn())
		{
			PC->GetPawn()->Reset();
			PC->GetPawn()->Destroy();
		}

		// 重生：在随机 PlayerStart 生成新 Pawn
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
		if (PlayerStarts.Num() > 0)
		{
			int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
			RestartPlayerAtPlayerStart(PC, PlayerStarts[Selection]);
		}
		else
		{
			RestartPlayer(PC);
		}
	}

	// 重置存活计数 = 各阵营总人数（低频调用，可遍历）
	AttackerAliveCount = GetPlayersInTeam(ETeamID::ETI_Attacker).Num();
	DefenderAliveCount = GetPlayersInTeam(ETeamID::ETI_Defender).Num();
}

// ========================================================================
// 状态推送：服务器 MatchState 变化 → 通知所有 PlayerController
// ========================================================================
void ATeamDeathmatchGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABlasterPlayerController* BlasterPlayer = Cast<ABlasterPlayerController>(*It);
		if (BlasterPlayer)
		{
			BlasterPlayer->OnMatchStateSet(MatchState, true);
		}
	}
}

// ========================================================================
// 玩家加入/退出
// ========================================================================
void ATeamDeathmatchGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (bTeamsAssigned)
	{
		// 比赛已开始 → 中途加入处理
		HandleMidRoundJoin(NewPlayer);
	}
	// else：比赛未开始，AssignTeamsOnce 时统一分配
}

void ATeamDeathmatchGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	HandleMidRoundLeave(Exiting);
}

void ATeamDeathmatchGameMode::HandleMidRoundJoin(APlayerController* NewPlayer)
{
	ABlasterPlayerState* PS = NewPlayer->GetPlayerState<ABlasterPlayerState>();
	if (!PS) return;

	// 按阵营总人数分配到人少的一方（保持双方平衡）
	int32 AtkCount = GetPlayersInTeam(ETeamID::ETI_Attacker).Num();
	int32 DefCount = GetPlayersInTeam(ETeamID::ETI_Defender).Num();
	ETeamID AssignedTeam = (AtkCount <= DefCount)
		? ETeamID::ETI_Attacker : ETeamID::ETI_Defender;
	PS->SetTeamID(AssignedTeam);

	if (MatchState == MatchState::RoundInProgress)
	{
		// 回合进行中不生成 Pawn → 旁观到下回合开始
		// AliveCount 不递增（尚未生成 Pawn，不参与当前回合）
	}
	else
	{
		RestartPlayer(NewPlayer);
		// 下回合 CleanupBodiesAndRespawn 时 AliveCount 按 GetPlayersInTeam 重置
	}
}

void ATeamDeathmatchGameMode::HandleMidRoundLeave(AController* Exiting)
{
	ABlasterPlayerState* PS = Exiting->GetPlayerState<ABlasterPlayerState>();
	if (!PS) return;

	// 从存活计数中移除（如果是战斗中且该玩家还活着）
	if (MatchState == MatchState::RoundInProgress)
	{
		APawn* ExitingPawn = Exiting->GetPawn();
		ABlasterCharacter* ExitingChar = Cast<ABlasterCharacter>(ExitingPawn);
		// 仅在未死亡时递减：已死亡的玩家已经递减过了
		if (ExitingChar && !ExitingChar->IsElimmed())
		{
			if (PS->TeamID == ETeamID::ETI_Attacker)
				AttackerAliveCount--;
			else if (PS->TeamID == ETeamID::ETI_Defender)
				DefenderAliveCount--;
		}
		CheckRoundEnd();
	}
}

// ========================================================================
// 辅助函数
// ========================================================================
TArray<ABlasterPlayerState*> ATeamDeathmatchGameMode::GetPlayersInTeam(ETeamID Team) const
{
	TArray<ABlasterPlayerState*> Result;
	if (!GameState) return Result;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		ABlasterPlayerState* BPS = Cast<ABlasterPlayerState>(PS);
		if (BPS && BPS->TeamID == Team)
		{
			Result.Add(BPS);
		}
	}
	return Result;
}

TArray<ABlasterPlayerState*> ATeamDeathmatchGameMode::GetActivePlayers() const
{
	TArray<ABlasterPlayerState*> Result;
	if (!GameState) return Result;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		// 排除等待中的玩家（中途加入等待下回合者不算活跃）
		ABlasterPlayerState* BPS = Cast<ABlasterPlayerState>(PS);
		if (BPS && !PS->IsSpectator() && !PS->IsABot())
		{
			Result.Add(BPS);
		}
	}
	return Result;
}
