#pragma once

#include "CoreMinimal.h"
#include "../Projectile/ProjectileGrenade.h"
#include "ThrowableProjectile.generated.h"

// 投掷物抽象基类 — 定义飞行参数接口和虚函数，子类各自实现爆炸效果
// 不再包含 ThrowableType 枚举或具体爆炸参数（FlashMaxDuration/SmokeDuration 等移到子类）
UCLASS(Abstract)
class BLASTER_API AThrowableProjectile : public AProjectileGrenade
{
	GENERATED_BODY()

public:
	AThrowableProjectile();

	// 由投掷组件调用：设置初速并激活飞行
	void Launch(const FVector& HandLocation, const FVector& AimTarget);

	// 由投掷组件在 Spawn 后立即调用：覆盖默认引信时间
	void SetFuseTime(float Time);

	// --- 子类需重写的虚函数接口 ---

	// 是否需要蓄力定时器（Frag=true 到期手中爆炸，Flash/Smoke=false 无限持握）
	virtual bool HasCookTimer() const { return false; }

	// 最大蓄力时间（仅 HasCookTimer()==true 时有效）
	virtual float GetMaxCookTime() const { return 0.f; }

	// 计算投出后的剩余引信（CookElapsed：已蓄力时间）
	virtual float GetRemainingFuse(float CookElapsed) const { return 3.f; }

	// 投掷初速
	UPROPERTY(EditAnywhere)
	float ThrowSpeed = 1500.f;

	// 投掷仰角加成（度），使抛射有自然的上抛弧度
	UPROPERTY(EditAnywhere)
	float ThrowUpwardAngle = 15.f;

	// 重力缩放：0=无重力直线飞行，1=正常重力，>1=更大下坠
	UPROPERTY(EditAnywhere, Category = "Throwable|Physics")
	float ProjectileGravityScale = 1.0f;

protected:
	virtual void BeginPlay() override;

	// 默认爆炸 = 径向伤害（子类可重写为闪光/烟雾等效果）
	virtual void ExplodeDamage() override;

	// 碰触即爆型 OnHit 绑定（闪光弹/烟雾弹常用）
	UFUNCTION()
	virtual void OnThrowableHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 碰触即爆（子类构造中设为 true 启用）
	UPROPERTY(EditAnywhere)
	bool bExplodeOnImpact = false;
};
