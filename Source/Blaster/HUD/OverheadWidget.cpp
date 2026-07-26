// Fill out your copyright notice in the Description page of Project Settings.


#include "OverheadWidget.h"
#include "Components/TextBlock.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"

void UOverheadWidget::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	RemoveFromParent();
	Super::OnLevelRemovedFromWorld(InLevel, InWorld);
}

void UOverheadWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn)
{
	ShowPlayerTeamRole(InPawn);
}

void UOverheadWidget::ShowPlayerTeamRole(APawn* InPawn)
{
	if (InPawn == nullptr)
	{
		SetDisplayText(FString());
		return;
	}

	ABlasterPlayerState* TargetPS = InPawn->GetPlayerState<ABlasterPlayerState>();
	APlayerController* LocalPC = GetOwningPlayer();
	ABlasterPlayerState* LocalPS = LocalPC
		? LocalPC->GetPlayerState<ABlasterPlayerState>() : nullptr;

	if (!TargetPS || !LocalPS)
	{
		// 降级：无 PlayerState 时显示旧版 NetRole
		ENetRole Role = InPawn->GetLocalRole();
		FString RoleStr;
		switch (Role)
		{
		case ROLE_Authority:       RoleStr = TEXT("Authority"); break;
		case ROLE_AutonomousProxy: RoleStr = TEXT("AutonomousProxy"); break;
		case ROLE_SimulatedProxy:  RoleStr = TEXT("SimulatedProxy"); break;
		default: break;
		}
		SetDisplayText(RoleStr);
		return;
	}

	// 规则1: 自己 → 始终显示名字
	if (TargetPS == LocalPS)
	{
		SetDisplayText(TargetPS->GetPlayerName());
		return;
	}

	// 规则2: 同阵营队友 → 始终显示名字（不论死活）
	if (TargetPS->TeamID != ETeamID::ETI_None && TargetPS->TeamID == LocalPS->TeamID)
	{
		SetDisplayText(TargetPS->GetPlayerName());
		return;
	}

	// 规则3: 敌方 + 无阵营 → 隐藏名字
	SetDisplayText(FString());
}