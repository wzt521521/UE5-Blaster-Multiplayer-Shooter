#include "BuffComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UBuffComponent::UBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBuffComponent::BeginPlay()
{
	Super::BeginPlay();
	// 缓存角色引用，避免每次 Cast 的开销
	Character = Cast<ABlasterCharacter>(GetOwner());
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

	// 清除旧 Timer，避免重复 Buff 时旧 Timer 提前恢复
	if (GetWorld()->GetTimerManager().IsTimerActive(SpeedBuffTimer))
	{
		GetWorld()->GetTimerManager().ClearTimer(SpeedBuffTimer);
	}

	// 服务器本地立刻生效，Multicast RPC 推送到所有客户端
	Character->GetCharacterMovement()->MaxWalkSpeed = BuffBaseSpeed;
	Character->GetCharacterMovement()->MaxWalkSpeedCrouched = BuffCrouchSpeed;

	MulticastSpeedBuff(BuffBaseSpeed, BuffCrouchSpeed);

	// 启动 Timer，到期自动恢复原始值
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

	// 恢复到拾取 Buff 前记录的原始速度
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
