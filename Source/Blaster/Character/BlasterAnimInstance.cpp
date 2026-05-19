// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterAnimInstance.h"
#include "BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

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
    bWeaponEquipped = BlasterCharacter->IsWeaponEquipped();

    bIsCrouched = BlasterCharacter->bIsCrouched;
    bAiming = BlasterCharacter->IsAiming();
    TurningInPlace = BlasterCharacter->GetTurningInPlace();


    FRotator AimRotation = BlasterCharacter->GetBaseAimRotation();//定义了一个旋转值，记录"你面朝哪里"。
    //把角色当前的面朝方向（准星/摄像机方向）存到 AimRotation 里。这个旋转决定了动画蓝图中上半身的朝向，比如瞄准时身体扭转的角度。

    FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(BlasterCharacter->GetVelocity());//定义了一个旋转值，记录"你在往哪里走"。
    //GetVelocity()	获取角色当前的速度向量（大小+方向），比如"向东 600cm/s"
    //MakeRotFromX(...)	把速度方向转换成旋转值，回答"朝这个方向跑需要面朝多少度"
    //AimRotation        = 面朝哪（上半身）    → 比如 90°（向东看）
    //MovementRotation   = 往哪跑（下半身）    → 比如 180°（向南跑）
    //两者一减就是上下身分离角度——动画蓝图用它来决定腿朝哪个方向跑、上半身要不要扭转，实现"边后退边开枪"的效果。
    FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
    DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaTime, 6.f);
    YawOffset = DeltaRotation.Yaw;//动画蓝图用它来决定腿朝哪个方向跑，实现"边后退边开枪"的效果。

    CharacterRotationLastFrame = CharacterRotation;
    CharacterRotation= BlasterCharacter->GetActorRotation();
    const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
    const float Target = Delta.Yaw / DeltaTime;
    const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f);
    Lean = FMath::Clamp(Interp, -90.f, 90.f);

    AO_Yaw=BlasterCharacter->GetAO_Yaw();
    AO_Pitch=BlasterCharacter->GetAO_Pitch();
}