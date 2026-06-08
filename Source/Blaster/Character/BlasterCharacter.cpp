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
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "BlasterAnimInstance.h"
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

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;


}	


void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("Char Capsule [%s]: CollisionEnabled=%d, ResponseToWorldDynamic=%d"),
		*UEnum::GetValueAsString(GetLocalRole()),
		GetCapsuleComponent()->GetCollisionEnabled(),
		GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_WorldDynamic));
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

}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if(Combat)
	{
		Combat->Character = this;//让Combat组件知道它所属的角色是谁
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


  
void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);//注册要复制的变量
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AimOffset(DeltaTime);
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
	if(Combat)//只有服务器才处理装备逻辑，必须在服务器上执行
	{
		if(HasAuthority())//服务器主机玩家直接装备
		Combat->EquipWeapon(OverlappingWeapon);
		else{//客户端玩家向服务器发送装备请求
			ServerEquipWeapon(OverlappingWeapon);
		}
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

void ABlasterCharacter::ServerEquipWeapon_Implementation(AWeapon *WeaponToEquip)
{
	if(Combat)
	{
		Combat->EquipWeapon(WeaponToEquip);
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
