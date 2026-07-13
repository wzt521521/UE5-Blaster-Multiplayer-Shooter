#include "BuffComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UBuffComponent::UBuffComponent()
{
	// 开启 Tick：护盾 ramp-up 需要每帧递增
	PrimaryComponentTick.bCanEverTick = true;
}

void UBuffComponent::BeginPlay()
{
	Super::BeginPlay();
	// 缓存角色引用，避免每次 Cast 的开销
	Character = Cast<ABlasterCharacter>(GetOwner());
}

void UBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 每帧驱动护盾平滑递增
	ShieldRampUp(DeltaTime);
}

void UBuffComponent::SetInitialSpeeds(float BaseSpeed, float CrouchSpeed)
{
	InitialBaseSpeed = BaseSpeed;
	InitialCrouchSpeed = CrouchSpeed;
}

void UBuffComponent::SetInitialJumpVelocity(float Velocity)
{
	InitialJumpVelocity = Velocity;
}

void UBuffComponent::BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffTime)
{
	if (Character == nullptr || Character->GetCharacterMovement() == nullptr) return;

	if (GetWorld()->GetTimerManager().IsTimerActive(SpeedBuffTimer))
	{
		GetWorld()->GetTimerManager().ClearTimer(SpeedBuffTimer);
	}

	Character->GetCharacterMovement()->MaxWalkSpeed = BuffBaseSpeed;
	Character->GetCharacterMovement()->MaxWalkSpeedCrouched = BuffCrouchSpeed;

	MulticastSpeedBuff(BuffBaseSpeed, BuffCrouchSpeed);

	GetWorld()->GetTimerManager().SetTimer(
		SpeedBuffTimer,
		this,
		&UBuffComponent::ResetSpeeds,
		BuffTime
	);
}

void UBuffComponent::BuffJump(float BuffJumpVelocity, float BuffTime)
{
	if (Character == nullptr || Character->GetCharacterMovement() == nullptr) return;

	if (GetWorld()->GetTimerManager().IsTimerActive(JumpBuffTimer))
	{
		GetWorld()->GetTimerManager().ClearTimer(JumpBuffTimer);
	}

	Character->GetCharacterMovement()->JumpZVelocity = BuffJumpVelocity;
	MulticastJumpBuff(BuffJumpVelocity);

	GetWorld()->GetTimerManager().SetTimer(
		JumpBuffTimer,
		this,
		&UBuffComponent::ResetJump,
		BuffTime
	);
}

void UBuffComponent::ResetSpeeds()
{
	if (Character == nullptr || Character->GetCharacterMovement() == nullptr) return;

	Character->GetCharacterMovement()->MaxWalkSpeed = InitialBaseSpeed;
	Character->GetCharacterMovement()->MaxWalkSpeedCrouched = InitialCrouchSpeed;

	MulticastSpeedBuff(InitialBaseSpeed, InitialCrouchSpeed);
}

void UBuffComponent::ResetJump()
{
	if (Character == nullptr || Character->GetCharacterMovement() == nullptr) return;

	Character->GetCharacterMovement()->JumpZVelocity = InitialJumpVelocity;
	MulticastJumpBuff(InitialJumpVelocity);
}

// ——————————————————————————————————————————————————————
// Shield 护盾系统
// 拾取后不是瞬间加盾，而是 Tick 每帧递增（平滑 ramp-up）
// 多次拾取可叠加：ShieldReplenishAmount 累加
// ——————————————————————————————————————————————————————
void UBuffComponent::ReplenishShield(float ShieldAmount, float ReplenishTime)
{
	bReplenishingShield = true;
	ShieldReplenishRate = ShieldAmount / ReplenishTime; // 计算每秒增加量
	ShieldReplenishAmount += ShieldAmount;              // 累加，支持多次拾取叠加
}

void UBuffComponent::ShieldRampUp(float DeltaTime)
{
	if (!bReplenishingShield || Character == nullptr || Character->IsElimmed()) return;

	const float ReplenishThisFrame = ShieldReplenishRate * DeltaTime;

	// 逐帧增加护盾值，上限为 MaxShield
	Character->SetShield(FMath::Clamp(
		Character->GetShield() + ReplenishThisFrame, 0.f, Character->GetMaxShield()));
	Character->UpdateHUDShield();

	ShieldReplenishAmount -= ReplenishThisFrame;

	// 护盾已满或总量已用完 → 停止 ramp-up
	if (ShieldReplenishAmount <= 0.f || Character->GetShield() >= Character->GetMaxShield())
	{
		bReplenishingShield = false;
		ShieldReplenishAmount = 0.f;
	}
}

// ——————————————————————————————————————————————————————
// NetMulticast RPC：服务器调用，在所有客户端（含服务器自身）执行
// CharacterMovementComponent 属性不复制，只能通过 RPC 让各客户端本地修改
// ——————————————————————————————————————————————————————
void UBuffComponent::MulticastSpeedBuff_Implementation(float BaseSpeed, float CrouchSpeed)
{
	if (Character == nullptr || Character->GetCharacterMovement() == nullptr) return;

	Character->GetCharacterMovement()->MaxWalkSpeed = BaseSpeed;
	Character->GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
}

void UBuffComponent::MulticastJumpBuff_Implementation(float JumpVelocity)
{
	if (Character == nullptr || Character->GetCharacterMovement() == nullptr) return;

	Character->GetCharacterMovement()->JumpZVelocity = JumpVelocity;
}
