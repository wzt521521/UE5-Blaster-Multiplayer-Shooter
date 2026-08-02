#include "Blaster/BombMode/BombStatusWidget.h"

void UBombStatusWidget::UpdateTimer(float RemainingTime, float TotalTime)
{
	if (TimerText)
	{
		TimerText->SetText(FText::FromString(
			FString::Printf(TEXT("%.1f"), FMath::Max(0.f, RemainingTime))));
	}
}

void UBombStatusWidget::UpdateSiteName(const FString& SiteName)
{
	if (SiteNameText)
	{
		SiteNameText->SetText(FText::FromString(SiteName));
	}
}

void UBombStatusWidget::UpdateStatusText(const FString& Text)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Text));
	}
}

void UBombStatusWidget::SetBombUIVisible(bool bVisible)
{
	// 延迟加入 Viewport：首次显示时 Widget 可能尚未在 Viewport 中，
	// 由 UpdateBombStatusUI → PC Tick 路径调用，而非 ShowBombStatus API
	if (bVisible && !IsInViewport())
	{
		AddToViewport();
	}
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}
