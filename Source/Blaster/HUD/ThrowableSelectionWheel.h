#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blaster/BlasterTypes/ThrowableTypes.h"
#include "ThrowableSelectionWheel.generated.h"

class UButton;
class UTextBlock;
class UThrowableComponent;

// 点击选中委托：按钮被点击时广播，PlayerController 绑定此事件处理选择和关闭面板
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThrowableTypeClicked, EThrowableType, SelectedType);

// 投掷物选择面板 — 点击图标直接确认选择
UCLASS()
class BLASTER_API UThrowableSelectionWheel : public UUserWidget
{
	GENERATED_BODY()

public:
	// 点击选中事件：按钮点击时广播，外部绑定后处理选择 + 关闭面板
	UPROPERTY(BlueprintAssignable)
	FOnThrowableTypeClicked OnTypeClicked;

	// 打开面板：刷新各类型数量并显示
	void Show(UThrowableComponent* ThrowableComp);

	// 关闭面板
	void Hide();

	// 刷新三种手雷的持有数量显示
	void RefreshCounts(UThrowableComponent* ThrowableComp);

protected:
	virtual void NativeConstruct() override;

	// ===== 蓝图绑定控件（按钮 + 数量文字）=====
	UPROPERTY(meta = (BindWidget))
	UButton* FragButton;

	UPROPERTY(meta = (BindWidget))
	UButton* FlashButton;

	UPROPERTY(meta = (BindWidget))
	UButton* SmokeButton;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* FragCountText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* FlashCountText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* SmokeCountText;

private:
	// 按钮点击回调 — 广播委托让 PlayerController 处理选择和关闭
	UFUNCTION()
	void OnFragClicked();

	UFUNCTION()
	void OnFlashClicked();

	UFUNCTION()
	void OnSmokeClicked();

	// 各类型剩余数量缓存
	int32 Counts[3] = {0, 0, 0};
};
