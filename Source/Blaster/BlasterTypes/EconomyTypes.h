// ──────────────────────────────────────────────────
// Blaster 经济系统 — 基础类型定义
// ──────────────────────────────────────────────────
// 包含:
//   ELogicalTeam      — 逻辑队伍标识（比赛分配，半场不变）
//   FOnMoneyChanged   — Money 变化委托（驱动 Widget 刷新）

#pragma once

#include "CoreMinimal.h"
#include "EconomyTypes.generated.h"


// ──────────────────────────────────────────────────
// ELogicalTeam：逻辑队伍枚举
// ──────────────────────────────────────────────────
// 比赛开始时（AssignTeamsOnce）一次性分配，半场交换后不变。
// 与 ETeamID（Attacker/Defender，会因半场而翻转）是正交维度。
// 连胜/连败计数器、回合胜场归属此枚举。
UENUM(BlueprintType)
enum class ELogicalTeam : uint8
{
    ELT_None   UMETA(DisplayName = "None"),
    ELT_TeamA  UMETA(DisplayName = "Team A"),
    ELT_TeamB  UMETA(DisplayName = "Team B")
};


// ──────────────────────────────────────────────────
// FOnMoneyChanged：动态多播委托
// ──────────────────────────────────────────────────
// 参数:
//   NewMoney — 变化后的金额
//   Delta    — 变动量（正值=加钱，负值=扣钱）
// BlueprintAssignable → 蓝图可直接绑定，驱动 BuyMenu/HUD 刷新
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnMoneyChanged,
    int32, NewMoney,
    int32, Delta
);
