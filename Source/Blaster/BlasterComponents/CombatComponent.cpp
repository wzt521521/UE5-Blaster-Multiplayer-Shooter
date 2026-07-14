


#include "CombatComponent.h"
#include "../Character/BlasterCharacter.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blaster/Weapon/Shotgun.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "../PlayerController/BlasterPlayerController.h"
#include "Blaster/Weapon/Projectile.h"

UCombatComponent::UCombatComponent()
{

	PrimaryComponentTick.bCanEverTick = true;

	BaseWalkSpeed=600.f;
	AimWalkSpeed=300.f;
}

void UCombatComponent::EquipWeapon(AWeapon *WeaponToEquip)//此函数从始至终都只会在服务器上面执行
{
	if(Character==NULL)return;
	if(WeaponToEquip==NULL)return;
	if (CombatState != ECombatState::ECS_Unoccupied) return;
	EquipPrimaryWeapon(WeaponToEquip);
	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
}

void UCombatComponent::EquipPrimaryWeapon(AWeapon* WeaponToEquip)
{
	if (WeaponToEquip == nullptr) return;
	DropEquippedWeapon();
	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	AttachActorToRightHand(EquippedWeapon);
	EquippedWeapon->SetOwner(Character);
	EquippedWeapon->SetHUDAmmo();
	PlayEquipWeaponSound(WeaponToEquip);
	ReloadEmptyWeapon();
}

void UCombatComponent::PlayEquipWeaponSound(AWeapon* WeaponToEquip)
{
	if (Character && WeaponToEquip && WeaponToEquip->EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			WeaponToEquip->EquipSound,
			Character->GetActorLocation()
		);
	}
}

void UCombatComponent::DropEquippedWeapon()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->Dropped();
	}
}

void UCombatComponent::AttachActorToRightHand(AActor* ActorToAttach)
{
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr) return;
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (HandSocket)
	{
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::AttachActorToLeftHand(AActor* ActorToAttach)
{
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr || EquippedWeapon == nullptr) return;
	bool bUsePistolSocket =
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol ||
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SubmachineGun;
	FName SocketName = bUsePistolSocket ? FName("PistolSocket") : FName("LeftHandSocket");
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(SocketName);
	if (HandSocket)
	{
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::ReloadEmptyWeapon()
{
	if (EquippedWeapon && EquippedWeapon->IsEmpty())
	{
		Reload();
	}
}

void UCombatComponent::Reload()
{
	if (EquippedWeapon && EquippedWeapon->HasSpareAmmo()
		&& CombatState != ECombatState::ECS_Reloading
		&& !EquippedWeapon->IsFull()
		&& !bLocallyReloading)
	{
		if (!Character->HasAuthority()) HandReload();
		ServerReload();
		bLocallyReloading = true;
	}
}

void UCombatComponent::FinishReloading()
{
	if (Character == NULL) return;
	bLocallyReloading = false;
	if (Character->HasAuthority()) {
		CombatState = ECombatState::ECS_Unoccupied;
	}
	if (bFireButtonPressed) {
		Fire();
	}
}

void UCombatComponent::ServerReload_Implementation()//只会在服务器执行
{
	if(Character == NULL||EquippedWeapon==NULL) return;

	int32 ReloadAmount = AmountToReload();
	if (ReloadAmount > 0)
	{
		EquippedWeapon->ReloadFromSpare(ReloadAmount);
	}
	HandReload();
	CombatState = ECombatState::ECS_Reloading;
	bLocallyReloading = true;
}

void UCombatComponent::SetAiming(bool bIsAiming)
{
	bAiming = bIsAiming;
	if (!GetOwner()->HasAuthority())
	{
		ServerSetAiming(bIsAiming);
	}
	if(Character){
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
	// 狙击枪开镜/关镜 Scope Widget（仅本地玩家可见）
	if (Character && Character->IsLocallyControlled() && EquippedWeapon && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle)
	{
		Character->ShowSniperScopeWidget(bIsAiming);
	}
}

void UCombatComponent::OnRep_EquippedWeapon()
{
	if (EquippedWeapon && Character)
	{
		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
		AttachActorToRightHand(EquippedWeapon);
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
		PlayEquipWeaponSound(EquippedWeapon);
		EquippedWeapon->EnableCustomDepth(false);
		EquippedWeapon->SetHUDAmmo();
	}
}

void UCombatComponent::FireButtonPressed(bool bPressed)
{
	bFireButtonPressed = bPressed;
	if (bFireButtonPressed)
	{
		Fire();
		if(EquippedWeapon){
			CrosshairShootingFactor = 0.75f;
		}
	}
}

void UCombatComponent::Fire()
{
	if (CanFire())
	{
		bCanFire = false;
		if (EquippedWeapon == nullptr) return;

		if (Character && Character->IsLocallyControlled())
		{
			FHitResult HitResult;
			TraceUnderCrosshairs(HitResult);
			HitTarget = HitResult.ImpactPoint;
		}

		switch (EquippedWeapon->FireType)
		{
		case EFireType::EFT_Projectile:
			FireProjectileWeapon();
			break;
		case EFireType::EFT_HitScan:
			FireHitScanWeapon();
			break;
		case EFireType::EFT_Shotgun:
			FireShotgun();
			break;
		}

		ApplyRecoil();
		StartFireTimer();
	}
}

void UCombatComponent::FireProjectileWeapon()
{
	if (EquippedWeapon && Character)
	{
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget, bAiming) : HitTarget;
		LocalFire(HitTarget);
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireHitScanWeapon()
{
	if (EquippedWeapon && Character)
	{
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget, bAiming) : HitTarget;
		LocalFire(HitTarget);
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireShotgun()
{
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	if (Shotgun && Character)
	{
		TArray<FVector_NetQuantize> HitTargets;
		Shotgun->ShotgunTraceEndWithScatter(HitTarget, HitTargets, bAiming);
		if (!Character->HasAuthority()) ShotgunLocalFire(HitTargets);
		ServerShotgunFire(HitTargets, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::ShotgunLocalFire(const TArray<FVector_NetQuantize>& TraceHitTargets)
{
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	if (Shotgun == nullptr || Character == nullptr) return;
	if (CombatState == ECombatState::ECS_Unoccupied)
	{
		bLocallyReloading = false;
		Character->PlayFireMontage(bAiming);
		Shotgun->FireShotgun(TraceHitTargets);
	}
}

void UCombatComponent::LocalFire(const FVector_NetQuantize& TraceHitTarget)
{
	if (EquippedWeapon == nullptr) return;
	if (Character)
	{
		//只在本地玩家的客户端上面播放开火动画和特效，不实际生成子弹
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}

void UCombatComponent::ApplyRecoil()
{
	// 纯客户端视觉效果：开火后摄像机视角上跳+随机水平偏移，不影响子弹落点
	// 不实现自动恢复 → 玩家需手动拉鼠标压枪
	if (!Character || !Character->IsLocallyControlled() || !EquippedWeapon) return;

	APlayerController* PC = Character->GetController<APlayerController>();
	if (!PC) return;

	float PitchRecoil = FMath::RandRange(EquippedWeapon->RecoilPitchMin, EquippedWeapon->RecoilPitchMax);
	float YawRecoil  = FMath::RandRange(EquippedWeapon->RecoilYawMin,  EquippedWeapon->RecoilYawMax);

	PC->AddPitchInput(-PitchRecoil); // UE Pitch: 正=低头, 负=抬头 → 取反实现上跳
	PC->AddYawInput(YawRecoil);      // 正值 = 屏幕右偏
}

void UCombatComponent::StartFireTimer()
{
	if (EquippedWeapon == nullptr || Character == nullptr) return;
	Character->GetWorldTimerManager().SetTimer(
		FireTimer,
		this,
		&UCombatComponent::FireTimerFinished,
		EquippedWeapon->FireDelay
	);
}

void UCombatComponent::FireTimerFinished()
{
	if (EquippedWeapon == nullptr) return;
	bCanFire = true;
	if (bFireButtonPressed && EquippedWeapon->bAutomatic)
	{
		Fire();
	}
	ReloadEmptyWeapon();
}

bool UCombatComponent::CanFire()
{
	if (EquippedWeapon == nullptr) return false;
	if (bLocallyReloading) return false;
	return !EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Unoccupied;
}

//从屏幕正中心的准星位置，向 3D 世界里射出一根长达 800 米的"激光指针"，
//看看它碰到了什么，把碰撞点的世界坐标存到 TraceHitResult 里。
void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult)
{
	FVector2D ViewportSize;//获取屏幕尺寸
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	//算出准星在屏幕上的位置
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;

	//把屏幕上的 2D 点，转成 3D 世界里的射线
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,//CrosshairWorldPosition：摄像机所在的位置（射线起点）
		CrosshairWorldDirection//CrosshairWorldDirection：摄像机朝向的方向（射线方向）
	);

	if (bScreenToWorld)
	{

		FVector Start = CrosshairWorldPosition;//CrosshairWorldPosition是摄像机的位置

		if (Character)
		{
			float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();

			//把射线起点往前推，跳过玩家自己的身体
			Start += CrosshairWorldDirection * (DistanceToCharacter + 100.f);//CrosshairWorldDirection 是瞄准的方向向量（单位向量）
		}

		//发射射线
		FVector End = Start + CrosshairWorldDirection * 80000.f;

		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult,      // 输出：命中结果（撞到了谁、撞在哪）
			Start,               // 起点
			End,                 // 终点
			ECC_Visibility       // 只检测"可见"通道上的物体
		);

		if (!TraceHitResult.bBlockingHit)//如果射线没有碰到任何物体，就把终点当作命中点
		{
			TraceHitResult.ImpactPoint = End;
			HitTarget = End;
		}
		else
		{
			HitTarget = TraceHitResult.ImpactPoint;
			// DrawDebugSphere(GetWorld(), TraceHitResult.ImpactPoint, 16.f, 12, FColor::Red, false, 2.f);
		}

		if(TraceHitResult.GetActor() && TraceHitResult.GetActor()->Implements<UInteractWithCrosshairsInterface>())
		{
			HUDPackage.CrosshairsColor = FLinearColor::Red;
		}
		else{
			HUDPackage.CrosshairsColor = FLinearColor::White;
		}
	}
}

// 每帧将当前武器的准星纹理打包传给 HUD，用于绘制十字准心
void UCombatComponent::SetHUDCrosshairs(float DeltaTime)
{
	if(Character == nullptr) return;

	// 惰性缓存：只在第一次调用时获取 PlayerController，后续复用，避免每帧 Cast
	Controller = Controller == nullptr ? Cast<ABlasterPlayerController>(Character->GetController()) : Controller;

	if(Controller)
	{
		// 从 Controller 拿到 HUD 并转为自定义类型
		HUD = HUD == nullptr ? Cast<ABlasterHud>(Controller->GetHUD()) : HUD;
		if(HUD)
		{

			
			if(EquippedWeapon)
			{
				// 把武器上的五块准星纹理填入数据包，HUD 会根据武器散布动态偏移每块的位置
				HUDPackage.CrosshairsCenter = EquippedWeapon->CrosshairsCenter;
				HUDPackage.CrosshairsLeft = EquippedWeapon->CrosshairsLeft;
				HUDPackage.CrosshairsRight = EquippedWeapon->CrosshairsRight;
				HUDPackage.CrosshairsBottom = EquippedWeapon->CrosshairsBottom;
				HUDPackage.CrosshairsTop = EquippedWeapon->CrosshairsTop;
			}
			else{
				// 没有武器时使用默认准星（可以是全透明的占位图）
				HUDPackage.CrosshairsCenter = nullptr;
				HUDPackage.CrosshairsLeft = nullptr;
				HUDPackage.CrosshairsRight = nullptr;
				HUDPackage.CrosshairsBottom = nullptr;
				HUDPackage.CrosshairsTop = nullptr;
			}
			//计算准星散布
			FVector2D WalkSpreadRange(0.f,Character->GetCharacterMovement()->MaxWalkSpeed);
			FVector2D VelocityMultiplierRange(0.f,1.f);
			FVector Velocity = Character->GetVelocity();
			Velocity.Z = 0.f;//只考虑水平速度对准星散布的影响，垂直速度不影响
			CrosshairVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpreadRange, VelocityMultiplierRange, Velocity.Size());

			if(Character->GetCharacterMovement()->IsFalling())//如果角色在空中，增加散布
			{
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 2.25f, DeltaTime, 2.25f);
			}
			else
			{
				CrosshairInAirFactor = FMath::FInterpTo(CrosshairInAirFactor, 0.f, DeltaTime, 2.25f);
			}

			if(bAiming)//如果角色在瞄准，减少散布
			{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.58f, DeltaTime, 30.f);
			}
			else
			{
				CrosshairAimFactor = FMath::FInterpTo(CrosshairAimFactor, 0.f, DeltaTime, 30.f);
			}

			// 射击散布从当前值平滑回 0，模拟后坐力恢复（不用 timer 而用插值，帧率无关）
			CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 40.f);

			// 最终准星散布 = 基础值 + 移动 + 腾空 - 瞄准 + 射击
			// 瞄准是减项（收束准星），其余是加项（扩大准星）
			HUDPackage.CrosshairsSpread = 0.5f +
			CrosshairVelocityFactor+
			CrosshairInAirFactor-
			CrosshairAimFactor+
			CrosshairShootingFactor;

			// 将数据包交给 HUD，DrawHUD() 下一帧就会用新的纹理绘制准星
			HUD->SetHUDPackage(HUDPackage);
		}
	}
}

void UCombatComponent::InterpFOV(float DeltaTime)
{
	if (EquippedWeapon == nullptr || Character == nullptr) return;
	if (bAiming)
	{
		// 开镜：当前 FOV → 武器的瞄准视野，速度由武器决定（每把枪手感不同）
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
	}
	else
	{
		// 收镜：当前 FOV → 腰射基准视野，速度由 CombatComponent 统一控制
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
	}
	// 将插值后的结果写入相机
	if(Character && Character->GetFollowCamera())
	{
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget)
{
	if (Character && Character->IsLocallyControlled()) return;
	LocalFire(TraceHitTarget);
}

void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget, float FireDelay)
{
	MulticastFire(TraceHitTarget);
}

void UCombatComponent::ServerShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay)
{
	MulticastShotgunFire(TraceHitTargets);
}

void UCombatComponent::MulticastShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets)
{
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	ShotgunLocalFire(TraceHitTargets);
}

void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming)
{
	bAiming = bIsAiming;
	if(Character){
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	if(Character){
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

		if(Character->GetFollowCamera())
		{
			// 记录相机原始 FOV 作为腰射基准，后续收镜时恢复到此值
			DefaultFOV = Character->GetFollowCamera()->FieldOfView;
			CurrentFOV = DefaultFOV;
		}
	}
	
}


void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	

	// 只对本地玩家运行：射线检测、准星绘制、FOV 平滑
	if (Character && Character->IsLocallyControlled() && EquippedWeapon)
	{
		SetHUDCrosshairs(DeltaTime);
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		HitTarget = HitResult.ImpactPoint;
		
		InterpFOV(DeltaTime);
	}

}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
	DOREPLIFETIME(UCombatComponent, CombatState);
}

void UCombatComponent::OnRep_CombatState()
{
	if (Character == NULL) return;
	switch (CombatState)
	{
	case ECombatState::ECS_Reloading:
		if (Character && !Character->IsLocallyControlled()) HandReload();
		break;
	case ECombatState::ECS_Unoccupied:
		if (bFireButtonPressed)
		{
			Fire();
		}
		break;
	}
}
void  UCombatComponent::HandReload()
{
	Character->PlayReloadMontage();
}

int32 UCombatComponent::AmountToReload()
{
	if (EquippedWeapon == nullptr) return 0;
	int32 RoomInMag = EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo();
	int32 AmountSpare = EquippedWeapon->GetSpareAmmo();
	return FMath::Min(RoomInMag, AmountSpare);
}

bool UCombatComponent::PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount)
{
	// 只有装备了同类型武器才添加备弹（CS:GO 规则：子弹类型必须匹配当前枪支）
	if (EquippedWeapon && EquippedWeapon->GetWeaponType() == WeaponType)
	{
		EquippedWeapon->AddToSpare(AmmoAmount);

		// 类型匹配：如果弹匣为空，自动触发换弹（用户体验优化）
		if (EquippedWeapon->IsEmpty())
		{
			Reload();
		}
		return true; // 拾取成功，通知调用方可以销毁拾取物
	}

	return false; // 拾取失败（无装备武器或类型不匹配），通知调用方显示提示
}


