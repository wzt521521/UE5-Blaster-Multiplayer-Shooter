#include "SmokeGrenade.h"
#include "Engine/World.h"

ASmokeGrenade::ASmokeGrenade()
{
	// 松手后 2s 引信倒计时，不碰触即爆
}

void ASmokeGrenade::ExplodeDamage()
{
	if (!HasAuthority()) return;

	const FVector SpawnLocation = GetActorLocation();

	if (SmokeActorClass)
	{
		GetWorld()->SpawnActor<AActor>(
			SmokeActorClass,
			SpawnLocation,
			GetActorRotation()
		);
	}
}
