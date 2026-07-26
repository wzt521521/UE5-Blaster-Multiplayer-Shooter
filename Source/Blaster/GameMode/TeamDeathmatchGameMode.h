#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "TeamDeathmatchGameMode.generated.h"

class ABlasterCharacter;
class ABlasterPlayerController;
class ABlasterPlayerState;

// 回合制阵营对抗 GameMode：歼灭胜利条件（全灭对手），为爆破模式铺路
// 继承 AGameMode（非 ABlasterGameMode），回合制状态机与 Deathmatch 完全独立
UCLASS()
class BLASTER_API ATeamDeathmatchGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ATeamDeathmatchGameMode();
	virtual void Tick(float DeltaTime) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void OnMatchStateSet() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Character 健康值归零时调用 → 递减 AliveCount → CheckRoundEnd
	void OnPlayerKilled(ABlasterCharacter* DeadCharacter,
	                    ABlasterPlayerController* VictimController,
	                    ABlasterPlayerController* AttackerController);

	FORCEINLINE float GetCountdownTime() const { return CountdownTime; }
	FORCEINLINE int32 GetRoundNumber() const { return RoundNumber; }
	FORCEINLINE int32 GetAttackerRoundWins() const { return AttackerRoundWins; }
	FORCEINLINE int32 GetDefenderRoundWins() const { return DefenderRoundWins; }
	FORCEINLINE int32 GetAttackerAliveCount() const { return AttackerAliveCount; }
	FORCEINLINE int32 GetDefenderAliveCount() const { return DefenderAliveCount; }

protected:
	virtual void BeginPlay() override;

	// ---- 可配置参数 ----
	// 开局人数阈值（≥此人数自动开始比赛）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	int32 AimPeople = 2;

	// 先赢 N 局获胜
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	int32 RoundsToWin = 7;

	// 回合准备倒计时（秒）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	float RoundPrepareTime = 5.f;

	// 回合结果播报时长（秒）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	float RoundEndTime = 4.f;

	// 比赛结果播报时长（秒）
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	float MatchEndTime = 8.f;

	// 返回大厅的地图路径
	UPROPERTY(EditDefaultsOnly, Category = "Round Settings")
	FString LobbyMapPath = TEXT("/Game/Maps/Lobby");

private:
	// ---- 回合生命周期 ----
	float CountdownTime = 0.f;
	int32 AttackerRoundWins = 0;
	int32 DefenderRoundWins = 0;
	int32 RoundNumber = 0;

	// 事件驱动存活计数器：Tick 中只做 O(1) 比较，不遍历 PlayerArray
	UPROPERTY(Replicated)
	int32 AttackerAliveCount = 0;

	UPROPERTY(Replicated)
	int32 DefenderAliveCount = 0;

	// 标记阵营是否已分配（一场比赛只分配一次）
	bool bTeamsAssigned = false;

	void StartRoundPrepare();
	void AssignTeamsOnce();             // 比赛开始一次性随机分配阵营
	void StartRoundInProgress();
	void CheckRoundEnd();               // O(1) 比较存活计数器
	void EndRound(ETeamID Winner);
	void CheckMatchEnd();
	void ConcludeMatch(ETeamID Winner);
	void ReturnToLobby();

	// 中途加入/退出
	void HandleMidRoundJoin(APlayerController* NewPlayer);
	void HandleMidRoundLeave(AController* Exiting);

	// 辅助
	void CleanupBodiesAndRespawn();     // 销毁死尸 + 重生所有玩家 + 重置 AliveCount
	TArray<ABlasterPlayerState*> GetPlayersInTeam(ETeamID Team) const;
	TArray<ABlasterPlayerState*> GetActivePlayers() const;
};
