#pragma once

#include "CoreMinimal.h"
#include "ThrowableProjectile.h"
#include "FragGrenade.generated.h"

class UNiagaraSystem;

// 破片手雷子类：蓄力定时器到期手中爆炸，投出后径向伤害 + Niagara 爆发特效
UCLASS()
class BLASTER_API AFragGrenade : public AThrowableProjectile
{
	GENERATED_BODY()

public:
	// 蓄力：有定时器，6s 到期手中爆炸
	virtual bool HasCookTimer() const override { return true; }
	virtual float GetMaxCookTime() const override { return 6.f; }
	virtual float GetRemainingFuse(float CookElapsed) const override
	{
		return FMath::Max(0.2f, GetMaxCookTime() - CookElapsed);
	}

protected:
	// 爆炸：径向伤害 + Niagara 爆发特效
	virtual void ExplodeDamage() override;

	// 爆炸 Niagara 特效（一次性爆发，火光+碎片+烟雾）
	UPROPERTY(EditAnywhere, Category = "Frag Grenade|VFX")
	UNiagaraSystem* ExplosionEffect;
};
