#pragma once

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    EWT_AssaultRifle UMETA(DisplayName = "Assault Rifle"),
    EWT_SubmachineGun UMETA(DisplayName = "Submachine Gun"),
    EWT_MAX UMETA(DisplayName = "DefaultMAX")
};