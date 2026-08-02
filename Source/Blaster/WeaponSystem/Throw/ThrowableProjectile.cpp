#include "ThrowableProjectile.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AThrowableProjectile::AThrowableProjectile()
{
	if (ProjectileMovementComponent)
	{
		ProjectileMovementComponent->InitialSpeed = 0.f;  // 停用自动初速，由 Launch() 手动设置 + ReplicatedUsing 同步给客户端
		ProjectileMovementComponent->MaxSpeed = ThrowSpeed;
		ProjectileMovementComponent->bShouldBounce = true;
	}
}

void AThrowableProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AThrowableProjectile, ReplicatedVelocity);
	DOREPLIFETIME(AThrowableProjectile, ReplicatedGravityScale);
}

void AThrowableProjectile::Launch(const FVector& HandLocation, const FVector& AimTarget)
{
	if (!ProjectileMovementComponent) return;

	// 绕瞄准方向的右轴旋转上抛角，相对于瞄准线而非固定世界Z轴
	FVector Direction = (AimTarget - HandLocation).GetSafeNormal();
	const FVector RightAxis = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	Direction = Direction.RotateAngleAxis(ThrowUpwardAngle, RightAxis);

	ProjectileMovementComponent->Velocity = Direction * ThrowSpeed;
	ProjectileMovementComponent->ProjectileGravityScale = ProjectileGravityScale;

	// 复制给客户端：确保客户端 ProjectileMovementComponent 收到相同的 Velocity 和 GravityScale
	ReplicatedVelocity = ProjectileMovementComponent->Velocity;
	ReplicatedGravityScale = ProjectileGravityScale;
}

void AThrowableProjectile::OnRep_LaunchParams()
{
	// 客户端收到复制的 Launch 参数，直接赋值给 ProjectileMovementComponent，
	// 绕过 InitialSpeed + SpawnRotation 自动推导（会被 BP 默认值覆盖不可靠）
	if (!HasAuthority() && ProjectileMovementComponent)
	{
		ProjectileMovementComponent->Velocity = ReplicatedVelocity;
		ProjectileMovementComponent->ProjectileGravityScale = ReplicatedGravityScale;
	}
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
