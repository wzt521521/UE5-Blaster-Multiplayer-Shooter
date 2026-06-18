// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerState.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Net/UnrealNetwork.h"

void ABlasterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABlasterPlayerState, Defeats);
}

void ABlasterPlayerState::AddToScore(float ScoreAmount)
{
    // ————————————————————————————————————————————
    // 加分链路（服务器执行）：Score 是 APlayerState 自带的复制变量，
    // 修改后自动同步到所有客户端，同时把新分数推到 HUD
    // ————————————————————————————————————————————
    SetScore(GetScore() + ScoreAmount);

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