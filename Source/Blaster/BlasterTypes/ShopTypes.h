// ──────────────────────────────────────────────────
// Blaster 武器购买系统 — 商店类型定义
// ──────────────────────────────────────────────────
// 包含:
//   EShopItemCategory  — 物品类别枚举（DataTable Category 列）
//   EBuffType           — Buff 类型枚举（DataTable + BuffComponent 共用）
//   FShopItemRow        — DataTable 行结构（每行 = 一个可购买商品）
//
// 依赖关系:
//   - 依赖 WeaponTypes.h（EWeaponType 枚举，用于弹药匹配）
//   - 依赖 ThrowableTypes.h（EThrowableType 枚举，用于投掷物类型）
//   - 被 GameMode/PlayerController/BuyMenu/GameState 引用

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Blaster/WeaponSystem/Weapon/WeaponTypes.h"
#include "Blaster/BlasterTypes/ThrowableTypes.h"
// TSubclassOf 需要完整 UCLASS 定义 — Weapon.h 不依赖 ShopTypes.h，无循环风险
#include "Blaster/WeaponSystem/Weapon/Weapon.h"
#include "ShopTypes.generated.h"


// ============================================================
// EShopItemCategory：物品类别
// ============================================================
// 用途：DataTable 中 Category 列的值，驱动 ProcessPurchase 的 switch 分发
UENUM(BlueprintType)
enum class EShopItemCategory : uint8
{
    ESIC_None      UMETA(DisplayName = "None"),
    ESIC_Weapon    UMETA(DisplayName = "Weapon"),
    ESIC_Ammo      UMETA(DisplayName = "Ammo"),
    ESIC_Throwable UMETA(DisplayName = "Throwable"),
    ESIC_Buff      UMETA(DisplayName = "Buff"),

    ESIC_MAX       UMETA(DisplayName = "DefaultMAX")
};


// ============================================================
// EBuffType：Buff 类型
// ============================================================
// 用途：DataTable 中 BuffType 列的值，同时供 BuffComponent 识别 Buff 种类
// 设计：与 UBuffComponent 的三个 Buff 方法一一对应
//   EBT_Speed  → BuffSpeed()
//   EBT_Jump   → BuffJump()
//   EBT_Shield → ReplenishShield()
UENUM(BlueprintType)
enum class EBuffType : uint8
{
    EBT_None   UMETA(DisplayName = "None"),
    EBT_Speed  UMETA(DisplayName = "Speed"),
    EBT_Jump   UMETA(DisplayName = "Jump"),
    EBT_Shield UMETA(DisplayName = "Shield"),
    EBT_Heal   UMETA(DisplayName = "Heal"),

    EBT_MAX    UMETA(DisplayName = "DefaultMAX")
};


// ============================================================
// FShopItemRow：DataTable 行结构
// ============================================================
// 每行表示一个可购买商品，包含通用字段和按类别专用的字段
//
// 字段使用规则（按 Category）：
//   Weapon:    使用 WeaponClass
//   Ammo:      使用 AmmoWeaponType + AmmoAmount
//   Throwable: 使用 ThrowableType
//   Buff:      使用 BuffType
// 不参与当前 Category 的字段留空/默认值即可
USTRUCT(BlueprintType)
struct FShopItemRow : public FTableRowBase
{
    GENERATED_BODY()

    // ---- 通用字段（所有类别必填）----

    // 唯一编号，客户端 RPC 只传此值，服务器查表获取其余字段
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    int32 ItemID = 0;

    // UI 显示名称（如 "Assault Rifle"）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    FText DisplayName;

    // 物品类别，驱动 ProcessPurchase 的 switch 分发
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    EShopItemCategory Category = EShopItemCategory::ESIC_None;

    // 价格（服务端权威，测试阶段全部填 1）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    int32 Price = 1;

    // ---- 武器专用 ----

    // 武器蓝图类（如 BP_AssaultRifle），SpawnActor 时使用
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    TSubclassOf<AWeapon> WeaponClass;

    // ---- 弹药专用 ----

    // 此弹药适配的武器类型，购买时需与手持武器类型匹配
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    EWeaponType AmmoWeaponType;

    // 每次购买增加的备弹数量（当前全局 = 2，可逐项差异化）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    int32 AmmoAmount = 2;

    // ---- 投掷物专用 ----

    // 投掷物类型，对应 ThrowableComponent 中的三个计数器
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    EThrowableType ThrowableType;

    // ---- Buff 专用 ----

    // Buff 类型，对应 BuffComponent 的三个 Buff 方法
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    EBuffType BuffType;
};
