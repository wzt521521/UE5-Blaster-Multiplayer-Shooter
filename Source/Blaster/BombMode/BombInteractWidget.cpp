#include "Blaster/BombMode/BombInteractWidget.h"

void UBombInteractWidget::UpdateProgress(float Progress)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(FMath::Clamp(Progress, 0.f, 1.f));
	}
}

void UBombInteractWidget::UpdatePromptText(const FString& Text)
{
	if (PromptText)
	{
		PromptText->SetText(FText::FromString(Text));
	}
}

void UBombInteractWidget::SetInteractVisible(bool bVisible)
{
	// 延迟加入 Viewport：首次显示时 Widget 可能尚未在 Viewport 中，
	// 由 PushInteractUI → UpdateBombInteractUI 路径调用
	if (bVisible && !IsInViewport())
	{
		AddToViewport();
	}
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

void UBombInteractWidget::SetProgressBarVisible(bool bVisible)
{
	if (ProgressBar)
	{
		ProgressBar->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
