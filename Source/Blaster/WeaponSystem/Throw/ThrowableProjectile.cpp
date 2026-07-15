#include "ThrowableProjectile.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AThrowableProjectile::AThrowableProjectile()
{
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->InitialSpeed = 0.f;  // 停用自动初速，由 Launch() 手动设置
		ProjectileMovementComponent->MaxSpeed = ThrowSpeed;
		ProjectileMovementComponent->bShouldBounce = true;
	}
}

void AThrowableProjectile::Launch(const FVector& HandLocation, const FVector& AimTarget)
{
	if (!ProjectileMovementComponent) return;

	FVector Direction = (AimTarget - HandLocation).GetSafeNormal();
	const float UpwardRads = FMath::DegreesToRadians(ThrowUpwardAngle);
	Direction.Z += FMath::Sin(UpwardRads);
	Direction.Normalize();

	ProjectileMovementComponent->Velocity = Direction * ThrowSpeed;
	ProjectileMovementComponent->ProjectileGravityScale = ProjectileGravityScale;
}

void AThrowableProjectile::SetFuseTime(float Time)
{
	DestroyTime = FMath::Max(Time, 0.1f);
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(DestroyTimer);
	TimerManager.SetTimer(DestroyTimer, FTimerDelegate::CreateUObject(this, &AThrowableProjectile::DestroyTimerFinished), DestroyTime, false);
}

void AThrowableProjectile::BeginPlay()
{
	AActor::BeginPlay();

	SpawnTrailSystem();
	ProjectileMovementComponent->OnProjectileBounce.AddDynamic(this, &AThrowableProjectile::OnBounce);

	// 碰触引爆型：绑定碰撞，忽略投掷者防止 spawn 瞬间撞到自己脚下爆炸
	if (bExplodeOnImpact && HasAuthority())
	{
		CollisionBox->IgnoreActorWhenMoving(GetInstigator(), true);
		CollisionBox->OnComponentHit.AddDynamic(this, &AThrowableProjectile::OnThrowableHit);
		CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Block);
	}
}

// 基类默认爆炸 = 径向伤害，FragGrenade 直接继承此行为
// Flashbang/SmokeGrenade 子类各自重写
void AThrowableProjectile::ExplodeDamage()
{
	Super::ExplodeDamage();  // AProjectile::ExplodeDamage → ApplyRadialDamageWithFalloff
}

void AThrowableProjectile::OnThrowableHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor == GetInstigator() || OtherActor == GetOwner()) return;  // 忽略投掷者
	ProjectileMovementComponent->StopMovementImmediately();
	Destroy();
}
