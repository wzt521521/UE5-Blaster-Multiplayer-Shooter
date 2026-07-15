#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blaster/BlasterTypes/ThrowableTypes.h"
#include "ThrowableComponent.generated.h"

class AThrowableProjectile;
class ABlasterCharacter;
class UAnimMontage;

// 投掷物组件 — 管理手雷的装备/蓄力/投掷/冷却全流程
// 权威模型：状态和数量由服务器裁决，客户端做乐观预测
// 解耦设计：不关心投掷物具体效果，只负责生命周期管理
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BLASTER_API UThrowableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThrowableComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	friend class ABlasterCharacter;

	// ===== 输入接口（由 ABlasterCharacter 按键绑定调用）=====
	void StartCooking();
	void ExecuteThrow();
	void CancelCooking();

	// ===== 装备/卸载（由选择面板调用）=====
	void EquipThrowable(EThrowableType Type);   // 选择面板确认后调用
	void UnequipThrowable();                    // 切武器/扔完后调用

	// ===== 切换/查询 =====
	void SelectThrowable(EThrowableType Type);
	FORCEINLINE bool IsIdle()              const { return ThrowState == EThrowableState::ETS_Idle; }
	FORCEINLINE bool IsCooking()           const { return ThrowState == EThrowableState::ETS_Cooking; }
	FORCEINLINE bool IsThrowing()          const { return ThrowState == EThrowableState::ETS_Throwing; }
	FORCEINLINE bool IsThrowableEquipped() const { return bThrowableEquipped; }
	FORCEINLINE int32 GetCount(EThrowableType Type) const
	{
		switch (Type)
		{
		case EThrowableType::ETT_FragGrenade:  return FragGrenadeCount;
		case EThrowableType::ETT_Flashbang:    return FlashbangCount;
		case EThrowableType::ETT_SmokeGrenade: return SmokeGrenadeCount;
		default:                                return 0;
		}
	}
	FORCEINLINE EThrowableType GetSelectedType() const { return SelectedType; }
	FORCEINLINE bool CanFire() const { return IsIdle(); }

	// HUD 倒计时查询（-1 = 未烹饪，0~1 = 进度百分比）
	FORCEINLINE float GetCookingProgress() const
	{
		if (ThrowState != EThrowableState::ETS_Cooking || CurrentMaxCookTime <= 0.f) return -1.f;
		const float Elapsed = GetWorld()->GetTimeSeconds() - CookStartTime;
		return Elapsed / CurrentMaxCookTime;
	}

protected:
	virtual void BeginPlay() override;

	// ===== 服务器 RPC（客户端调用，服务器权威执行）=====
	UFUNCTION(Server, Reliable)
	void ServerStartCooking(EThrowableType Type);

	UFUNCTION(Server, Reliable)
	void ServerCancelCooking();

	UFUNCTION(Server, Reliable)
	void ServerThrow(FVector_NetQuantize AimTarget);

	// ===== 多播 RPC（所有客户端播动画）=====
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayThrowAnimation();

	// ===== 服务器内部 =====
	void SpawnThrowable(const FVector& AimTarget, float RemainingFuse);
	void TransitionTo(EThrowableState NewState);
	void CookTimerExpired();
	void ExplodeInHand();     // 蓄力定时器到期 → 在手中生成投射物并立即爆炸
	void DelayedThrow_Internal(); // 延迟投出回调（由 ExecuteThrow 的 Timer 触发）

private:
	UPROPERTY()
	ABlasterCharacter* Character;

	// ===== 配置 =====

	// 投掷物类型 → 投射物类映射（编辑器中配置：Frag→BP_FragGrenade, Flash→BP_Flashbang, Smoke→BP_SmokeGrenade）
	UPROPERTY(EditAnywhere, Category = "Throwable|Config")
	TMap<EThrowableType, TSubclassOf<AThrowableProjectile>> ThrowableClasses;

	UPROPERTY(EditAnywhere, Category = "Throwable|Config")
	UAnimMontage* ThrowMontage;

	UPROPERTY(EditAnywhere, Category = "Throwable|Config")
	UAnimMontage* EquipMontage;  // 拿出投掷物的装备动画

	UPROPERTY(EditAnywhere, Category = "Throwable|Config")
	float ThrowCooldown = 0.5f;

	// 松开左键到实际投出之间的延迟（配合投掷动画释放帧）
	UPROPERTY(EditAnywhere, Category = "Throwable|Config")
	float ThrowDelayTime = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Throwable|Config")
	FName HandSocketName = FName("RightHandSocket");

	// 轨迹预测起点插槽（可偏右上，实现预测线平移效果，与手雷实际生成位置分离）
	UPROPERTY(EditAnywhere, Category = "Throwable|Config")
	FName ThrowSocketName = FName("ThrowSocket");

	// ===== 复制状态（仅服务器写入）=====
	UPROPERTY(ReplicatedUsing = OnRep_ThrowState)
	EThrowableState ThrowState = EThrowableState::ETS_Idle;

	UPROPERTY(Replicated)
	EThrowableType SelectedType = EThrowableType::ETT_FragGrenade;

	// 三种投掷物持有数量 — UE5 不支持 TMap 复制，拆为三个独立变量
	UPROPERTY(EditAnywhere, Replicated, Category = "Throwable|Config")
	int32 FragGrenadeCount = 0;

	UPROPERTY(EditAnywhere, Replicated, Category = "Throwable|Config")
	int32 FlashbangCount = 0;

	UPROPERTY(EditAnywhere, Replicated, Category = "Throwable|Config")
	int32 SmokeGrenadeCount = 0;

	UFUNCTION()
	void OnRep_ThrowState();

	// ===== 运行时（非复制）=====
	bool  bThrowableEquipped = false;  // 本地标记：已选中投掷物，左键应蓄力而非开火
	bool  bCanThrow = true;
	float CookStartTime;
	FTimerHandle CooldownTimer;
	FTimerHandle CookTimer;
	FTimerHandle ThrowDelayTimer; // 延迟投出定时器

	FVector_NetQuantize CachedAimTarget; // 松手瞬间锁定的瞄准点，防止 0.5s 延迟期间准星漂移

	// 从当前选中类型 CDO 缓存的参数（非复制，选型时更新）
	float CurrentMaxCookTime = 0.f;           // 当前类型蓄力最大时间
	bool  bCurrentHasCookTimer = false;       // 当前类型是否有蓄力定时器
	float CurrentThrowSpeed = 1500.f;         // 投掷初速
	float CurrentThrowAngle = 15.f;           // 上抛角度
	float CurrentGravityScale = 1.0f;         // 重力缩放
};
