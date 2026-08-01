#include "BombDefusalGameMode.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/BlasterTypes/MatchState.h"    // 独立 MatchState 常量（与 BlasterGameMode 解耦）
#include "Blaster/BlasterTypes/EconomyTypes.h" // ELogicalTeam 枚举 — AssignTeamsOnce/PostLogin 直接使用
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "Blaster/BlasterTypes/ShopTypes.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/BlasterComponents/ThrowableComponent.h"
#include "Blaster/BlasterComponents/BuffComponent.h"
#include "EngineUtils.h"  // TActorIterator

ABombDefusalGameMode::ABombDefusalGameMode()
{
	// 延迟开局：手动控制角色生成和状态机启动时机
	bDelayedStart = true;
	// 必须设 PlayerStateClass，否则 GetActivePlayers() 里 Cast<ABlasterPlayerState> 失败
	PlayerStateClass = ABlasterPlayerState::StaticClass();
	// 必须显式设 GameStateClass：确保客户端创建的 GameState 代理是 ABlasterGameState 类型，
	// 否则 GetGameState<ABlasterGameState>() 返回 nullptr，所有委托绑定和 OnRep 回调静默失效
	GameStateClass = ABlasterGameState::StaticClass();
}

void ABombDefusalGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 缓存 GameState 引用并同步阶段时长配置（这些值后续不再变化，但客户端需要知道）
	BlasterGameState = GetGameState<ABlasterGameState>();
	if (BlasterGameState)
	{
		BlasterGameState->RoundPrepareDuration = RoundPrepareTime;
		BlasterGameState->RoundEndDuration = RoundEndTime;
		BlasterGameState->MatchEndDuration = MatchEndTime;
	}

	// ===== ECONOMY CONFIG =====
	// 加载经济配置 DataAsset → 写入 GameState（Phase 5 迁移）
	if (HasAuthority() && !EconomyConfigRef.IsNull())
	{
		UEconomyConfig* Config = EconomyConfigRef.LoadSynchronous();
		if (BlasterGameState)
		{
			BlasterGameState->EconomyConfig = Config;
			BlasterGameState->HalftimeRound = HalftimeRound;
		}
	}

	// ===== SHOP DATA TABLE =====
	// 加载商店物品 DataTable → 写入 GameState（服务端查表定价用）
	if (HasAuthority() && !ShopItemTableRef.IsNull())
	{
		UDataTable* LoadedTable = ShopItemTableRef.LoadSynchronous();
		if (BlasterGameState)
		{
			BlasterGameState->ShopItemTable = LoadedTable;
			UE_LOG(LogTemp, Log, TEXT("[Shop] DT_ShopItems loaded: %d rows"),
				LoadedTable ? LoadedTable->GetRowMap().Num() : 0);
		}
	}
}

// ========================================================================
// Tick 驱动状态机
// WaitingToStart → AssignTeams(瞬间) → RoundPrepare → RoundInProgress → RoundEnd → ...
// ========================================================================
void ABombDefusalGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MatchState == MatchState::WaitingToStart)
	{
		// 人数达标 → 一次性分配阵营 → 短暂 AssignTeams 状态让客户端显示阵营提示
		if (GetActivePlayers().Num() >= AimPeople)
		{
			AssignTeamsOnce();
			SetMatchState(MatchState::AssignTeams);
		}
	}
	else if (MatchState == MatchState::AssignTeams)
	{
		// 阵营提示展示一帧后立即进入回合准备（无倒计时，瞬间过渡）
		StartRoundPrepare();
	}
	else if (MatchState == MatchState::RoundPrepare)
	{
		CountdownTime -= DeltaTime;
		if (BlasterGameState) BlasterGameState->RemainingCountdown = CountdownTime;
		if (CountdownTime <= 0.f)
		{
			StartRoundInProgress();
		}
	}
	else if (MatchState == MatchState::RoundInProgress)
	{
		// 回合计时器倒计时 + 存活计数由 OnPlayerKilled 事件驱动
		CountdownTime -= DeltaTime;
		if (BlasterGameState) BlasterGameState->RemainingCountdown = CountdownTime;
		// 超时：攻击方未能全灭/下包 → 保卫者获胜
		if (CountdownTime <= 0.f)
		{
			EndRound(ETeamID::ETI_Defender);
		}
		CheckRoundEnd();
	}
	else if (MatchState == MatchState::RoundEnd)
	{
		CountdownTime -= DeltaTime;
		if (BlasterGameState) BlasterGameState->RemainingCountdown = CountdownTime;
		if (CountdownTime <= 0.f)
		{
			CheckMatchEnd(); // 内部判断是继续下一回合还是结束比赛
		}
	}
	else if (MatchState == MatchState::HalftimeSwap)
	{
		// 半场交换展示倒计时
		CountdownTime -= DeltaTime;
		if (BlasterGameState) BlasterGameState->RemainingCountdown = CountdownTime;
		if (CountdownTime <= 0.f)
		{
			ExecuteHalftimeSwap();
			StartRoundPrepare();  // 下半场第一回合
		}
	}
	else if (MatchState == MatchState::MatchEnd)
	{
		CountdownTime -= DeltaTime;
		if (BlasterGameState) BlasterGameState->RemainingCountdown = CountdownTime;
		if (CountdownTime <= 0.f)
		{
			ReturnToLobby();
		}
	}
}

// ========================================================================
// 阵营分配：比赛开始一次性随机分配，整场不变
// ========================================================================
void ABombDefusalGameMode::AssignTeamsOnce()
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

		// ── 新增：分配 LogicalTeam ──
		// 前一半 → TeamA，后一半 → TeamB（与 Attacker/Defender 分配一致，上半场 TeamA=Attacker）
		ELogicalTeam LT = (i < AttackerCount) ? ELogicalTeam::ELT_TeamA : ELogicalTeam::ELT_TeamB;
		ActivePlayers[i]->SetLogicalTeam(LT);

		// ── 新增：发放起始金 ──
		if (BlasterGameState && BlasterGameState->EconomyConfig)
		{
			ActivePlayers[i]->Money = BlasterGameState->EconomyConfig->StartingMoney;  // $200
		}

		// ── 新增：归零 RoundKills ──
		ActivePlayers[i]->ResetRoundKills();
	}
}

// ========================================================================
// 回合生命周期函数
// ========================================================================
void ABombDefusalGameMode::StartRoundPrepare()
{
	RoundNumber++;

	// [NEW] Step 7: 回合开始前清理上回合残留
	ClearAllBuffsOnAllPlayers();    // 清除所有存活玩家的 Buff
	CleanupDroppedWeapons();        // 销毁地面遗留武器

	CountdownTime = RoundPrepareTime;

	// 必须先推送到 GameState，再切 MatchState：
	// SetMatchState → HandleRoundPrepare → 读 GameState，Sync 在后会导致读到旧值
	SyncToGameState();
	if (BlasterGameState) BlasterGameState->BroadcastRoundInfo();  // 委托驱动 Widget 刷新
	SetMatchState(MatchState::RoundPrepare);

	// 清尸体、重生所有玩家、重置存活计数
	CleanupBodiesAndRespawn();
}

void ABombDefusalGameMode::StartRoundInProgress()
{
	// 重新校准存活计数：RoundPrepare 期间可能有玩家退出导致计数失准
	// 直接写入 GameState（唯一权威源），引擎自动复制到所有客户端
	if (!BlasterGameState) BlasterGameState = GetGameState<ABlasterGameState>();
	if (BlasterGameState)
	{
		BlasterGameState->AttackerAliveCount = GetPlayersInTeam(ETeamID::ETI_Attacker).Num();
		BlasterGameState->DefenderAliveCount = GetPlayersInTeam(ETeamID::ETI_Defender).Num();
		// 重校准后广播：客户端通过 OnRep_AliveCount 自动广播，但服务端无 OnRep 机制，
		// 必须手动广播确保服务端 RoundOverlay 能看到最新存活人数
		BlasterGameState->BroadcastAliveCount();
	}

	// 归零所有玩家的回合击杀计数（新回合战斗开始）
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterPlayerState* PS = (*It)->GetPlayerState<ABlasterPlayerState>())
		{
			PS->ResetRoundKills();
		}
	}

	// 回合计时器启动：超时 → 保卫者获胜（经典爆破规则）
	CountdownTime = RoundTime;
	SetMatchState(MatchState::RoundInProgress);

	// 恢复所有玩家的战斗输入（RoundPrepare 期间被 CleanupBodiesAndRespawn 禁用）
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABlasterCharacter* Char = Cast<ABlasterCharacter>((*It)->GetPawn()))
		{
			Char->bDisableGameplayInput = false;
		}
	}
}

void ABombDefusalGameMode::OnPlayerKilled(ABlasterCharacter* DeadCharacter,
	ABlasterPlayerController* VictimController,
	ABlasterPlayerController* AttackerController)
{
	if (!VictimController || !VictimController->PlayerState) return;

	// 加分统计（保留计分逻辑，可用于后续经济系统）
	ABlasterPlayerState* AttackerPS = AttackerController
		? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
	ABlasterPlayerState* VictimPS = Cast<ABlasterPlayerState>(VictimController->PlayerState);

	if (AttackerPS && AttackerPS != VictimPS)
	{
		AttackerPS->AddToScore(1.f);
		// ── 经济系统：回合击杀计数 +1（不发钱，回合结束时统一结算）──
		AttackerPS->IncrementRoundKills();
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

	// 事件驱动递减存活计数器（O(1) 判定，直接写入 GameState 唯一权威源）
	if (!BlasterGameState) BlasterGameState = GetGameState<ABlasterGameState>();
	if (VictimPS && BlasterGameState)
	{
		if (VictimPS->TeamID == ETeamID::ETI_Attacker)
			BlasterGameState->AttackerAliveCount--;
		else if (VictimPS->TeamID == ETeamID::ETI_Defender)
			BlasterGameState->DefenderAliveCount--;

		BlasterGameState->BroadcastAliveCount();  // 委托驱动 Widget 刷新
	}

	CheckRoundEnd();
}

void ABombDefusalGameMode::CheckRoundEnd()
{
	// 仅在战斗中检查：回合结束/准备阶段不重复判定
	if (MatchState != MatchState::RoundInProgress) return;
	if (!BlasterGameState) return;

	if (BlasterGameState->AttackerAliveCount <= 0)
		EndRound(ETeamID::ETI_Defender);
	else if (BlasterGameState->DefenderAliveCount <= 0)
		EndRound(ETeamID::ETI_Attacker);
}

void ABombDefusalGameMode::EndRound(ETeamID Winner)
{
	// 存储上一回合胜者，供 HandleRoundEnd 显示
	LastRoundWinner = Winner;

	// ── 映射 Winner(ETeamID) → LogicalTeam，递增逻辑队胜场（直接写入 GameState）──
	ELogicalTeam WinningLT = GetLogicalTeamFromRole(Winner);
	if (BlasterGameState) BlasterGameState->AddRoundWin(WinningLT);

	// ── 回合经济发放（所有金钱变动的唯一入口）──
	DistributeRoundEconomy(WinningLT);

	CountdownTime = RoundEndTime;

	// 必须先推送到 GameState，再切 MatchState，确保 HandleRoundEnd 读到最新值
	SyncToGameState();
	if (BlasterGameState) BlasterGameState->BroadcastRoundResult();  // 委托驱动 Widget 刷新
	SetMatchState(MatchState::RoundEnd);
}

void ABombDefusalGameMode::CheckMatchEnd()
{
	if (!BlasterGameState) return;

	if (BlasterGameState->GetRoundWinsForTeam(ELogicalTeam::ELT_TeamA) >= RoundsToWin)
		ConcludeMatch(ELogicalTeam::ELT_TeamA);
	else if (BlasterGameState->GetRoundWinsForTeam(ELogicalTeam::ELT_TeamB) >= RoundsToWin)
		ConcludeMatch(ELogicalTeam::ELT_TeamB);
	else if (RoundNumber == BlasterGameState->HalftimeRound && !BlasterGameState->bIsSecondHalf)
	{
		CountdownTime = HalftimeSwapTime;
		SetMatchState(MatchState::HalftimeSwap);
	}
	else
		StartRoundPrepare();
}

void ABombDefusalGameMode::ConcludeMatch(ELogicalTeam Winner)
{
	// ── 映射 LogicalTeam → ETeamID（按当前半场反推）──
	// 上半场: TeamA=Attacker, TeamB=Defender
	// 下半场: TeamA=Defender, TeamB=Attacker
	// ── 写入新字段：ELogicalTeam 维度（Phase 5 新增）──
	if (BlasterGameState) BlasterGameState->LastMatchWinnerLT = Winner;

	// 旧字段：按半场反推 ETeamID（Widget 仍通过旧字段读比赛胜者）
	const bool bSecondHalf = BlasterGameState ? BlasterGameState->bIsSecondHalf : false;
	if (bSecondHalf)
	{
		LastMatchWinner = (Winner == ELogicalTeam::ELT_TeamA)
			? ETeamID::ETI_Defender : ETeamID::ETI_Attacker;
	}
	else
	{
		LastMatchWinner = (Winner == ELogicalTeam::ELT_TeamA)
			? ETeamID::ETI_Attacker : ETeamID::ETI_Defender;
	}

	CountdownTime = MatchEndTime;

	// 必须先推送到 GameState，再切 MatchState，确保 HandleMatchEnd 读到最新值
	SyncToGameState();
	if (BlasterGameState) BlasterGameState->BroadcastMatchResult();  // 委托驱动 Widget 刷新
	SetMatchState(MatchState::MatchEnd);
}

void ABombDefusalGameMode::ReturnToLobby()
{
	UWorld* World = GetWorld();
	if (World)
	{
		// 先切到 LeavingMap 防重入：ServerTravel 是帧末延迟执行的，
		// 若不切状态，Tick 会在后续帧重复调用 ReturnToLobby
		SetMatchState(MatchState::LeavingMap);
		bUseSeamlessTravel = true;
		World->ServerTravel(LobbyMapPath);
	}
}

// ========================================================================
// 复活逻辑：销毁死尸 → 重生所有玩家 → 重置存活计数
// ========================================================================
void ABombDefusalGameMode::CleanupBodiesAndRespawn()
{
	// 缓存 PlayerStart 列表（循环外获取，避免每个 PC 都查询一次）
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		ABlasterCharacter* OldCharacter = Cast<ABlasterCharacter>(PC->GetPawn());

		// 存活玩家：保留 Pawn，传送回重生点（武器/投掷物/血条/护盾自然继承）
		if (OldCharacter && !OldCharacter->IsElimmed())
		{
			if (PlayerStarts.Num() > 0)
			{
				int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
				OldCharacter->SetActorLocation(PlayerStarts[Selection]->GetActorLocation());
			}
			OldCharacter->bDisableGameplayInput = true;
			continue;
		}

		// 死亡玩家：销毁尸体 → 重生 → 发放默认武器
		if (OldCharacter)
		{
			PC->UnPossess();
			OldCharacter->Destroy();
		}

		if (PlayerStarts.Num() > 0)
		{
			int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
			RestartPlayerAtPlayerStart(PC, PlayerStarts[Selection]);
		}
		else
		{
			RestartPlayer(PC);
		}

		// 准备阶段禁止移动/战斗输入，只允许转视角和购买
		if (ABlasterCharacter* NewChar = Cast<ABlasterCharacter>(PC->GetPawn()))
		{
			NewChar->bDisableGameplayInput = true;
		}
	}

	// 重置存活计数 = 各阵营总人数（低频调用，可遍历，写入 GameState 唯一权威源）
	if (!BlasterGameState) BlasterGameState = GetGameState<ABlasterGameState>();
	if (BlasterGameState)
	{
		BlasterGameState->AttackerAliveCount = GetPlayersInTeam(ETeamID::ETI_Attacker).Num();
		BlasterGameState->DefenderAliveCount = GetPlayersInTeam(ETeamID::ETI_Defender).Num();
		BlasterGameState->BroadcastAliveCount();  // 委托驱动 Widget 刷新
	}
}

// ========================================================================
// 状态推送：服务器 MatchState 变化 → 通知所有 PlayerController
// ========================================================================
void ABombDefusalGameMode::OnMatchStateSet()
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
void ABombDefusalGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (bTeamsAssigned)
	{
		// ── 经济系统要求：比赛开始后拒绝中途加入 ──
		// 设为 ELT_None + Spectator，确保 GetActivePlayers() 不选中，Phase 3 经济遍历无污染
		if (ABlasterPlayerState* PS = NewPlayer->GetPlayerState<ABlasterPlayerState>())
		{
			PS->SetLogicalTeam(ELogicalTeam::ELT_None);
			PS->SetIsSpectator(true);
		}
	}
	// else：比赛未开始，AssignTeamsOnce 时统一分配
}

void ABombDefusalGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	HandleMidRoundLeave(Exiting);
}

void ABombDefusalGameMode::HandleMidRoundJoin(APlayerController* NewPlayer)
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

void ABombDefusalGameMode::HandleMidRoundLeave(AController* Exiting)
{
	ABlasterPlayerState* PS = Exiting->GetPlayerState<ABlasterPlayerState>();
	if (!PS) return;

	// 从存活计数中移除（如果是战斗中且该玩家还活着）
	if (MatchState == MatchState::RoundInProgress)
	{
		APawn* ExitingPawn = Exiting->GetPawn();
		ABlasterCharacter* ExitingChar = Cast<ABlasterCharacter>(ExitingPawn);
		// 仅在未死亡时递减：已死亡的玩家已经在 OnPlayerKilled 中递减过了
		if (ExitingChar && !ExitingChar->IsElimmed() && BlasterGameState)
		{
			if (PS->TeamID == ETeamID::ETI_Attacker)
				BlasterGameState->AttackerAliveCount--;
			else if (PS->TeamID == ETeamID::ETI_Defender)
				BlasterGameState->DefenderAliveCount--;

			BlasterGameState->BroadcastAliveCount();  // 委托驱动 Widget 刷新
		}
		CheckRoundEnd();
	}
}

// ========================================================================
// 将 CountdownTime / 回合信息推送到 GameState（服务器执行）
// 客户端通过 GetGameState<ABlasterGameState>() 读取，无需依赖 GameMode
// ========================================================================
void ABombDefusalGameMode::SyncToGameState()
{
	if (!BlasterGameState)
		BlasterGameState = GetGameState<ABlasterGameState>();
	if (!BlasterGameState) return;

	BlasterGameState->CurrentRoundNumber = RoundNumber;
	BlasterGameState->LastRoundWinner = LastRoundWinner;
	BlasterGameState->LastMatchWinner = LastMatchWinner;
}

// ========================================================================
// 辅助函数
// ========================================================================
TArray<ABlasterPlayerState*> ABombDefusalGameMode::GetPlayersInTeam(ETeamID Team) const
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

TArray<ABlasterPlayerState*> ABombDefusalGameMode::GetActivePlayers() const
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

// ========================================================================
// 经济系统辅助函数
// ========================================================================

// ── 角色 → 逻辑队伍映射 ──
// 上半场: Attacker=TeamA, Defender=TeamB
// 下半场: Attacker=TeamB, Defender=TeamA（角色翻转后）
ELogicalTeam ABombDefusalGameMode::GetLogicalTeamFromRole(ETeamID TeamRole) const
{
	const bool bSecondHalf = BlasterGameState ? BlasterGameState->bIsSecondHalf : false;
	if (!bSecondHalf)
	{
		if (TeamRole == ETeamID::ETI_Attacker) return ELogicalTeam::ELT_TeamA;
		if (TeamRole == ETeamID::ETI_Defender) return ELogicalTeam::ELT_TeamB;
	}
	else
	{
		if (TeamRole == ETeamID::ETI_Attacker) return ELogicalTeam::ELT_TeamB;
		if (TeamRole == ETeamID::ETI_Defender) return ELogicalTeam::ELT_TeamA;
	}
	return ELogicalTeam::ELT_None;
}

// ── 按 LogicalTeam 筛选玩家 ──
TArray<ABlasterPlayerState*> ABombDefusalGameMode::GetPlayersInLogicalTeam(ELogicalTeam LT) const
{
	TArray<ABlasterPlayerState*> Result;
	if (!GameState) return Result;

	for (APlayerState* PS : GameState->PlayerArray)
	{
		ABlasterPlayerState* BPS = Cast<ABlasterPlayerState>(PS);
		if (BPS && BPS->LogicalTeam == LT && !BPS->IsSpectator())
		{
			Result.Add(BPS);
		}
	}
	return Result;
}

// ── 回合经济发放（EndRound 末尾调用，所有金钱变动的唯一入口）──
void ABombDefusalGameMode::DistributeRoundEconomy(ELogicalTeam WinningLT)
{
	if (!BlasterGameState || !BlasterGameState->EconomyConfig) return;

	UEconomyConfig* Config = BlasterGameState->EconomyConfig;

	// ① 确定败方
	ELogicalTeam LosingLT = (WinningLT == ELogicalTeam::ELT_TeamA)
		? ELogicalTeam::ELT_TeamB : ELogicalTeam::ELT_TeamA;

	// ② ⚠ 快照胜方连败次数 —— 必须在归零前！
	int32 SnapshotLoss = BlasterGameState->GetLossStreakForTeam(WinningLT);

	// ③ 更新计数器（直接写入 GameState）
	BlasterGameState->ResetLossStreak(WinningLT);
	BlasterGameState->IncrementWinStreak(WinningLT);
	BlasterGameState->ResetWinStreak(LosingLT);
	BlasterGameState->IncrementLossStreak(LosingLT);

	// ④ 计算胜方奖励
	int32 LossBonus = Config->GetLossBonus(SnapshotLoss);
	int32 WinStreak = BlasterGameState->GetWinStreakForTeam(WinningLT);
	int32 BaseReward = (WinStreak >= Config->WinStreakThreshold)
		? Config->WinStreakPenalty : Config->WinBaseReward;

	// ⑤ 发放：胜方每人 BaseReward + LossBonus + KillReward×RoundKills
	for (ABlasterPlayerState* PS : GetPlayersInLogicalTeam(WinningLT))
	{
		PS->AddMoney(BaseReward + LossBonus);
		PS->AddMoney(PS->GetRoundKills() * Config->KillReward);
		PS->ResetRoundKills();
	}

	// ⑥ 发放：败方每人 LossParticipation + KillReward×RoundKills
	for (ABlasterPlayerState* PS : GetPlayersInLogicalTeam(LosingLT))
	{
		PS->AddMoney(Config->LossParticipation);
		PS->AddMoney(PS->GetRoundKills() * Config->KillReward);
		PS->ResetRoundKills();
	}

	UE_LOG(LogTemp, Log, TEXT("[Economy] Round distributed: Winner=%s | Base=%d + LossBonus=%d"),
		WinningLT == ELogicalTeam::ELT_TeamA ? TEXT("TeamA") : TEXT("TeamB"),
		BaseReward, LossBonus);
}

// ── 半场交换（MR12：第 12 局结束后执行）──
void ABombDefusalGameMode::ExecuteHalftimeSwap()
{
	if (!BlasterGameState || !BlasterGameState->EconomyConfig) return;

	UEconomyConfig* Config = BlasterGameState->EconomyConfig;

	// ① 标记下半场（写入 GameState，必须在角色翻转之前）
	BlasterGameState->bIsSecondHalf = true;

	// ②+④ 翻转 ETeamID + 经济重置
	for (APlayerState* PS : GameState->PlayerArray)
	{
		ABlasterPlayerState* BPS = Cast<ABlasterPlayerState>(PS);
		if (!BPS || BPS->IsSpectator()) continue;

		if (BPS->TeamID == ETeamID::ETI_Attacker)
			BPS->SetTeamID(ETeamID::ETI_Defender);
		else if (BPS->TeamID == ETeamID::ETI_Defender)
			BPS->SetTeamID(ETeamID::ETI_Attacker);

		BPS->Money = Config->StartingMoney;
		BPS->OnMoneyChanged.Broadcast(BPS->Money, 0);
	}

	// ③ 旧字段交换 —— Phase 5 删除！新字段是 LogicalTeam 维度不需要交换

	// ⑤ 连胜/连败归零（比分不重置）
	BlasterGameState->ResetAllStreaks();

	// [NEW] Step 7: 半场交换物品重置
	// A — 清除所有 Buff
	ClearAllBuffsOnAllPlayers();

	// B+C — 清除投掷物 + 发放默认武器（EquipWeapon 内部自动 Drop 旧武器到地面）
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get());
		if (!PC) continue;

		ABlasterCharacter* Character = Cast<ABlasterCharacter>(PC->GetPawn());
		if (!Character) continue;

		// B — 清除投掷物
		if (Character->GetThrowable())
		{
			Character->GetThrowable()->ClearAllThrowables();
		}

		// C — 发放默认武器（EquipWeapon → DropEquippedWeapon 自动将旧武器变为 Dropped）
		//     不主动 Destroy：否则 SpawDefaultWeapon → EquipWeapon → DropEquippedWeapon 访问野指针
		Character->SpawDefaultWeapon();
	}

	// D — 清理地面武器（销毁 B/C 步骤中 Drop 到地面的旧武器，兜底）
	CleanupDroppedWeapons();

	SyncToGameState();
	if (BlasterGameState) BlasterGameState->BroadcastRoundInfo();

	UE_LOG(LogTemp, Log, TEXT("[Economy] HalftimeSwap executed | Score: TeamA=%d TeamB=%d | All Money=$%d"),
		BlasterGameState->TeamARoundWins, BlasterGameState->TeamBRoundWins, Config->StartingMoney);
}

// ================================================================
// 购买系统：物品分发
// ================================================================

void ABombDefusalGameMode::ProcessPurchase(ABlasterPlayerController* PC,
                                            const FShopItemRow& ItemRow)
{
    ABlasterCharacter* Character = Cast<ABlasterCharacter>(PC->GetPawn());
    if (!Character) return;

    switch (ItemRow.Category)
    {
    case EShopItemCategory::ESIC_Weapon:
        SpawnAndEquipPurchasedWeapon(Character, ItemRow.WeaponClass);
        break;

    case EShopItemCategory::ESIC_Ammo:
        GrantAmmoToEquippedWeapon(Character, ItemRow.AmmoWeaponType, ItemRow.AmmoAmount);
        break;

    case EShopItemCategory::ESIC_Throwable:
        AddThrowableToInventory(Character, ItemRow.ThrowableType);
        break;

    case EShopItemCategory::ESIC_Buff:
        ApplyBuffToCharacter(Character, ItemRow.BuffType);
        break;

    default:
        break;
    }
}

// ── 武器：Spawn + 设置初始弹药 + 装备 ──
void ABombDefusalGameMode::SpawnAndEquipPurchasedWeapon(
    ABlasterCharacter* Character, TSubclassOf<AWeapon> WeaponClass)
{
    if (!WeaponClass) return;

    UWorld* World = GetWorld();
    if (!World) return;

    // [1] 服务端生成武器 Actor
    AWeapon* Weapon = World->SpawnActor<AWeapon>(WeaponClass);
    if (!Weapon) return;

    // [2] 设置初始弹药：满弹匣 + 1 倍弹匣容量备弹
    Weapon->SetInitialAmmo(Weapon->GetMagCapacity(), Weapon->GetMagCapacity());

    // [3] 标记为非默认武器：死亡时掉落而非销毁
    Weapon->bDestroyWeapon = false;

    // [4] 装备武器（内部自动 DropEquippedWeapon 扔掉旧武器）
    Character->GetCombat()->EquipWeapon(Weapon);

    UE_LOG(LogTemp, Log, TEXT("[Shop] %s equipped purchased weapon: %s (Ammo=%d, Spare=%d)"),
        *Character->GetName(), *GetNameSafe(Weapon), Weapon->GetAmmo(), Weapon->GetSpareAmmo());
}

// ── 弹药：给手持武器补充备弹 ──
void ABombDefusalGameMode::GrantAmmoToEquippedWeapon(
    ABlasterCharacter* Character, EWeaponType AmmoWeaponType, int32 Amount)
{
    // [1] 获取手持武器
    AWeapon* EquippedWeapon = Character->GetEquippedWeapon();
    if (!EquippedWeapon) return;  // 无武器，拒绝

    // [2] 类型匹配校验
    if (EquippedWeapon->GetWeaponType() != AmmoWeaponType) return;

    // [3] 备弹已满校验
    if (EquippedWeapon->GetSpareAmmo() >= EquippedWeapon->GetMaxSpareAmmo()) return;

    // [4] 补充备弹（AddToSpare 内部已做 Clamp）
    EquippedWeapon->AddToSpare(Amount);

    UE_LOG(LogTemp, Log, TEXT("[Shop] %s granted %d ammo to %s (Spare=%d)"),
        *Character->GetName(), Amount, *GetNameSafe(EquippedWeapon),
        EquippedWeapon->GetSpareAmmo());
}

// ── 投掷物：增加库存 ──
void ABombDefusalGameMode::AddThrowableToInventory(
    ABlasterCharacter* Character, EThrowableType ThrowableType)
{
    UThrowableComponent* ThrowComp = Character->GetThrowable();
    if (!ThrowComp) return;

    // [1] 获取上限（从 EconomyConfig）
    ABlasterGameState* GS = GetGameState<ABlasterGameState>();
    if (!GS || !GS->EconomyConfig) return;

    int32 MaxCount = 0;
    switch (ThrowableType)
    {
    case EThrowableType::ETT_FragGrenade:
        MaxCount = GS->EconomyConfig->MaxFragGrenadeCount;
        break;
    case EThrowableType::ETT_Flashbang:
        MaxCount = GS->EconomyConfig->MaxFlashbangCount;
        break;
    case EThrowableType::ETT_SmokeGrenade:
        MaxCount = GS->EconomyConfig->MaxSmokeGrenadeCount;
        break;
    default:
        return;
    }

    // [2] 校验上限
    if (!ThrowComp->CanAddThrowable(ThrowableType, MaxCount)) return;

    // [3] 增加计数
    ThrowComp->AddThrowable(ThrowableType, 1);

    UE_LOG(LogTemp, Log, TEXT("[Shop] %s purchased throwable %d, now has %d"),
        *Character->GetName(), (int32)ThrowableType,
        ThrowComp->GetCount(ThrowableType));
}

// ── Buff：立即应用效果 ──
void ABombDefusalGameMode::ApplyBuffToCharacter(
    ABlasterCharacter* Character, EBuffType BuffType)
{
    UBuffComponent* BuffComp = Character->GetBuff();
    if (!BuffComp) return;

    switch (BuffType)
    {
    case EBuffType::EBT_Speed:
        BuffComp->BuffSpeed(1600.f, 850.f, 30.f);
        break;

    case EBuffType::EBT_Jump:
        BuffComp->BuffJump(4000.f, 30.f);
        break;

    case EBuffType::EBT_Shield:
        BuffComp->ReplenishShield(100.f, 5.f);
        break;

    case EBuffType::EBT_Heal:
        BuffComp->BuffHeal(100.f);  // 与 HealthPickup::HealAmount 调平
        break;

    default:
        break;
    }

    UE_LOG(LogTemp, Log, TEXT("[Shop] %s applied buff %d"),
        *Character->GetName(), (int32)BuffType);
}

// ================================================================
// 回合清理
// ================================================================

void ABombDefusalGameMode::CleanupDroppedWeapons()
{
    // 遍历世界中所有 AWeapon Actor，销毁 Dropped 状态的（地面遗留武器）
    for (TActorIterator<AWeapon> It(GetWorld()); It; ++It)
    {
        AWeapon* Weapon = *It;
        if (Weapon && Weapon->GetWeaponState() == EWeaponState::EWS_Dropped)
        {
            Weapon->Destroy();
        }
    }
}

void ABombDefusalGameMode::ClearAllBuffsOnAllPlayers()
{
    // 遍历所有 PlayerController，对存活角色清除 Buff
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(It->Get());
        if (!PC) continue;

        ABlasterCharacter* Character = Cast<ABlasterCharacter>(PC->GetPawn());
        if (Character && Character->GetBuff())
        {
            Character->GetBuff()->ClearAllBuffs();
        }
    }
}
