#include "FragGrenade.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void AFragGrenade::ExplodeDamage()
{
	// 径向伤害（父类 AProjectile::ExplodeDamage）
	Super::ExplodeDamage();

	// Niagara 爆发特效
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			ExplosionEffect,
			GetActorLocation(),
			GetActorRotation()
		);
	}
}
