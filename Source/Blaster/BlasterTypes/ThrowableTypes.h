#pragma once

// 投掷物类型 — 不同手雷的行为由 AThrowableProjectile 子类区分
UENUM(BlueprintType)
enum class EThrowableType : uint8
{
	ETT_None          UMETA(DisplayName = "None"),           // 占位：TMap 默认值不能与有效键冲突
	ETT_FragGrenade   UMETA(DisplayName = "Frag Grenade"),
	ETT_Flashbang     UMETA(DisplayName = "Flashbang"),
	ETT_SmokeGrenade  UMETA(DisplayName = "Smoke Grenade"),

	ETT_MAX           UMETA(DisplayName = "DefaultMAX")
};

// 投掷组件状态机 — 控制输入可打断性 & 与武器系统的互斥
UENUM(BlueprintType)
enum class EThrowableState : uint8
{
	ETS_Idle      UMETA(DisplayName = "Idle"),       // 空闲：可接收输入
	ETS_Cooking   UMETA(DisplayName = "Cooking"),     // 蓄力中：按住键，显示投掷轨迹
	ETS_Throwing  UMETA(DisplayName = "Throwing"),    // 投掷动画播放中，不可打断

	ETS_MAX       UMETA(DisplayName = "DefaultMAX")
};
