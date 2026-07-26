#pragma once

#include "CoreMinimal.h"
#include "TeamTypes.generated.h"

// 阵营标识：攻击者/保卫者，用于回合制对抗模式
UENUM(BlueprintType)
enum class ETeamID : uint8
{
	ETI_None        UMETA(DisplayName = "None"),
	ETI_Attacker    UMETA(DisplayName = "Attacker"),
	ETI_Defender    UMETA(DisplayName = "Defender")
};
