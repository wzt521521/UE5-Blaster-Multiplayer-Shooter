// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterHud.h"

void ABlasterHud::DrawHUD()
{
	Super::DrawHUD();

	// 获取屏幕尺寸和中心点
	FVector2D ViewportSize;
	if(GEngine)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);

		// 按顺序绘制五块准星纹理，每块都以屏幕中心为基准定位
		// 当前全部叠在中心，后续会根据武器散布（spread）动态偏移各块的位置

        float SpreadScaled = HUDPackage.CrosshairSpreadMax * HUDPackage.CrosshairsSpread;
        
		if(HUDPackage.CrosshairsCenter)
		{
            FVector2D Spread(0.f, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter,Spread);
		}
		if(HUDPackage.CrosshairsLeft)
		{
			FVector2D Spread(-SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, Spread);
		}
		if(HUDPackage.CrosshairsRight)
		{
			FVector2D Spread(SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, Spread);
		}
		if(HUDPackage.CrosshairsBottom)
		{
			FVector2D Spread(0.f, SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsBottom, ViewportCenter, Spread);
		}
		if(HUDPackage.CrosshairsTop)
		{
			FVector2D Spread(0.f, -SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, Spread);
		}
	}
}

// 将指定纹理以 ViewportCenter 为中心绘制到屏幕上
void ABlasterHud::DrawCrosshair(UTexture2D *Texture, FVector2D ViewportCenter,FVector2D Spread)
{
	// 获取纹理实际像素尺寸
	const float TextureWidth = Texture->GetSizeX();
	const float TextureHeight = Texture->GetSizeY();

	// 将纹理中心对齐到目标点：绘制起点 = 目标点 - 纹理尺寸的一半
	const FVector2D TextureDrawPoint(
		ViewportCenter.X - (TextureWidth / 2.f)+Spread.X,
		ViewportCenter.Y - (TextureHeight / 2.f)+Spread.Y
	);

	// 使用完整 UV（0,0 到 1,1）绘制整张纹理，不裁剪
	DrawTexture(
		Texture,
		TextureDrawPoint.X,
		TextureDrawPoint.Y,
		TextureWidth,
		TextureHeight,
		0.f,	// 起始 U
		0.f,	// 起始 V
		1.f,	// 纹理宽度占比
		1.f,	// 纹理高度占比
		FLinearColor::White
	);
}
