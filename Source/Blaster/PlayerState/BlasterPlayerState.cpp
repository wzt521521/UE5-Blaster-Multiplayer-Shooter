// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Net/UnrealNetwork.h"

void ABlasterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABlasterPlayerState, Defeats);
    DOREPLIFETIME(ABlasterPlayerState, TeamID);
}

void ABlasterPlayerState::AddToScore(float ScoreAmount)
{
    // ————————————————————————————————————————————
    // 加分链路（服务器执行）：Score 是 APlayerState 自带的复制变量，
    // 修改后自动同步到所有客户端
    // 注意：击杀/死亡数据仅记录，不再推送到 CharacterOverlay 显示
    // ————————————————————————————————————————————
    SetScore(GetScore() + ScoreAmount);

    // 每次分数变动后更新 GameState 的最高分排行榜，确保冷却阶段能正确显示胜者
    if (ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>())
    {
        GS->UpdateTopScore(this);
    }
}

void ABlasterPlayerState::OnRep_Score()
{
    Super::OnRep_Score();
    // Score 数据已复制到客户端，仅记录不再推送到 HUD
}

void ABlasterPlayerState::AddToDefeats(int32 DefeatsAmount)
{
    // ————————————————————————————————————————————
    // 败场链路（服务器执行）：Defeats 是需要手动复制的变量，
    // 修改后通过 OnRep_Defeats 同步到客户端
    // 注意：死亡数据仅记录，不再推送到 CharacterOverlay 显示
    // ————————————————————————————————————————————
    Defeats += DefeatsAmount;
}

void ABlasterPlayerState::OnRep_Defeats()
{
    // Defeats 数据已复制到客户端，仅记录不再推送到 HUD
}

void ABlasterPlayerState::SetTeamID(ETeamID NewTeamID)
{
    // 服务器权威：更新 TeamID，引擎自动复制到客户端
    TeamID = NewTeamID;
    // 非服务器环境（如 Listen Server 的客户端 PS）立即走 OnRep 通知逻辑
    if (!HasAuthority())
    {
        OnRep_TeamID();
    }
}

void ABlasterPlayerState::OnRep_TeamID()
{
    // 客户端收到 TeamID 复制后 → 通知自身 Controller 刷新 HUD 阵营标识
    // OverheadWidget 在下一帧 Tick 中通过 ShowPlayerTeamRole 读取新 TeamID
    Character = Character == NULL ? Cast<ABlasterCharacter>(GetPawn()) : Character;
    if (Character)
    {
        Controller = Controller == NULL ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
        if (Controller)
        {
            // 阵营变化时刷新 HUD：RoundOverlay 中的阵营标签
            // 具体 HUD 更新逻辑在 PlayerController 的 HandleAssignTeams 中处理
        }
    }
}