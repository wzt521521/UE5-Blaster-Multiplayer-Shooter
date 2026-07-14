// Fill out your copyright notice in the Description page of Project Settings.
#include "BlasterCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "../Weapon/Weapon.h"
#include "../BlasterComponents/CombatComponent.h"
#include "Blaster/BlasterComponents/BuffComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blaster/Blaster.h"
#include "Kismet/KismetMathLibrary.h"
#include "BlasterAnimInstance.h"
#include "../PlayerController/BlasterPlayerController.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "TimerManager.h"
#include "Blaster/Weapon/WeaponTypes.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"


ABlasterCharacter::ABlasterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationYaw=false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat= CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	Buff = CreateDefaultSubobject<UBuffComponent>(TEXT("BuffComponent"));
	Buff->SetIsReplicated(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;


}	


void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
	UpdateHUDHealth();
	UpdateHUDShield();
	// ————————————————————————————————————————————
	// 血量 UI 初始化：把初始血量发送到屏幕上的血条
	// 这里的调用时机是 BeginPlay（只执行一次），所以只负责 UI 初始显示
	// 后续血量变化由 OnRep_Health 驱动
	// ————————————————————————————————————————————
	
	if(HasAuthority()){
		OnTakeAnyDamage.AddDynamic(this,&ABlasterCharacter::ReceiveDamage);
	}
}

void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ABlasterCharacter::Jump);

	PlayerInputComponent->BindAxis("MoveForward", this, &ABlasterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABlasterCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ABlasterCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ABlasterCharacter::LookUp);

	PlayerInputComponent->BindAction("Equip", IE_Pressed, this, &ABlasterCharacter::EquipButtonPressed);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ABlasterCharacter::CrouchButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ABlasterCharacter::AimButtonPressed);
	PlayerInputComponent->BindAction("Aim", IE_Released, this, &ABlasterCharacter::AimButtonReleased);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ABlasterCharacter::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ABlasterCharacter::FireButtonReleased);

	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ABlasterCharacter::ReloadButtonPressed);
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if(Combat)
	{
		Combat->Character = this;//让Combat组件知道它所属的角色是谁
	}

	// 记录角色原始移动属性，Buff 到期后恢复
	if (Buff && GetCharacterMovement())
	{
		Buff->SetInitialSpeeds(GetCharacterMovement()->MaxWalkSpeed, GetCharacterMovement()->MaxWalkSpeedCrouched);
		Buff->SetInitialJumpVelocity(GetCharacterMovement()->JumpZVelocity);
	}
}

void ABlasterCharacter::PlayFireMontage(bool bAiming)
{
	if(Combat==NULL||Combat->EquippedWeapon==NULL)return;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();//AnimInstance 是角色网格体的动画实例，负责播放动画蒙太奇
	if(AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}

}

void ABlasterCharacter::PlayReloadMontage()
{
	if(Combat==NULL||Combat->EquippedWeapon==NULL)return;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if(AnimInstance && ReloadMontage)
	{
		AnimInstance->Montage_Play(ReloadMontage);
		FName SectionName;
		switch(Combat->EquippedWeapon->GetReloadType()){
			case EWeaponType::EWT_AssaultRifle:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_RocketLauncher:
				SectionName = FName("RocketLauncher");
				break;
			case EWeaponType::EWT_SubmachineGun:
				SectionName = FName("Pistol");
				break;
			case EWeaponType::EWT_SniperRifle:
				SectionName = FName("SniperRifle");
				break;
			case EWeaponType::EWT_GrenadeLauncher:
				SectionName = FName("GrenadeLauncher");
				break;
			default:
				SectionName = FName("Rifle");
				break;
		}
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayHitReactMontage()
{
	if(Combat == NULL) return; // 受击动画不应要求武器——重生后未拾取武器时也需要播放
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if(AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName = FName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayElimMontage()//只负责播放动画
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if(AnimInstance && ElimMontage)
	{
		AnimInstance->Montage_Play(ElimMontage);
	}
}

void ABlasterCharacter::Elim()
{
	// 掉落所有武器
	DropOrDestroyWeapons();
	MulticastElim();
	// 延迟后自动回调 ElimTimerFinished，给死亡动画留出播放时间
	GetWorldTimerManager().SetTimer(
		ElimTimer,                                       // FTimerHandle 句柄
		this,                                            // 回调对象 = 当前角色
		&ABlasterCharacter::ElimTimerFinsished,          // 回调函数
		ElimDelay                                        // 延迟秒数
	);
}

void ABlasterCharacter::MulticastElim_Implementation()//MulticastElim只负责多播，其他逻辑由另一个Elim函数处理
{
	if(BlasterPlayerController){
		BlasterPlayerController->SetHUDWeaponAmmo(0);
	}
	bElimmed = true;
	PlayElimMontage();

	// 死亡时关闭狙击镜 Scope Widget
	if (IsLocallyControlled() && Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle)
	{
		ShowSniperScopeWidget(false);
	}

	//禁用碰撞
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
	if(BlasterPlayerController){
		DisableInput(BlasterPlayerController);
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);//禁用碰撞
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);//禁用碰撞
}

void ABlasterCharacter::ElimTimerFinsished()
{
	ABlasterGameMode* BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	if(BlasterGameMode)
	{
		BlasterGameMode->RequestRespawn(this, Controller);//调用复活
	}
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);//注册要复制的变量
	DOREPLIFETIME(ABlasterCharacter, Health);
	DOREPLIFETIME(ABlasterCharacter, Shield);
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AimOffset(DeltaTime);
	HideCameraIfCharacterClose();
	PollInit();
}



void ABlasterCharacter::MoveForward(float Value)
{
	if(Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::MoveRight(float Value)
{
	if(Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::Turn(float Value)
{
	AddControllerYawInput(Value);
}

void ABlasterCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void ABlasterCharacter::EquipButtonPressed()
{
	if (Combat && Combat->CombatState == ECombatState::ECS_Unoccupied)
	{
		ServerEquipButtonPressed();
	}
}

void ABlasterCharacter::CrouchButtonPressed()
{
	if(bIsCrouched){
		UnCrouch();
	}
	else{
		Crouch();
	}
	
}

void ABlasterCharacter::ReloadButtonPressed()
{
	if(Combat)
	{
		Combat->Reload();
	}
}

void ABlasterCharacter::AimButtonPressed()
{
	if(Combat)
	{
		Combat->SetAiming(true);
	}
}

void ABlasterCharacter::AimButtonReleased()
{
	if(Combat)
	{
		Combat->SetAiming(false);
	}
}

void ABlasterCharacter::AimOffset(float DeltaTime)//AimOffset 在每台机器上都跑
//包括服务器和所有客户端。所以每台机器都在本地算自己的 AO_Pitch，都在本地做同样的修正，最终都修正到了正确的值。
{
	if(Combat && Combat->EquippedWeapon ==NULL)return;
	FVector Velocity = GetVelocity();
	Velocity.Z = 0;
	//计算 Speed（水平速度）和 bIsInAir
	float Speed = Velocity.Size();
	bool bIsInAir = GetCharacterMovement()->IsFalling();


	if(Speed == 0.f ||! bIsInAir)//不在空中
	{
		//CurrentAimRotation = 当前瞄准 Yaw
		FRotator CurrentAimRotation = FRotator(0.f,GetBaseAimRotation().Yaw,0.f);
		//DeltaAimRotation = CurrentAimRotation - StartAimRotation
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation,StartAimRotation);
		//AO_Yaw = DeltaAimRotation.Yaw          ← 瞄准方向偏离基准的角度
		AO_Yaw = DeltaAimRotation.Yaw;
		//if (不在转身中): InterpAO_Yaw = AO_Yaw ← 保存原始值
		if(TurningInPlace==ETurningInPlace::ETIP_NotTurning){
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;//角色旋转模式：控制器旋转，不使用角色旋转
		TurnInPlace(DeltaTime);//判断是否触发转身
	}
	if(Speed>0.f||bIsInAir){
		StartAimRotation = FRotator(0.f,GetBaseAimRotation().Yaw,0.f);
		AO_Yaw=0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace=ETurningInPlace::ETIP_NotTurning;
	}
	AO_Pitch=GetBaseAimRotation().Pitch;
	//角色上下镜头移动的时候，ue会自动复制底层的pitch角度，然后再自动复制底层角度到其他的机器
	//其他的机器再通过AO_Pitch=GetBaseAimRotation().Pitch;来赋值AO_Pitch，以期望更新角度
	//但是GetBaseAimRotation().Pitch在其他机器的解码环节会出现问题，导致AO_Pitch的值不对
	//fix 做的就是这件事：把 270°-360° 这种"无符号低头"映射回正确的 -90°-0°。
	if(AO_Pitch>90.f&&!IsLocallyControlled()){
		FVector2D InRange(270.f,360.f);
		FVector2D OutRange(-90.f,0.f);
		AO_Pitch=FMath::GetMappedRangeValueClamped(InRange,OutRange,AO_Pitch);
	}
}

void ABlasterCharacter::Jump()
{
	if(bIsCrouched){
		UnCrouch();
	}
	else{
		Super::Jump();
	}
}

void ABlasterCharacter::FireButtonPressed()
{
	if(Combat)
	{
		Combat->FireButtonPressed(true);//由combat组件处理开火逻辑，参数 true 表示按下开火键
	}
}

void ABlasterCharacter::FireButtonReleased()
{
	if(Combat)
	{
		Combat->FireButtonPressed(false);
	}
}

//这个函数只会在服务器上面调用
void ABlasterCharacter::ReceiveDamage(AActor *DamagedActor, float Damage, const UDamageType *DamageType, AController *InstigatorController, AActor *DamageCauser)
{
	// ————————————————————————————————————————————
	// 护盾先吸收伤害，护盾不够了才扣血
	// ————————————————————————————————————————————
	float DamageToHealth = Damage;
	if (Shield > 0.f)
	{
		if (Shield >= Damage)           // 护盾能完全吸收
		{
			Shield = FMath::Clamp(Shield - Damage, 0.f, MaxShield);
			DamageToHealth = 0.f;
		}
		else                            // 护盾不够，先扣光护盾，剩余伤害扣血
		{
			DamageToHealth = FMath::Clamp(DamageToHealth - Shield, 0.f, Damage);
			Shield = 0.f;
		}
	}

	Health = FMath::Clamp(Health - DamageToHealth, 0.f, MaxHealth);
	UpdateHUDHealth();
	UpdateHUDShield();
	PlayHitReactMontage();

	if(Health <= 0.f){
		ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();//先获取游戏模式
		if(BlasterGameMode){
			BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
			ABlasterPlayerController* AttackerController = Cast<ABlasterPlayerController>(InstigatorController);
			BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackerController);
		}
	}
	
}

void ABlasterCharacter::UpdateHUDHealth()
{
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void ABlasterCharacter::PollInit()
{
	if (BlasterPlayerController == nullptr)
	{
		BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
		if (BlasterPlayerController)
		{
			SpawDefaultWeapon();
			UpdateHUDHealth();
			UpdateHUDShield();
		}
	}
}

void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)//参数自动传入
//当客户端收到服务器传来的新值时，客户端会执行这个函数
{
	if(OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}

	if(LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	if (Combat && OverlappingWeapon)
	{
		Combat->EquipWeapon(OverlappingWeapon);
	}
}

void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
	if(AO_Yaw>90.f){
		TurningInPlace=ETurningInPlace::ETIP_Right;
	}
	else if(AO_Yaw<-90.f){
		TurningInPlace=ETurningInPlace::ETIP_Left;
	}
	else{
		TurningInPlace=ETurningInPlace::ETIP_NotTurning;
	}

	//转身的平滑收尾阶段。
	//不直接跳变，而是让 AO_Yaw 从当前值插值逼近 0（速度 4.0），等转到 15° 以内就判定转身完成，重置基准方向。
	if(TurningInPlace!=ETurningInPlace::ETIP_NotTurning){
		InterpAO_Yaw=FMath::FInterpTo(InterpAO_Yaw,0.f,DeltaTime,4.f);
		AO_Yaw=InterpAO_Yaw;
		if(FMath::Abs(AO_Yaw)<15.f){
			TurningInPlace=ETurningInPlace::ETIP_NotTurning;
			StartAimRotation=FRotator(0.f,GetBaseAimRotation().Yaw,0.f);
		}
	}

}


void ABlasterCharacter::HideCameraIfCharacterClose()
{
	// ————————————————————————————————————————————
	// 摄像机近身透视防护：当摄像机离角色太近时（如被墙壁挤压、滚轮拉近），
	// 隐藏角色身体和武器模型，防止摄像机穿透到模型内部看到"空心"或"眼部穿模"
	// 只在本地玩家执行——远程玩家看到的第三人称视角不需要此处理
	// ————————————————————————————————————————————
	if (!IsLocallyControlled()) return;                                                     // 只处理本地控制端，不需要画蛇添足管别人的画面

	// 计算摄像机到角色中心点之间的距离，如果小于阈值（200cm）则认为摄像机进入了角色体内
	// CameraThreshold = 200.f，约2米，足以覆盖角色胶囊体半径（约34cm）+ 弹簧臂最小长度
	float DistanceToCamera = (FollowCamera->GetComponentLocation() - GetActorLocation()).Size();
	if (DistanceToCamera < CameraThreshold)
	{
		// 摄像机太近 → 隐藏角色本体，否则画面会被角色的脑袋/肩膀挡住
		GetMesh()->SetVisibility(false);

		// 同时隐藏武器模型（对所有者不可见），否则枪口会"怼进"摄像机镜头里
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		// 摄像机已经退回到安全距离 → 恢复显示角色和武器
		GetMesh()->SetVisibility(true);

		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}


void ABlasterCharacter::OnRep_Health()
{
	UpdateHUDHealth();
	PlayHitReactMontage();
}

void ABlasterCharacter::OnRep_Shield(float LastShield)
{
	UpdateHUDShield();
	// 护盾减少（受伤）时播放受击动画
	if (Shield < LastShield)
	{
		PlayHitReactMontage();
	}
}

void ABlasterCharacter::UpdateHUDShield()
{
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDShield(Shield, MaxShield);
	}
}

void ABlasterCharacter::Heal(float HealAmount)
{
	// 仅服务器修改复制属性 Health，客户端通过 OnRep_Health 更新 HUD
	if (!HasAuthority()) return;
	Health = FMath::Clamp(Health + HealAmount, 0.f, MaxHealth);
	UpdateHUDHealth();
}

void ABlasterCharacter::SetOverlappingWeapon(AWeapon *Weapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}

	OverlappingWeapon = Weapon;//此变量被标记为复制变量，ue会先调用GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps)函数进行复制
	//然后调用OnRep_OverlappingWeapon(AWeapon* LastWeapon)函数处理后续逻辑


	if(IsLocallyControlled() && OverlappingWeapon)//只有本地玩家才显示拾取提示UI
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
}

bool ABlasterCharacter::IsWeaponEquipped()
{
	return (Combat && Combat->EquippedWeapon);
}

AWeapon* ABlasterCharacter::GetEquippedWeapon() const
{
	if (Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}

ECombatState ABlasterCharacter::GetCombatState() const
{
	if (Combat == nullptr) return ECombatState::ECS_Unoccupied;
	return Combat->CombatState;
}

bool ABlasterCharacter::IsAiming()
{
    return (Combat && Combat->bAiming);
}

FVector ABlasterCharacter::GetHitTarget() const
{
	if(Combat==NULL)
    return FVector();
	return Combat->HitTarget;
}

void ABlasterCharacter::SpawDefaultWeapon()
{
	ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();
	UWorld* World = GetWorld();
	if (BlasterGameMode && World && !bElimmed && DefaultWeaponClass)
	{
		AWeapon* StartingWeapon = World->SpawnActor<AWeapon>(DefaultWeaponClass);
		StartingWeapon->bDestroyWeapon = true;
		if (Combat)
		{
			Combat->EquipWeapon(StartingWeapon);
		}
	}
}

void ABlasterCharacter::DropOrDestroyWeapon(AWeapon* Weapon)
{
	if (Weapon == nullptr) return;
	if (Weapon->bDestroyWeapon)
	{
		Weapon->Destroy();
	}
	else
	{
		Weapon->Dropped();
	}
}

void ABlasterCharacter::DropOrDestroyWeapons()
{
	if (Combat && Combat->EquippedWeapon)
	{
		DropOrDestroyWeapon(Combat->EquippedWeapon);
	}
}
