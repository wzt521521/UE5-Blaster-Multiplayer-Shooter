#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuyMenu.generated.h"

class UTextBlock;

// 购买菜单 Widget（CSGO 风格武器商店）
// 蓝图定义布局 + 武器列表，C++ 提供金钱读取、购买校验、自动刷新
// 生命周期由 ABlasterPlayerController::ToggleBuyMenu 管理
UCLASS()
class BLASTER_API UBuyMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 从本地 PlayerState 读取当前持有金额（供蓝图调用，读的是客户端复制值）
	UFUNCTION(BlueprintCallable, Category = "BuyMenu")
	int32 GetPlayerMoney() const;

	// 请求购买物品（客户端调用 → 服务端 RPC 校验执行）
	UFUNCTION(BlueprintCallable, Category = "BuyMenu")
	void RequestPurchase(int32 ItemID, int32 ItemCost);

	// Money 变化 → 直接更新屏幕上的 MoneyText（C++ 侧闭环，蓝图无需操作）
	UFUNCTION()
	void OnMoneyChangedHandler(int32 NewMoney, int32 Delta);

private:
	// 蓝图同名 TextBlock 控件（BindWidget 自动绑定）
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MoneyText;
};
