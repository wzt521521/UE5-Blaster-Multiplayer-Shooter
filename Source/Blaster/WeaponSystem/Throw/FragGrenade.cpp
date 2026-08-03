#include "FragGrenade.h"
#if !UE_SERVER
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#endif

void AFragGrenade::ExplodeDamage()
{
	// 径向伤害（父类 AProjectile::ExplodeDamage）
	Super::ExplodeDamage();

	// Niagara 爆发特效
#if !UE_SERVER
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			Cast<UNiagaraSystem>(ExplosionEffect),
			GetActorLocation(),
			GetActorRotation()
		);
	}
#endif
}
