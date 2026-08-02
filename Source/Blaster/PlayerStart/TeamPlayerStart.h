// ──────────────────────────────────────────────────
// Blaster 生殖系统 — 阵营出生点层级
// ──────────────────────────────────────────────────
// 继承链:
//   APlayerStart
//     └── ATeamPlayerStart (Abstract)     — 父类，定义 Team 属性
//           ├── AAttackerPlayerStart      — Team = ETI_Attacker
//           └── ADefenderPlayerStart      — Team = ETI_Defender
//
// 设计原则:
//   ① 父类 Abstract，不允许直接拖入关卡，强制使用具名子类
//   ② Team 为 BlueprintReadOnly，子类 C++ 构造函数硬设值，蓝图层不可改
//   ③ 子类纯数据（无额外逻辑），未来可各自扩展专属行为（出生特效等）
//   ④ ChoosePlayerStart 走 Cast<ATeamPlayerStart>，无需感知子类存在
//
// 依赖:
//   - TeamTypes.h（ETeamID 枚举）
//   - 被 BombDefusalGameMode 的 ChoosePlayerStart 查询

#pragma once

#include "GameFramework/PlayerStart.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "TeamPlayerStart.generated.h"


// ────────────────────────────────────────────
// ATeamPlayerStart：阵营出生点抽象基类
// ────────────────────────────────────────────
// 只定义 Team 属性的存在，不决定具体阵营值。
// Abstract → 只能拖入 AAttackerPlayerStart / ADefenderPlayerStart 到关卡
UCLASS(Abstract, BlueprintType)
class BLASTER_API ATeamPlayerStart : public APlayerStart
{
    GENERATED_BODY()

public:
    // 此出生点归属阵营，子类构造函数硬设值
    // BlueprintReadOnly → 蓝图可读不可写，防止配置错误
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawn")
    ETeamID Team = ETeamID::ETI_None;
};


// ────────────────────────────────────────────
// AAttackerPlayerStart：攻击方出生点
// ────────────────────────────────────────────
// 仅构造函数设置 Team = ETI_Attacker，无额外逻辑
UCLASS(BlueprintType)
class BLASTER_API AAttackerPlayerStart : public ATeamPlayerStart
{
    GENERATED_BODY()

public:
    AAttackerPlayerStart();
};


// ────────────────────────────────────────────
// ADefenderPlayerStart：防守方出生点
// ────────────────────────────────────────────
// 仅构造函数设置 Team = ETI_Defender，无额外逻辑
UCLASS(BlueprintType)
class BLASTER_API ADefenderPlayerStart : public ATeamPlayerStart
{
    GENERATED_BODY()

public:
    ADefenderPlayerStart();
};
