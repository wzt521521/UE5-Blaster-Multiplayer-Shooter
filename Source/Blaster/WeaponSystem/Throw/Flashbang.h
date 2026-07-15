#pragma once

#include "CoreMinimal.h"
#include "ThrowableProjectile.h"
#include "Flashbang.generated.h"

class UNiagaraSystem;

// 闪光弹子类：无蓄力定时器可无限持握，爆炸后对视线内角色造成白屏致盲
UCLASS()
class BLASTER_API AFlashbang : public AThrowableProjectile
{
	GENERATED_BODY()

public:
	AFlashbang();

	// 无蓄力定时器，可无限持握
	virtual bool HasCookTimer() const override { return false; }
	virtual float GetRemainingFuse(float CookElapsed) const override { return 2.f; }

protected:
	virtual void ExplodeDamage() override;

	// 白屏效果持续时间（秒）
	UPROPERTY(EditAnywhere, Category = "Flashbang")
	float FlashMaxDuration = 3.f;

	// 影响半径（cm）
	UPROPERTY(EditAnywhere, Category = "Flashbang")
	float FlashRadius = 1000.f;

	// 闪光爆发 Niagara 粒子
	UPROPERTY(EditAnywhere, Category = "Flashbang")
	UNiagaraSystem* ExplosionEffect;
};
