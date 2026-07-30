#include "Characteroverlay.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Components/TextBlock.h"

void UCharacteroverlay::NativeConstruct()
{
	Super::NativeConstruct();

	// 绑定 Money 变化委托：回合结束发钱 / 购买扣款后自动刷新 HUD 金额显示
	if (ABlasterPlayerState* PS = GetOwningPlayerState<ABlasterPlayerState>())
	{
		PS->OnMoneyChanged.AddDynamic(this, &UCharacteroverlay::OnMoneyChangedHandler);
	}
}

void UCharacteroverlay::NativeDestruct()
{
	if (ABlasterPlayerState* PS = GetOwningPlayerState<ABlasterPlayerState>())
	{
		PS->OnMoneyChanged.RemoveDynamic(this, &UCharacteroverlay::OnMoneyChangedHandler);
	}

	Super::NativeDestruct();
}

void UCharacteroverlay::OnMoneyChangedHandler(int32 NewMoney, int32 Delta)
{
	if (MoneyText) MoneyText->SetText(FText::AsNumber(NewMoney));
}
