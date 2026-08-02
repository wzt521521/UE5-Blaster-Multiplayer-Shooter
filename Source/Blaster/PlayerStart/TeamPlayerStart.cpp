// ──────────────────────────────────────────────────
// ATeamPlayerStart / AAttackerPlayerStart / ADefenderPlayerStart 实现
// ──────────────────────────────────────────────────

#include "TeamPlayerStart.h"

// ── AAttackerPlayerStart：构造函数硬设 Team = ETI_Attacker ──
AAttackerPlayerStart::AAttackerPlayerStart()
{
    Team = ETeamID::ETI_Attacker;
}

// ── ADefenderPlayerStart：构造函数硬设 Team = ETI_Defender ──
ADefenderPlayerStart::ADefenderPlayerStart()
{
    Team = ETeamID::ETI_Defender;
}
