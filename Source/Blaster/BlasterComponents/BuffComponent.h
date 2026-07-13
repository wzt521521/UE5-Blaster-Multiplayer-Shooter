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

	// 记录角色原始移动属性，Buff 结束后恢复用
	void SetInitialSpeeds(float BaseSpeed, float CrouchSpeed);
	void SetInitialJumpVelocity(float Velocity);

	// Speed Buff：服务器调用 → 修改本机 Movement + 启动 Timer + Multicast 推送到所有客户端
	void BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime);
	// Jump Buff：同上模式
	void BuffJump(float BuffJumpVelocity, float BuffTime);

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
};
