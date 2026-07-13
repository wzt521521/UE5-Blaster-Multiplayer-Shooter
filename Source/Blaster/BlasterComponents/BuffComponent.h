#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuffComponent.generated.h"

UCLASS()
class BLASTER_API UBuffComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class ABlasterCharacter;

public:
	UBuffComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 记录角色原始移动属性，Buff 结束后恢复用
	void SetInitialSpeeds(float BaseSpeed, float CrouchSpeed);
	void SetInitialJumpVelocity(float Velocity);

	// Speed Buff：服务器调用 → 修改本机 Movement + 启动 Timer + Multicast 推送到所有客户端
	void BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime);
	// Jump Buff：同上模式
	void BuffJump(float BuffJumpVelocity, float BuffTime);

	// Shield：服务器调用 → Tick 每帧递增护盾（平滑 ramp-up），可叠加多次拾取
	void ReplenishShield(float ShieldAmount, float ReplenishTime);

protected:
	virtual void BeginPlay() override;

private:
	// 缓存角色指针，避免每次 Cast
	UPROPERTY()
	class ABlasterCharacter* Character;

	// 角色原始移动属性，Timer 到期后恢复
	float InitialBaseSpeed;
	float InitialCrouchSpeed;
	float InitialJumpVelocity;

	FTimerHandle SpeedBuffTimer;
	FTimerHandle JumpBuffTimer;

	// Timer 回调：恢复角色原始速度
	UFUNCTION()
	void ResetSpeeds();
	// Timer 回调：恢复角色原始跳跃
	UFUNCTION()
	void ResetJump();

	// 多播 RPC：把速度/跳跃修改推送到所有客户端
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpeedBuff(float BaseSpeed, float CrouchSpeed);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastJumpBuff(float JumpVelocity);

	// ———— Shield ramp-up 状态 ————
	// 护盾不是瞬间加上，而是每帧递增，实现平滑增长效果
	bool bReplenishingShield = false;
	float ShieldReplenishRate = 0.f;   // 每秒增加多少护盾值
	float ShieldReplenishAmount = 0.f; // 剩余待增加的护盾总量

	void ShieldRampUp(float DeltaTime);
};
