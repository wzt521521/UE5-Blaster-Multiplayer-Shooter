#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BuyMenu.generated.h"

// 购买菜单 Widget（CSGO 风格武器商店），蓝图定义布局，C++ 提供类锚点
// 生命周期由 ABlasterPlayerController::ToggleBuyMenu 管理
UCLASS()
class BLASTER_API UBuyMenu : public UUserWidget
{
	GENERATED_BODY()
};
