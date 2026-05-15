// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterAnimInstance.h"
#include "../BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
void UBlasterAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());

    
}

void UBlasterAnimInstance::NativeUpdateAnimation(float DeltaTime)//每帧调用
{
    Super::NativeUpdateAnimation(DeltaTime);

    if (BlasterCharacter==NULL){
        BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
    }
    
    if(BlasterCharacter==NULL) return;

    FVector Velocity = BlasterCharacter->GetVelocity();//获取角色的速度
    Velocity.Z = 0;//只考虑水平速度
    Speed = Velocity.Size();//计算速度的大小

    bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling();//判断角色是否在空中

    bIsAccelerating = BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0;//判断角色是否在加速
    
}