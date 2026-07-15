#pragma once

#include "CoreMinimal.h"
#include "ThrowableProjectile.h"
#include "SmokeGrenade.generated.h"

// 烟雾弹子类：无蓄力定时器可无限持握，爆炸后生成烟雾 Actor
UCLASS()
class BLASTER_API ASmokeGrenade : public AThrowableProjectile
{
	GENERATED_BODY()

public:
	ASmokeGrenade();

	// 无蓄力定时器，可无限持握
	virtual bool HasCookTimer() const override { return false; }
	virtual float GetRemainingFuse(float CookElapsed) const override { return 2.f; }

protected:
	virtual void ExplodeDamage() override;

	// 烟雾持续总时间（秒）
	UPROPERTY(EditAnywhere, Category = "Smoke")
	float SmokeDuration = 10.f;

	// 烟雾 Actor 类（如 BP_Smoke），爆炸时在落点生成
	UPROPERTY(EditAnywhere, Category = "Smoke")
	TSubclassOf<AActor> SmokeActorClass;

	// 烟雾半径（cm），传给生成的 Actor 做缩放参考
	UPROPERTY(EditAnywhere, Category = "Smoke")
	float SmokeRadius = 500.f;
};
