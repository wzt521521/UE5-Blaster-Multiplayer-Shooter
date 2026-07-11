#pragma once

UENUM(BlueprintType)
enum class ECombatState : uint8
{
    ECS_Unoccupied UMETA(DisplayName = "Unoccupied"),
    ECS_FireTimerInProgress UMETA(DisplayName = "FireTimerInProgress"),
    ECS_Reloading UMETA(DisplayName = "Reloading"),
    ECS_Equipping UMETA(DisplayName = "Equipping"),
    ECS_Stunned UMETA(DisplayName = "Stunned"),
    ECS_ThrowingGrenade UMETA(DisplayName = "ThrowingGrenade"),
    ECS_Dead UMETA(DisplayName = "Dead")
};