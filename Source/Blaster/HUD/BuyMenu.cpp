#include "BuyMenu.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Components/TextBlock.h"

void UBuyMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// 绑定 Money 变化委托：回合结束发钱 / 购买扣款后自动刷新金额显示
	if (ABlasterPlayerState* PS = GetOwningPlayerState<ABlasterPlayerState>())
	{
		PS->OnMoneyChanged.AddDynamic(this, &UBuyMenu::OnMoneyChangedHandler);
	}
}

void UBuyMenu::NativeDestruct()
{
	// 解绑委托：防止 Widget 销毁后 PlayerState 持有野指针
	if (ABlasterPlayerState* PS = GetOwningPlayerState<ABlasterPlayerState>())
	{
		PS->OnMoneyChanged.RemoveDynamic(this, &UBuyMenu::OnMoneyChangedHandler);
	}

	Super::NativeDestruct();
}

int32 UBuyMenu::GetPlayerMoney() const
{
	// 客户端直接从 PlayerState 读取复制值，不发起 RPC
	if (ABlasterPlayerState* PS = GetOwningPlayerState<ABlasterPlayerState>())
	{
		return PS->Money;
	}
	return 0;
}

void UBuyMenu::RequestPurchase(int32 ItemID, int32 ItemCost)
{
	// 客户端→服务端 RPC：服务端校验 + 扣款 + 发放
	if (ABlasterPlayerController* PC = GetOwningPlayer<ABlasterPlayerController>())
	{
		PC->ServerRequestPurchase(ItemID, ItemCost);
	}
}

void UBuyMenu::OnMoneyChangedHandler(int32 NewMoney, int32 Delta)
{
	// Money 变化 → 直接更新屏幕上的 MoneyText（C++ 侧闭环）
	if (MoneyText) MoneyText->SetText(FText::AsNumber(NewMoney));
}
