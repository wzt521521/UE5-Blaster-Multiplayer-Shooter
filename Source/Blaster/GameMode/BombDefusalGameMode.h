#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/BlasterTypes/EconomyTypes.h"
#include "Blaster/Economy/EconomyConfig.h"
#include "BombDefusalGameMode.generated.h"

class ABlasterCharacter;
class ABlasterPlayerController;
class ABlasterPlayerState;

// 回合制阵营对抗 GameMode：歼灭胜利条件（全灭对手），后续叠加炸弹机制即为完整爆破模式
// 继承 AGameMode（非 ABlasterGameMode），回合制状态机与 Deathmatch 完全独立
UCLASS()
class BLASTER_API ABombDefusalGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ABombDefusalGameMode();
	virtual void Tick(float DeltaTime) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void OnMatchStateSet() override;

	// Character 健康值归零时调用 → 递减 AliveCount → CheckRoundEnd
	void OnPlayerKilled(ABlasterCharacter* DeadCharacter,
	                    ABlasterPlayerController* VictimController,
	                    ABlasterPlayerController* AttackerController);

	UFUNCTION(BlueprintPure)
	float GetCountdownTime() const { return CountdownTime; }
	UFUNCTION(BlueprintPure)
	int32 GetRoundNumber() const { return RoundNumber; }
	UFUNCTION(BlueprintPure)
	int32 GetAttackerRoundWins() const
	{
		const ABlasterGameState* GS = GetGameState<ABlasterGameState>();
		return GS ? GS->TeamARoundWins : 0;
	}
	UFUNCTION(BlueprintPure)
	int32 GetDefenderRoundWins() const
	{
		const ABlasterGameState* GS = GetGameState<ABlasterGameState>();
		return GS ? GS->TeamBRoundWins : 0;
	}
	UFUNCTION(BlueprintPure)
	int32 GetAttackerAliveCount() const
	{
		const ABlasterGameState* GS = GetGameState<ABlasterGameState>();
		return GS ? GS->AttackerAliveCount : 0;
	}
	UFUNCTION(BlueprintPure)
	int32 GetDefenderAliveCount() const
	{
		const ABlasterGameState* GS = GetGameState<ABlasterGameState>();
		return GS ? GS->DefenderAliveCount : 0;
	}
	UFUNCTION(BlueprintPure)
	ETeamID GetLastRoundWinner() const { return LastRoundWinner; }
	UFUNCTION(BlueprintPure)
	ETeamID GetLastMatchWinner() const { return LastMatchWinner; }

protected:
	virtual void BeginPlay() override;

	// ---- 可配置参数 ----
	// 开局人数阈值（≥此人数自动开始比赛）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	int32 AimPeople = 2;

	// 先赢 N 局获胜
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	int32 RoundsToWin = 13;

	// 回合准备倒计时（秒）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	float RoundPrepareTime = 5.f;

	// 回合结果播报时长（秒）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	float RoundEndTime = 4.f;

	// 回合战斗时长（秒），超时保卫者获胜
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	float RoundTime = 120.f;

	// 比赛结果播报时长（秒）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	float MatchEndTime = 8.f;

	// 返回大厅的地图路径
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	FString LobbyMapPath = TEXT("/Game/Maps/Lobby");

	// ── 半场交换配置 ──
	// 半场交换回合数（MR12: 第 12 局结束后交换）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	int32 HalftimeRound = 12;

	// 半场交换展示时长（秒）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	float HalftimeSwapTime = 5.f;

	// ── 经济系统配置 ──
	// 指向 DA_EconomyConfig DataAsset，BeginPlay 时加载
	UPROPERTY(EditDefaultsOnly, Category = "Economy")
	TSoftObjectPtr<UEconomyConfig> EconomyConfigRef;

private:
	// ---- 回合生命周期 ----
	float CountdownTime = 0.f;
	int32 RoundNumber = 0;

	// GameState 缓存：BeginPlay 时赋值，后续所有 AliveCount 读写直接走 GameState
	UPROPERTY()
	class ABlasterGameState* BlasterGameState = nullptr;

	// 标记阵营是否已分配（一场比赛只分配一次）
	bool bTeamsAssigned = false;

	// 上一回合胜者（HandleRoundEnd 显示用）
	ETeamID LastRoundWinner = ETeamID::ETI_None;
	// 比赛最终胜者（HandleMatchEnd 显示用）
	ETeamID LastMatchWinner = ETeamID::ETI_None;

	// ── 经济配置软引用（服务端，BeginPlay 中加载并写入 GameState）──
	// 运行时指针已迁移到 GameState->EconomyConfig

	void StartRoundPrepare();
	void AssignTeamsOnce();             // 比赛开始一次性随机分配阵营
	void StartRoundInProgress();
	void CheckRoundEnd();               // O(1) 比较存活计数器
	void EndRound(ETeamID Winner);
	void CheckMatchEnd();
	void ConcludeMatch(ELogicalTeam Winner);
	void ReturnToLobby();

	// 中途加入/退出
	void HandleMidRoundJoin(APlayerController* NewPlayer);
	void HandleMidRoundLeave(AController* Exiting);

	// 辅助
	void CleanupBodiesAndRespawn();     // 销毁死尸 + 重生所有玩家 + 重置 AliveCount
	TArray<ABlasterPlayerState*> GetPlayersInTeam(ETeamID Team) const;
	TArray<ABlasterPlayerState*> GetActivePlayers() const;

	// 将 CountdownTime / 回合信息 推送到 GameState，客户端通过 GameState 读取
	void SyncToGameState();

	// ── 经济系统辅助（Phase 3）──
	ELogicalTeam GetLogicalTeamFromRole(ETeamID TeamRole) const;       // 角色 → 逻辑队伍映射
	TArray<ABlasterPlayerState*> GetPlayersInLogicalTeam(ELogicalTeam LT) const; // 按逻辑队筛选玩家
	void DistributeRoundEconomy(ELogicalTeam WinningLT);           // 回合经济发放（统一发钱入口）
	void ExecuteHalftimeSwap();                                     // 半场交换执行（6 步顺序）
};
