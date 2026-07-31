#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "BuyMenu.generated.h"

// 购买菜单 Widget（CSGO 风格武器商店）
// BindWidget 模式：蓝图命名按钮 → C++ 自动绑定 → NativeConstruct 统一 AddDynamic
UCLASS()
class BLASTER_API UBuyMenu : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // 从本地 PlayerState 读取当前持有金额（供蓝图调用）
    UFUNCTION(BlueprintCallable, Category = "BuyMenu")
    int32 GetPlayerMoney() const;

    // 获取商店物品 DataTable（蓝图可调用）
    UFUNCTION(BlueprintCallable, Category = "BuyMenu")
    class UDataTable* GetShopItemTable() const;

    // 请求购买物品（客户端 → 服务端 RPC）
    UFUNCTION(BlueprintCallable, Category = "BuyMenu")
    void RequestPurchase(int32 ItemID);

    // Money 变化 → 更新 MoneyText（C++ 侧闭环）
    UFUNCTION()
    void OnMoneyChangedHandler(int32 NewMoney, int32 Delta);

private:
    // ── BindWidget 控件（名称必须与蓝图中的控件名一致）──

    UPROPERTY(meta = (BindWidget))
    UTextBlock* MoneyText;

    // ── 武器按钮（7 个）──
    UPROPERTY(meta = (BindWidget))
    UButton* Button_AR;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_SMG;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Rocket;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Pistol;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Sniper;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Shotgun;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_GrenadeLauncher;

    // ── 弹药按钮（7 个）──
    UPROPERTY(meta = (BindWidget))
    UButton* Button_AR_Ammo;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_SMG_Ammo;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Rocket_Ammo;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Pistol_Ammo;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Sniper_Ammo;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Shotgun_Ammo;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_GrenadeLauncher_Ammo;

    // ── 投掷物按钮（3 个）──
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Frag;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Flash;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Smoke;

    // ── Buff 按钮（3 个）──
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Speed;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Jump;
    UPROPERTY(meta = (BindWidget))
    UButton* Button_Shield;

    // ── 点击回调（一行搞定，每个硬编码 ItemID）──

    // 武器
    UFUNCTION() void OnARClicked()             { RequestPurchase(1001); }
    UFUNCTION() void OnSMGClicked()            { RequestPurchase(1002); }
    UFUNCTION() void OnRocketClicked()         { RequestPurchase(1003); }
    UFUNCTION() void OnPistolClicked()         { RequestPurchase(1004); }
    UFUNCTION() void OnSniperClicked()         { RequestPurchase(1005); }
    UFUNCTION() void OnShotgunClicked()        { RequestPurchase(1006); }
    UFUNCTION() void OnGrenadeLauncherClicked(){ RequestPurchase(1007); }

    // 弹药
    UFUNCTION() void OnARAmmoClicked()             { RequestPurchase(2001); }
    UFUNCTION() void OnSMGAmmoClicked()            { RequestPurchase(2002); }
    UFUNCTION() void OnRocketAmmoClicked()         { RequestPurchase(2003); }
    UFUNCTION() void OnPistolAmmoClicked()         { RequestPurchase(2004); }
    UFUNCTION() void OnSniperAmmoClicked()         { RequestPurchase(2005); }
    UFUNCTION() void OnShotgunAmmoClicked()        { RequestPurchase(2006); }
    UFUNCTION() void OnGrenadeLauncherAmmoClicked(){ RequestPurchase(2007); }

    // 投掷物
    UFUNCTION() void OnFragClicked()  { RequestPurchase(3001); }
    UFUNCTION() void OnFlashClicked() { RequestPurchase(3002); }
    UFUNCTION() void OnSmokeClicked() { RequestPurchase(3003); }

    // Buff
    UFUNCTION() void OnSpeedClicked()  { RequestPurchase(4001); }
    UFUNCTION() void OnJumpClicked()   { RequestPurchase(4002); }
    UFUNCTION() void OnShieldClicked() { RequestPurchase(4003); }
};
