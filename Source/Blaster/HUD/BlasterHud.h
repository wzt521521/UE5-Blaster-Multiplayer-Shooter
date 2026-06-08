// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BlasterHud.generated.h"
class UTexture2D;

// 准星绘制所需的数据包，由武器传递给HUD
USTRUCT(BlueprintType)
struct FHUDPackage
{
	GENERATED_BODY()
public:
	// 准星四方向纹理，分别绘制后拼成完整十字准心，根据武器散布动态偏移
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* CrosshairsCenter;
	UTexture2D* CrosshairsLeft;
	UTexture2D* CrosshairsRight;
	UTexture2D* CrosshairsBottom;
	UTexture2D* CrosshairsTop;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrosshairSpreadMax = 16.f; // 准星最大散布距离（像素）
	// 动态散布因子（0~1），由 CombatComponent 根据移动速度计算，0=静止，1=全速奔跑
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrosshairsSpread = 0.f;
	// 准星颜色，默认为白色
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor CrosshairsColor = FLinearColor::White;
};

/**
 * 自定义HUD，负责绘制武器准星
 */
UCLASS()
class BLASTER_API ABlasterHud : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
private:
	// 当前绘制使用的准星数据，由武器通过 SetHUDPackage 更新
	FHUDPackage HUDPackage;

	void DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter,FVector2D Spread);
public:
	FORCEINLINE void SetHUDPackage(const FHUDPackage& Package) { HUDPackage = Package; }
	FORCEINLINE FHUDPackage GetHUDPackage() const { return HUDPackage; }
};
