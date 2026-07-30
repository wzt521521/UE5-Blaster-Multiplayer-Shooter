#pragma once

#include "CoreMinimal.h"

// 自定义 MatchState 常量，定义在 MatchState.cpp 中
// ABombDefusalGameMode 和 ABlasterGameMode 平级引用，互不依赖
namespace MatchState
{
	extern BLASTER_API const FName Cooldown;
	extern BLASTER_API const FName AssignTeams;
	extern BLASTER_API const FName RoundPrepare;
	extern BLASTER_API const FName RoundInProgress;
	extern BLASTER_API const FName RoundEnd;
	extern BLASTER_API const FName HalftimeSwap;    // 半场交换阶段
	extern BLASTER_API const FName MatchEnd;
}
