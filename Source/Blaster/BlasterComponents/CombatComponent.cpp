


#include "CombatComponent.h"
#include "Blaster/BlasterCharacter.h"
#include "Blaster/Weapon/Weapon.h"
#include "Engine/SkeletalMeshSocket.h"

UCombatComponent::UCombatComponent()
{
	
	PrimaryComponentTick.bCanEverTick = false;

	
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
	
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	
	
}


void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);


}

