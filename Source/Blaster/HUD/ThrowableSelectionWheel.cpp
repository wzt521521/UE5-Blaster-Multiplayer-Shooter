#include "ThrowableSelectionWheel.h"
#include "Blaster/BlasterComponents/ThrowableComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UThrowableSelectionWheel::NativeConstruct()
{
	Super::NativeConstruct();

	// 绑定三个按钮的点击事件
	if (FragButton)
		FragButton->OnClicked.AddDynamic(this, &UThrowableSelectionWheel::OnFragClicked);
	if (FlashButton)
		FlashButton->OnClicked.AddDynamic(this, &UThrowableSelectionWheel::OnFlashClicked);
	if (SmokeButton)
		SmokeButton->OnClicked.AddDynamic(this, &UThrowableSelectionWheel::OnSmokeClicked);
}

void UThrowableSelectionWheel::Show(UThrowableComponent* ThrowableComp)
{
	RefreshCounts(ThrowableComp);
	SetVisibility(ESlateVisibility::Visible);
}

void UThrowableSelectionWheel::Hide()
{
	SetVisibility(ESlateVisibility::Hidden);
}

void UThrowableSelectionWheel::RefreshCounts(UThrowableComponent* ThrowableComp)
{
	if (!ThrowableComp) return;

	Counts[0] = ThrowableComp->GetCount(EThrowableType::ETT_FragGrenade);
	Counts[1] = ThrowableComp->GetCount(EThrowableType::ETT_Flashbang);
	Counts[2] = ThrowableComp->GetCount(EThrowableType::ETT_SmokeGrenade);

	if (FragCountText)
		FragCountText->SetText(FText::AsNumber(Counts[0]));
	if (FlashCountText)
		FlashCountText->SetText(FText::AsNumber(Counts[1]));
	if (SmokeCountText)
		SmokeCountText->SetText(FText::AsNumber(Counts[2]));
}

// ------------------------------------------------------------
// 按钮点击回调：直接广播委托，由 PlayerController 处理选择和关闭
// ------------------------------------------------------------
void UThrowableSelectionWheel::OnFragClicked()
{
	OnTypeClicked.Broadcast(EThrowableType::ETT_FragGrenade);
}

void UThrowableSelectionWheel::OnFlashClicked()
{
	OnTypeClicked.Broadcast(EThrowableType::ETT_Flashbang);
}

void UThrowableSelectionWheel::OnSmokeClicked()
{
	OnTypeClicked.Broadcast(EThrowableType::ETT_SmokeGrenade);
}
