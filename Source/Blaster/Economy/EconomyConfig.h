// ──────────────────────────────────────────────────
// Blaster 经济系统 — 经济配置 DataAsset
// ──────────────────────────────────────────────────
// UEconomyConfig 集中管理所有经济常量，蓝图可直接编辑。
// 设计原则:
//   ① 只存只读常量，不存运行时状态
//   ② 运行时经济状态存在 PlayerState（Money/RoundKills）
//      和 GameState（连胜/连败/胜场）
//   ③ GetLossBonus() 封装连败加成计算
//      调用方无需知道内部常量值

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EconomyConfig.generated.h"


UCLASS(BlueprintType)
class BLASTER_API UEconomyConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    // ────────────────────────────────────────────
    // 查询方法
    // ────────────────────────────────────────────

    // 计算连败加成金额（LossCount × LossBonusPerTier，封顶 LossBonusMax）
    // 示例: LossCount=3 → 150; LossCount=4 → 200; LossCount=5 → 200
    UFUNCTION(BlueprintPure, Category = "Economy|LossStreak")
    int32 GetLossBonus(int32 LossCount) const;

    // ────────────────────────────────────────────
    // 通用配置
    // ────────────────────────────────────────────

    // 每人初始金额
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|General")
    int32 StartingMoney = 200;

    // 金钱上限，-1 = 无上限。AddMoney 中判断此值：-1 直接加，>=0 裁剪
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|General",
        meta = (ClampMin = "-1", ToolTip = "金钱上限，-1 表示无上限"))
    int32 MaxMoney = -1;

    // ────────────────────────────────────────────
    // 回合奖励配置
    // ────────────────────────────────────────────

    // 胜方每人基础奖励
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|RoundRewards")
    int32 WinBaseReward = 200;

    // 连胜达到 WinStreakThreshold 后胜方每人降为此金额
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|RoundRewards")
    int32 WinStreakPenalty = 100;

    // 触发惩罚的连胜局数
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|RoundRewards")
    int32 WinStreakThreshold = 3;

    // 失败方每人每回合的保底补偿
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|RoundRewards")
    int32 LossParticipation = 50;

    // ────────────────────────────────────────────
    // 连败加成配置
    // ────────────────────────────────────────────

    // 每连败 1 局叠加金额，在下次获胜时一次性释放（消耗后归零）
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|LossStreak")
    int32 LossBonusPerTier = 50;

    // 连败加成封顶值（不随连败局数继续增长）
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|LossStreak")
    int32 LossBonusMax = 200;

    // ────────────────────────────────────────────
    // 击杀奖励
    // ────────────────────────────────────────────

    // 每次击杀统一奖励，不分武器类型，回合结束时按 RoundKills 结算
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|Kill")
    int32 KillReward = 50;

    // ────────────────────────────────────────────
    // 炸弹事件收益（预留接口，后续叠加炸弹机制时启用）
    // ────────────────────────────────────────────

    // 进攻方成功引爆炸弹，全队每人加成
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|Bomb")
    int32 BombDetonationBonus = 50;

    // 防守方成功拆除炸弹，全队每人加成
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy|Bomb")
    int32 BombDefusalBonus = 50;
};
