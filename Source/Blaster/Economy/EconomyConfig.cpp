// ──────────────────────────────────────────────────
// Blaster 经济系统 — 经济配置 DataAsset 实现
// ──────────────────────────────────────────────────

#include "Blaster/Economy/EconomyConfig.h"

// 计算连败加成：min(LossCount × LossBonusPerTier, LossBonusMax)
// 内部封装乘法+封顶，调用方无需访问 LossBonusPerTier/LossBonusMax
int32 UEconomyConfig::GetLossBonus(int32 LossCount) const
{
    const int32 RawBonus = LossCount * LossBonusPerTier;
    return FMath::Min(RawBonus, LossBonusMax);
}
