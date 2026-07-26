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
    // 修改后自动同步到所有客户端，同时把新分数推到 HUD
    // ————————————————————————————————————————————
    SetScore(GetScore() + ScoreAmount);

    // 每次分数变动后更新 GameState 的最高分排行榜，确保冷却阶段能正确显示胜者
    if (ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>())
    {
        GS->UpdateTopScore(this);
    }

    // 懒加载缓存：第一次调用时从 Pawn → Controller 拿到引用，后续复用
    Character = Character == NULL ? Cast<ABlasterCharacter>(GetPawn()) : Character;
    if(Character)
    {
        Controller = Controller == NULL ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
        if(Controller)
        {
            Controller->SetHUDScore(GetScore());
        }
    }
}

void ABlasterPlayerState::OnRep_Score()
{
    Super::OnRep_Score();

    // ————————————————————————————————————————————
    // 加分链路（客户端）：OnRep_Score 在 Score 被复制到客户端时触发，
    // 负责把服务器同步过来的新分数更新到 HUD
    // ————————————————————————————————————————————
    Character = Character == NULL ? Cast<ABlasterCharacter>(GetPawn()) : Character;
    if(Character)
    {
        Controller = Controller == NULL ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
        if(Controller)
        {
            Controller->SetHUDScore(GetScore());
        }
    }
}

void ABlasterPlayerState::AddToDefeats(int32 DefeatsAmount)
{
    // ————————————————————————————————————————————
    // 败场链路（服务器执行）：Defeats 是需要手动复制的变量，
    // 修改后通过 OnRep_Defeats 同步到客户端 HUD
    // ————————————————————————————————————————————
    Defeats += DefeatsAmount;

    Character = Character == NULL ? Cast<ABlasterCharacter>(GetPawn()) : Character;
    if (Character)
    {
        Controller = Controller == NULL ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
        if (Controller)
        {
            Controller->SetHUDDefeats(Defeats);
        }
    }
}

void ABlasterPlayerState::OnRep_Defeats()
{
    // ————————————————————————————————————————————
    // 败场链路（客户端）：Defeats 被复制到客户端时触发，
    // 负责把服务器同步过来的败场数更新到 HUD
    // ————————————————————————————————————————————
    Character = Character == NULL ? Cast<ABlasterCharacter>(GetPawn()) : Character;
    if (Character)
    {
        Controller = Controller == NULL ? Cast<ABlasterPlayerController>(Character->Controller) : Controller;
        if (Controller)
        {
            Controller->SetHUDDefeats(Defeats);
        }
    }
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