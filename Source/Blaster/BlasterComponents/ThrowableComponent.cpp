#include "ThrowableComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/WeaponSystem/Throw/ThrowableProjectile.h"
#include "Blaster/BlasterTypes/CombatState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

UThrowableComponent::UThrowableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 预填三个键避免编辑器 "default value key" 冲突，蓝图只需赋值对应类
	ThrowableClasses.Add(EThrowableType::ETT_FragGrenade, nullptr);
	ThrowableClasses.Add(EThrowableType::ETT_Flashbang, nullptr);
	ThrowableClasses.Add(EThrowableType::ETT_SmokeGrenade, nullptr);
}

void UThrowableComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<ABlasterCharacter>(GetOwner());
}

void UThrowableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Character && Character->IsLocallyControlled())
	{
		ABlasterPlayerController* PC = Cast<ABlasterPlayerController>(Character->Controller);
		if (PC)
		{
			if (ThrowState == EThrowableState::ETS_Cooking)
			{
				// 蓄力倒计时 HUD 更新（有定时器的才显示进度）
				if (bCurrentHasCookTimer)
				{
					const float Elapsed = GetWorld()->GetTimeSeconds() - CookStartTime;
					const float Remaining = FMath::Max(0.f, CurrentMaxCookTime - Elapsed);
					PC->SetHUDThrowableCooking(true, Remaining);
				}

				// 预测轨迹线绘制
				const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(HandSocketName);
				if (HandSocket)
				{
					const FVector HandLocation = HandSocket->GetSocketTransform(Character->GetMesh()).GetLocation();
					const FVector AimTarget = Character->GetActorLocation() + Character->GetActorForwardVector() * 1000.f;

					FVector Direction = (AimTarget - HandLocation).GetSafeNormal();
					const float UpwardRads = FMath::DegreesToRadians(CurrentThrowAngle);
					Direction.Z += FMath::Sin(UpwardRads);
					Direction.Normalize();
					const FVector LaunchVelocity = Direction * CurrentThrowSpeed;

					FPredictProjectilePathParams PathParams;
					PathParams.StartLocation = HandLocation;
					PathParams.LaunchVelocity = LaunchVelocity;
					PathParams.ProjectileRadius = 5.f;
					PathParams.MaxSimTime = 3.f;
					PathParams.bTraceWithCollision = true;
					PathParams.bTraceWithChannel = true;
					PathParams.TraceChannel = ECC_Visibility;
					PathParams.SimFrequency = 20.f;
					PathParams.ActorsToIgnore.Add(Character); // 忽略自身碰撞，避免轨迹被角色身体阻挡
					PathParams.OverrideGravityZ = CurrentGravityScale > 0.f
						? FMath::Sign(GetWorld()->GetGravityZ()) * FMath::Abs(GetWorld()->GetGravityZ()) * CurrentGravityScale
						: 0.f;

					FPredictProjectilePathResult PathResult;
					if (UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult))
					{
						// 画路径线和落点球
						for (int32 i = 0; i < PathResult.PathData.Num() - 1; ++i)
						{
							DrawDebugLine(GetWorld(),
								PathResult.PathData[i].Location,
								PathResult.PathData[i + 1].Location,
								FColor::Green, false, -1.f, 0, 1.f);
						}
						if (PathResult.PathData.Num() > 0)
						{
							const FVector LandingPoint = PathResult.LastTraceDestination.Location;
							DrawDebugSphere(GetWorld(), LandingPoint, 15.f, 12, FColor::Red, false, -1.f, 0, 1.f);
						}
					}
				}
			}
			else
			{
				PC->SetHUDThrowableCooking(false, 0.f);
			}
		}
	}
}

void UThrowableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UThrowableComponent, ThrowState);
	DOREPLIFETIME(UThrowableComponent, SelectedType);
	DOREPLIFETIME(UThrowableComponent, FragGrenadeCount);
	DOREPLIFETIME(UThrowableComponent, FlashbangCount);
	DOREPLIFETIME(UThrowableComponent, SmokeGrenadeCount);
}

// ========================================================================
// 装备/卸载
// ========================================================================

void UThrowableComponent::EquipThrowable(EThrowableType Type)
{
	if (!IsIdle())
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipThrowable FAILED: not idle, state=%d"), (int32)ThrowState);
		return;
	}
	if (GetCount(Type) <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipThrowable FAILED: count=0 for type=%d"), (int32)Type);
		return;
	}

	bThrowableEquipped = true;
	SelectThrowable(Type);
	UE_LOG(LogTemp, Log, TEXT("EquipThrowable SUCCESS: type=%d equipped, count=%d"), (int32)Type, GetCount(Type));

	if (Character && EquipMontage)
	{
		Character->PlayAnimMontage(EquipMontage);
	}
}

void UThrowableComponent::UnequipThrowable()
{
	bThrowableEquipped = false;
	if (IsCooking())
	{
		CancelCooking();
	}
}

// ========================================================================
// 输入接口 — 客户端乐观预测 + 服务器 RPC 权威裁决
// ========================================================================

void UThrowableComponent::StartCooking()
{
	if (!Character || !bCanThrow)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartCooking FAILED: no Character or bCanThrow=false"));
		return;
	}
	if (!IsIdle() || !bThrowableEquipped)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartCooking FAILED: IsIdle=%d, bEquipped=%d"), (int32)IsIdle(), (int32)bThrowableEquipped);
		return;
	}
	if (Character->GetCombatState() != ECombatState::ECS_Unoccupied)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartCooking FAILED: CombatState=%d"), (int32)Character->GetCombatState());
		return;
	}

	if (Character->HasAuthority())
	{
		// 服务器/单机直走权威逻辑：设 CookStartTime + 启动 CookTimer
		ServerStartCooking_Implementation(SelectedType);
	}
	else
	{
		// 客户端乐观预测 + 服务器 RPC
		TransitionTo(EThrowableState::ETS_Cooking);
		ServerStartCooking(SelectedType);
	}
	UE_LOG(LogTemp, Log, TEXT("StartCooking SUCCESS: type=%d"), (int32)SelectedType);
}

void UThrowableComponent::ExecuteThrow()
{
	if (ThrowState != EThrowableState::ETS_Cooking) return;

	GetWorld()->GetTimerManager().ClearTimer(CookTimer);

	if (ThrowMontage && Character)
	{
		Character->PlayAnimMontage(ThrowMontage);
	}

	// 客户端乐观预测：先切 Throwing，等服务器 RPC 复制回 Idle
	if (!Character->HasAuthority())
	{
		TransitionTo(EThrowableState::ETS_Throwing);
	}

	FVector AimTarget = Character->GetActorLocation() + Character->GetActorForwardVector() * 1000.f;
	ServerThrow(AimTarget);  // 服务器检查 Cooking 状态，内部 TransitionTo(Idle)

	bThrowableEquipped = false;
}

void UThrowableComponent::CancelCooking()
{
	if (ThrowState != EThrowableState::ETS_Cooking) return;

	GetWorld()->GetTimerManager().ClearTimer(CookTimer);
	TransitionTo(EThrowableState::ETS_Idle);

	if (!Character->HasAuthority())
	{
		ServerCancelCooking();
	}
}

// ========================================================================
// 服务器 RPC
// ========================================================================

void UThrowableComponent::ServerStartCooking_Implementation(EThrowableType Type)
{
	if (!bCanThrow || !IsIdle()) return;
	if (GetCount(Type) <= 0) return;

	SelectedType = Type;
	// 选型时从 CDO 缓存当前类型参数
	SelectThrowable(Type);

	TransitionTo(EThrowableState::ETS_Cooking);
	CookStartTime = GetWorld()->GetTimeSeconds();

	// 仅当当前类型有蓄力定时器时（Frag）才启动手中爆炸计时
	if (bCurrentHasCookTimer)
	{
		GetWorld()->GetTimerManager().SetTimer(
			CookTimer,
			this,
			&UThrowableComponent::CookTimerExpired,
			CurrentMaxCookTime
		);
	}
}

void UThrowableComponent::ServerCancelCooking_Implementation()
{
	if (ThrowState != EThrowableState::ETS_Cooking) return;
	GetWorld()->GetTimerManager().ClearTimer(CookTimer);
	TransitionTo(EThrowableState::ETS_Idle);
}

void UThrowableComponent::ServerThrow_Implementation(FVector_NetQuantize AimTarget)
{
	if (ThrowState != EThrowableState::ETS_Cooking) return;
	if (GetCount(SelectedType) <= 0) return;

	GetWorld()->GetTimerManager().ClearTimer(CookTimer);

	// 从 CDO 查询剩余引信（各子类自定规则）
	float RemainingFuse = 3.f;
	TSubclassOf<AThrowableProjectile>* ClassPtr = ThrowableClasses.Find(SelectedType);
	if (ClassPtr && *ClassPtr)
	{
		AThrowableProjectile* CDO = (*ClassPtr)->GetDefaultObject<AThrowableProjectile>();
		const float Elapsed = GetWorld()->GetTimeSeconds() - CookStartTime;
		RemainingFuse = CDO->GetRemainingFuse(Elapsed);
	}

	// 权威扣减数量
	switch (SelectedType)
	{
	case EThrowableType::ETT_FragGrenade:  FragGrenadeCount  = FMath::Max(FragGrenadeCount - 1, 0); break;
	case EThrowableType::ETT_Flashbang:    FlashbangCount    = FMath::Max(FlashbangCount - 1, 0);   break;
	case EThrowableType::ETT_SmokeGrenade: SmokeGrenadeCount = FMath::Max(SmokeGrenadeCount - 1, 0); break;
	}

	SpawnThrowable(AimTarget, RemainingFuse);
	MulticastPlayThrowAnimation();
	TransitionTo(EThrowableState::ETS_Idle);
}

void UThrowableComponent::MulticastPlayThrowAnimation_Implementation()
{
	if (Character && !Character->IsLocallyControlled() && ThrowMontage)
	{
		Character->PlayAnimMontage(ThrowMontage);
	}
}

// ========================================================================
// 服务器内部
// ========================================================================

void UThrowableComponent::SpawnThrowable(const FVector& AimTarget, float RemainingFuse)
{
	if (!Character) return;
	if (!Character->HasAuthority()) return;

	// 从 TMap 获取当前选中类型对应的投射物类
	TSubclassOf<AThrowableProjectile>* ClassPtr = ThrowableClasses.Find(SelectedType);
	if (!ClassPtr || !*ClassPtr) return;

	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(HandSocketName);
	if (!HandSocket) return;

	const FVector SpawnLocation = HandSocket->GetSocketTransform(Character->GetMesh()).GetLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AThrowableProjectile* Projectile = GetWorld()->SpawnActor<AThrowableProjectile>(
		*ClassPtr,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (Projectile)
	{
		Projectile->Launch(SpawnLocation, AimTarget);
		Projectile->SetFuseTime(RemainingFuse);
	}
}

// ========================================================================
// 状态机
// ========================================================================

void UThrowableComponent::TransitionTo(EThrowableState NewState)
{
	ThrowState = NewState;

	switch (NewState)
	{
	case EThrowableState::ETS_Idle:
		GetWorld()->GetTimerManager().SetTimer(
			CooldownTimer,
			FTimerDelegate::CreateLambda([this]() { bCanThrow = true; }),
			ThrowCooldown,
			false
		);
		break;
	case EThrowableState::ETS_Cooking:
		bCanThrow = false;
		break;
	case EThrowableState::ETS_Throwing:
		break;
	}
}

void UThrowableComponent::CookTimerExpired()
{
	if (ThrowState != EThrowableState::ETS_Cooking) return;
	ExplodeInHand();
}

void UThrowableComponent::ExplodeInHand()
{
	if (!Character || !Character->HasAuthority()) return;

	// 从 TMap 获取当前选中类型对应的投射物类
	TSubclassOf<AThrowableProjectile>* ClassPtr = ThrowableClasses.Find(SelectedType);
	if (!ClassPtr || !*ClassPtr) return;

	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(HandSocketName);
	if (!HandSocket) return;

	const FVector SpawnLocation = HandSocket->GetSocketTransform(Character->GetMesh()).GetLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AThrowableProjectile* Projectile = GetWorld()->SpawnActor<AThrowableProjectile>(
		*ClassPtr,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (Projectile)
	{
		Projectile->SetFuseTime(0.1f);  // 立即引爆
	}

	// 权威扣除数量（通用逻辑）
	switch (SelectedType)
	{
	case EThrowableType::ETT_FragGrenade:  FragGrenadeCount  = FMath::Max(FragGrenadeCount - 1, 0); break;
	case EThrowableType::ETT_Flashbang:    FlashbangCount    = FMath::Max(FlashbangCount - 1, 0);   break;
	case EThrowableType::ETT_SmokeGrenade: SmokeGrenadeCount = FMath::Max(SmokeGrenadeCount - 1, 0); break;
	}

	bThrowableEquipped = false;
	TransitionTo(EThrowableState::ETS_Idle);
}

void UThrowableComponent::OnRep_ThrowState()
{
	switch (ThrowState)
	{
	case EThrowableState::ETS_Idle:
		GetWorld()->GetTimerManager().ClearTimer(CookTimer);
		break;
	case EThrowableState::ETS_Cooking:
		CookStartTime = GetWorld()->GetTimeSeconds();
		break;
	case EThrowableState::ETS_Throwing:
		break;
	}
}

// ========================================================================
// 切换/查询
// ========================================================================

void UThrowableComponent::SelectThrowable(EThrowableType Type)
{
	if (!IsIdle()) return;
	if (GetCount(Type) <= 0) return;
	SelectedType = Type;

	// 从 CDO 缓存当前类型的蓄力参数 + 投掷物理参数
	TSubclassOf<AThrowableProjectile>* ClassPtr = ThrowableClasses.Find(Type);
	if (ClassPtr && *ClassPtr)
	{
		AThrowableProjectile* CDO = (*ClassPtr)->GetDefaultObject<AThrowableProjectile>();
		CurrentMaxCookTime = CDO->GetMaxCookTime();
		bCurrentHasCookTimer = CDO->HasCookTimer();
		CurrentThrowSpeed = CDO->ThrowSpeed;
		CurrentThrowAngle = CDO->ThrowUpwardAngle;
		CurrentGravityScale = CDO->ProjectileGravityScale;
	}
}
