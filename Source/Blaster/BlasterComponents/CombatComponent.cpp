


#include "CombatComponent.h"
#include "../Character/BlasterCharacter.h"
#include "Blaster/Weapon/Weapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UCombatComponent::UCombatComponent()
{
	
	PrimaryComponentTick.bCanEverTick = false;

	BaseWalkSpeed=600.f;
	AimWalkSpeed=300.f;
}

void UCombatComponent::EquipWeapon(AWeapon *WeaponToEquip)//此函数从始至终都只会在服务器上面执行
{
	if(Character==NULL)return;
	if(WeaponToEquip==NULL)return;

	EquippedWeapon=WeaponToEquip;
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);//设置武器状态为已装备
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if(HandSocket)
	{
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());//把武器附着到角色的右手骨骼上
	}

	EquippedWeapon->SetOwner(Character);//设置武器的拥有者为角色（ue内置函数），客户端能够得到消息已经被设置为武器拥有者
	//EquippedWeapon->SetOwner(Character);                          // Owner 关系复制
	//HandSocket->AttachActor(EquippedWeapon, Character->GetMesh()); // 附着关系复制
	//这两行代码
	// 	UE 自动把这三样东西同步给所有客户端：
	// 	复制内容			客户端效果
	// Actor存在性			武器 Actor 在客户端被创建
	// Owner关系			客户端知道这个武器属于谁
	// 附着到骨骼			武器出现在角色右手上


	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
	
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
}

void UCombatComponent::OnRep_EquippedWeapon()
{
	if(EquippedWeapon && Character){
		//装备武器时，切换角色旋转模式：从"移动朝向"变成"控制器朝向"。
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
	}
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
	}
}


void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);


}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
}
