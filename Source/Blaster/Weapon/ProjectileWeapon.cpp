// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Projectile.h"

void AProjectileWeapon::Fire(const FVector &HitTarget)
{
    Super::Fire(HitTarget);//播放动画
    if(!HasAuthority()) return;//如果不是服务器，就返回


    //下面的整体逻辑为从枪口位置射出一个投射物，飞向准星瞄准的那个点。
    APawn* InstigatorPawn = Cast<APawn>(GetOwner());
    const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
    if (MuzzleFlashSocket)
    {
        FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
        FVector ToTarget = HitTarget - SocketTransform.GetLocation();
        FRotator TargetRotation = ToTarget.Rotation();
        if(ProjectileClass&&InstigatorPawn)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = GetOwner();
            SpawnParams.Instigator = InstigatorPawn;
            UWorld* World = GetWorld();
            if(World){
                World->SpawnActor<AProjectile>(ProjectileClass, SocketTransform.GetLocation(), 
                TargetRotation, SpawnParams);
            }
        }
    }
    //===========================================================
}
