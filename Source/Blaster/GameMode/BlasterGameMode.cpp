// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterGameMode.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "GameFramework/PlayerStart.h"
void ABlasterGameMode::PlayEliminated(ABlasterCharacter *EliminatedCharacter, ABlasterPlayerController *VictimController, ABlasterPlayerController *AttackerController)
{
    if (AttackerController == nullptr || AttackerController->PlayerState == nullptr) return;
    if (VictimController == nullptr || VictimController->PlayerState == nullptr) return;

    ABlasterPlayerState* AttackerPlayerState = AttackerController ? Cast<ABlasterPlayerState>(AttackerController->PlayerState) : nullptr;
    ABlasterPlayerState* VictimPlayerState = VictimController ? Cast<ABlasterPlayerState>(VictimController->PlayerState) : nullptr;

    if(AttackerPlayerState&&AttackerPlayerState!=VictimPlayerState)
    {
        AttackerPlayerState->AddToScore(1.f);
    }
    if (VictimPlayerState)
    {
        VictimPlayerState->AddToDefeats(1);
    }
    if (EliminatedCharacter)
    {
        EliminatedCharacter->Elim();//死亡角色播放死亡动画
    }
}

void ABlasterGameMode::RequestRespawn(ACharacter *EliminatedCharacter, AController *EliminatedController)
{
    // ① 清理旧尸体——被淘汰的角色 Actor 已播完死亡动画，现在从世界移除
    if (EliminatedCharacter)
    {
        EliminatedCharacter->Reset();       // 把 Actor 属性恢复到 CDO 默认值，防止残留状态带到下次复活
        EliminatedCharacter->Destroy();     // 销毁旧角色（会触发 Destroyed 回调做清理）
    }

    // ② 随机复活点——从地图上所有 PlayerStart 中随机选一个，让玩家重新出生
    if (EliminatedController)
    {
        TArray<AActor*> PlayerStarts;                                            // 存放地图上所有出生点
        UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);  // 遍历地图收集 PlayerStart
        int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);          // 随机选一个出生点索引
        RestartPlayerAtPlayerStart(EliminatedController, PlayerStarts[Selection]);  // 在选中的出生点创建新角色
    }
}
