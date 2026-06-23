// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterAnimInstance.h"
#include "BlasterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Blaster/BlasterTypes/CombatState.h"
#include "Blaster/Weapon/Weapon.h"

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
    bElimmed = BlasterCharacter->IsElimmed();//从角色类中获取是否死亡
    FVector Velocity = BlasterCharacter->GetVelocity();//获取角色的速度
    Velocity.Z = 0;//只考虑水平速度
    Speed = Velocity.Size();//计算速度的大小

    bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling();//判断角色是否在空中

    bIsAccelerating = BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0;//判断角色是否在加速
    bWeaponEquipped = BlasterCharacter->IsWeaponEquipped();
    EquippedWeapon = BlasterCharacter->GetEquippedWeapon();

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

    // ————————————————————————————————————————————
    // Lean（倾斜）：根据旋转速率计算角色身体侧倾角度，模拟"离心力"效果
    // 当角色快速转身时，身体会向旋转方向的反方向倾斜，类似摩托车压弯
    // ————————————————————————————————————————————
    CharacterRotationLastFrame = CharacterRotation;                                      // 保存上一帧的朝向
    CharacterRotation = BlasterCharacter->GetActorRotation();                            // 获取当前帧的朝向
    const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(                   // 计算两帧之间的旋转差值（标准化到 [-180,180]）
        CharacterRotation, CharacterRotationLastFrame);
    const float Target = Delta.Yaw / DeltaTime;                                          // 旋转速率（度/秒）= Yaw差值 ÷ 帧间隔
    const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f);                // 平滑插值到目标速率，避免数值突变
    Lean = FMath::Clamp(Interp, -90.f, 90.f);                                            // 限制倾斜角度在 ±90° 范围内

    // ————————————————————————————————————————————
    // AO（Aim Offset）：瞄准偏移值，记录"眼睛看向哪里"与"身体朝向哪里"的差值
    // AO_Yaw：左右偏移   AO_Pitch：上下偏移
    // 在动画蓝图中用于叠加瞄准姿态，如角色朝前跑但准星向右上方瞄准时，上半身会相应扭转
    // ————————————————————————————————————————————
    AO_Yaw = BlasterCharacter->GetAO_Yaw();
    AO_Pitch = BlasterCharacter->GetAO_Pitch();

    // ————————————————————————————————————————————
    // IK（Inverse Kinematics）：将角色的手"吸附"到武器的正确握持位置
    // ————————————————————————————————————————————
    if (bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetWeaponMesh() && BlasterCharacter->GetMesh())
    {
        // 【左手IK】获取武器上 LeftHandSocket 的世界坐标，转换到角色右手骨骼的局部空间
        // 这样动画蓝图就可以用 IK 将左手拉到这个位置，实现双手握枪
        LeftHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(          // 获取武器上左手插槽的世界变换
            FName("LeftHandSocket"), ERelativeTransformSpace::RTS_World);
        FVector OutPosition;
        FRotator OutRotation;
        BlasterCharacter->GetMesh()->TransformToBoneSpace(                                // 将左手插槽的世界位置转换到右手骨骼空间
            FName("Hand_R"),                                                              // 参考骨骼：右手
            LeftHandTransform.GetLocation(),                                              // 输入：左手插槽世界位置
            FRotator::ZeroRotator,                                                        // 只转换位置，不关心旋转
            OutPosition,                                                                  // 输出：相对于右手骨骼的位置
            OutRotation);                                                                 // 输出：相对于右手骨骼的旋转
        LeftHandTransform.SetLocation(OutPosition);                                       // 用转换后的相对位置更新左手变换
        LeftHandTransform.SetRotation(FQuat(OutRotation));                                // 用转换后的相对旋转更新左手变换

        // 【右手瞄准旋转】仅本地控制的客户端执行——让持枪的右手始终指向准星目标
        if (BlasterCharacter->IsLocallyControlled())
        {
            bLocallyControlled = true;                                                     // 标记为本地玩家，动画蓝图据此区分表现
            FTransform RightHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(  // 获取武器上右手插槽的世界变换
                FName("hand_r"), ERelativeTransformSpace::RTS_World);

            // 计算右手到准星目标的方向：从右手位置出发，指向 HitTarget
            // HitTarget 是屏幕准星在三维空间中的命中点（通过射线检测获得）
            FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
                RightHandTransform.GetLocation(),
                RightHandTransform.GetLocation() + (RightHandTransform.GetLocation() - BlasterCharacter->GetHitTarget()));

            RightHandRotation = FMath::RInterpTo(                                          // 平滑插值右手朝向，速度 30，使瞄准动作流畅
                RightHandRotation, LookAtRotation, DeltaTime, 30.f);
        }
    }
    bUseFABRIK = BlasterCharacter->GetCombatState() != ECombatState::ECS_Reloading;
}