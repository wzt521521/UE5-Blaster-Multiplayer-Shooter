// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BlasterGameMode.generated.h"

namespace MatchState
{
	extern BLASTER_API const FName Cooldown;
	// 回合制阵营模式扩展：一次性阵营分配（瞬间过渡）
	extern BLASTER_API const FName AssignTeams;
	// 回合准备倒计时
	extern BLASTER_API const FName RoundPrepare;
	// 战斗中
	extern BLASTER_API const FName RoundInProgress;
	// 回合结果播报
	extern BLASTER_API const FName RoundEnd;
	// 比赛结果 → 返回大厅
	extern BLASTER_API const FName MatchEnd;
}

class ABlasterCharacter;
class ABlasterPlayerController;
class ACharacter;
class AController;

UCLASS()
class BLASTER_API ABlasterGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	ABlasterGameMode();
	virtual void Tick(float DeltaTime) override;
	virtual void PlayerEliminated(
		ABlasterCharacter* EliminatedCharacter,
		ABlasterPlayerController* VictimController,
		ABlasterPlayerController* AttackerController
	);
	virtual void RequestRespawn(ACharacter* EliminatedCharacter, AController* EliminatedController);

	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 10.f;

	UPROPERTY(EditDefaultsOnly)
	float MatchTime = 120.f;

	UPROPERTY(EditDefaultsOnly)
	float CooldownTime = 10.f;

	float LevelStartingTime = 0.f;

	FORCEINLINE float GetCountdownTime() const { return CountdownTime; }

protected:
	virtual void BeginPlay() override;
	virtual void OnMatchStateSet() override;

private:
	float CountdownTime = 0.f;
};
