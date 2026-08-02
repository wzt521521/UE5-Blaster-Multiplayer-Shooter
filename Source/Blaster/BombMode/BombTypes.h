#pragma once

#include "CoreMinimal.h"
#include "BombTypes.generated.h"

// 炸弹生命周期状态，由 ABombActor 服务器权威维护，复制到所有客户端
UENUM(BlueprintType)
enum class EBombState : uint8
{
	EBS_Idle        UMETA(DisplayName = "Idle"),       // 未分配，等待 GameMode 分配
	EBS_Carried     UMETA(DisplayName = "Carried"),    // 被攻方携带中
	EBS_Planted     UMETA(DisplayName = "Planted"),    // 已安放，倒计时中
	EBS_Exploded    UMETA(DisplayName = "Exploded"),   // 爆炸（攻方胜）
	EBS_Defused     UMETA(DisplayName = "Defused")     // 被拆除（守方胜）
};

// 安包/拆包交互类型，UBombInteractionComponent 根据当前目标和角色身份自动判定
UENUM(BlueprintType)
enum class EBombInteractionType : uint8
{
	EBIT_None       UMETA(DisplayName = "None"),
	EBIT_Planting   UMETA(DisplayName = "Planting"),   // 正在安包
	EBIT_Defusing   UMETA(DisplayName = "Defusing")    // 正在拆包
};
