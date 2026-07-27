// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterHud.h"
#include "Characteroverlay.h"
#include "Announcement.h"
#include "RoundOverlay.h"
#include "BuyMenu.h"
#include "ThrowableSelectionWheel.h"
#include "GameFramework/PlayerController.h"

void ABlasterHud::DrawHUD()
{
	Super::DrawHUD();

	FVector2D ViewportSize;
	if(GEngine)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);

        float SpreadScaled = HUDPackage.CrosshairSpreadMax * HUDPackage.CrosshairsSpread;

		if(HUDPackage.CrosshairsCenter)
		{
            FVector2D Spread(0.f, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter,Spread, HUDPackage.CrosshairsColor);
		}
		if(HUDPackage.CrosshairsLeft)
		{
			FVector2D Spread(-SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if(HUDPackage.CrosshairsRight)
		{
			FVector2D Spread(SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if(HUDPackage.CrosshairsBottom)
		{
			FVector2D Spread(0.f, SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsBottom, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if(HUDPackage.CrosshairsTop)
		{
			FVector2D Spread(0.f, -SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
	}
}

void ABlasterHud::DrawCrosshair(UTexture2D *Texture, FVector2D ViewportCenter,FVector2D Spread, FLinearColor CrosshairColor)
{
	const float TextureWidth = Texture->GetSizeX();
	const float TextureHeight = Texture->GetSizeY();

	const FVector2D TextureDrawPoint(
		ViewportCenter.X - (TextureWidth / 2.f)+Spread.X,
		ViewportCenter.Y - (TextureHeight / 2.f)+Spread.Y
	);

	DrawTexture(
		Texture,
		TextureDrawPoint.X,
		TextureDrawPoint.Y,
		TextureWidth,
		TextureHeight,
		0.f, 0.f, 1.f, 1.f,
		CrosshairColor
	);
}

// ========================================================================
// HUD 初始化：一次性创建所有 Widget，默认不加入 Viewport（隐藏）
// 后续通过 Show/Hide 函数控制可见性
// ========================================================================
void ABlasterHud::BeginPlay()
{
	Super::BeginPlay();
	InitializeHUD();
}

void ABlasterHud::InitializeHUD()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController) return;

	// 战斗 HUD（血条/弹药/倒计时）
	if (CharacterOverlayClass && !CharacterOverlay)
	{
		CharacterOverlay = CreateWidget<UCharacteroverlay>(PlayerController, CharacterOverlayClass);
	}

	// 公告面板（回合提示/倒计时/结果）—— 预加入 Viewport，默认隐藏
	if (AnnouncementClass && !Announcement)
	{
		Announcement = CreateWidget<UAnnouncement>(PlayerController, AnnouncementClass);
		Announcement->AddToViewport();
		Announcement->SetVisibility(ESlateVisibility::Hidden);
	}

	// 购买菜单
	if (BuyMenuClass && !BuyMenu)
	{
		BuyMenu = CreateWidget<UBuyMenu>(PlayerController, BuyMenuClass);
	}

	// 投掷物选择面板
	if (ThrowableWheelClass && !ThrowableWheel)
	{
		ThrowableWheel = CreateWidget<UThrowableSelectionWheel>(PlayerController, ThrowableWheelClass);
	}

	// 回合信息面板（回合数/存活人数/大比分）
	if (RoundOverlayClass && !RoundOverlay)
	{
		RoundOverlay = CreateWidget<URoundOverlay>(PlayerController, RoundOverlayClass);
	}
}

// ========================================================================
// CharacterOverlay
// ========================================================================
void ABlasterHud::ShowCharacterOverlay()
{
	if (CharacterOverlay && !CharacterOverlay->IsInViewport())
	{
		CharacterOverlay->AddToViewport();
	}
	CharacterOverlay->SetVisibility(ESlateVisibility::Visible);
}

void ABlasterHud::HideCharacterOverlay()
{
	if (CharacterOverlay)
	{
		CharacterOverlay->SetVisibility(ESlateVisibility::Hidden);
	}
}

// ========================================================================
// Announcement——集中创建 + 首次使用时兜底
// ========================================================================
void ABlasterHud::EnsureAnnouncement()
{
	// 兜底：如果 InitializeHUD 中创建失败（如客户端异步加载），此处补创建
	if (!Announcement && AnnouncementClass)
	{
		APlayerController* PC = GetOwningPlayerController();
		if (PC) Announcement = CreateWidget<UAnnouncement>(PC, AnnouncementClass);
	}
	if (Announcement && !Announcement->IsInViewport())
	{
		Announcement->AddToViewport();
	}
}

// ========================================================================
// BuyMenu
// ========================================================================
void ABlasterHud::CreateBuyMenu()
{
	// 集中创建已在 InitializeHUD 中完成，保留空实现兼容旧调用
}

void ABlasterHud::ShowBuyMenu()
{
	if (BuyMenu && !BuyMenu->IsInViewport())
	{
		BuyMenu->AddToViewport();
	}
}

void ABlasterHud::HideBuyMenu()
{
	if (BuyMenu && BuyMenu->IsInViewport())
	{
		BuyMenu->RemoveFromParent();
	}
}

// ========================================================================
// ThrowableWheel
// ========================================================================
void ABlasterHud::CreateThrowableWheel()
{
	// 集中创建已在 InitializeHUD 中完成
}

void ABlasterHud::ShowThrowableWheel()
{
	if (ThrowableWheel && !ThrowableWheel->IsInViewport())
	{
		ThrowableWheel->AddToViewport();
	}
}

void ABlasterHud::HideThrowableWheel()
{
	if (ThrowableWheel && ThrowableWheel->IsInViewport())
	{
		ThrowableWheel->RemoveFromParent();
	}
}

// ========================================================================
// RoundOverlay
// ========================================================================
void ABlasterHud::CreateRoundOverlay()
{
	// 集中创建已在 InitializeHUD 中完成
}

void ABlasterHud::ShowRoundOverlay()
{
	if (RoundOverlay && !RoundOverlay->IsInViewport())
	{
		RoundOverlay->AddToViewport();
	}
	RoundOverlay->SetVisibility(ESlateVisibility::Visible);
}

void ABlasterHud::HideRoundOverlay()
{
	if (RoundOverlay)
	{
		RoundOverlay->SetVisibility(ESlateVisibility::Hidden);
	}
}
