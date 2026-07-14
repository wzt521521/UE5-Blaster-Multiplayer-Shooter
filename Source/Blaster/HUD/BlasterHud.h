// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BlasterHud.generated.h"
class UTexture2D;
class UCharacteroverlay;
class UUserWidget;
class UBuyMenu;

// 准星绘制所需的数据包，由武器传递给HUD
USTRUCT(BlueprintType)
struct FHUDPackage
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTexture2D* CrosshairsCenter;
	UTexture2D* CrosshairsLeft;
	UTexture2D* CrosshairsRight;
	UTexture2D* CrosshairsBottom;
	UTexture2D* CrosshairsTop;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrosshairSpreadMax = 16.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CrosshairsSpread = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor CrosshairsColor = FLinearColor::White;
};

UCLASS()
class BLASTER_API ABlasterHud : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
	virtual void BeginPlay() override;

	// 角色HUD蓝图类（血条/弹药/准星等），蓝图中配置，运行时由AddCharacterOverlay()实例化
	UPROPERTY(EditAnywhere, Category="Player HUD")
	TSubclassOf<class UUserWidget> CharacterOverlayClass;

	// 角色HUD运行时实例指针，AddCharacterOverlay()创建后赋值，HandleCooldown()中RemoveFromParent移除
	UPROPERTY()
	class UCharacteroverlay* CharacterOverlay;

	// 公告面板蓝图类（等待玩家/比赛结束等提示），蓝图中配置，运行时由AddAnnouncement()实例化
	UPROPERTY(EditAnywhere, Category="Announcements")
	TSubclassOf<UUserWidget> AnnouncementClass;

	// 公告面板运行时实例指针，AddAnnouncement()创建后赋值，用于切换显示/隐藏和更新文本
	UPROPERTY()
	class UAnnouncement* Announcement;

	void AddAnnouncement();
	void AddCharacterOverlay();

	// 购买菜单蓝图类，蓝图中配置，运行时由 CreateBuyMenu() 实例化
	UPROPERTY(EditAnywhere, Category = "Player HUD")
	TSubclassOf<UUserWidget> BuyMenuClass;

	// 购买菜单运行时实例指针，ShowBuyMenu/HideBuyMenu 控制其 AddToViewport/RemoveFromParent
	UPROPERTY()
	class UBuyMenu* BuyMenu;

	void CreateBuyMenu();

private:

	FHUDPackage HUDPackage;

	void DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor CrosshairColor);
public:
	FORCEINLINE void SetHUDPackage(const FHUDPackage& Package) { HUDPackage = Package; }
	FORCEINLINE FHUDPackage GetHUDPackage() const { return HUDPackage; }
};
