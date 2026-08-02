// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BlasterHud.generated.h"
class UTexture2D;
class UCharacteroverlay;
class URoundOverlay;
class UAnnouncement;
class UBuyMenu;
class UThrowableSelectionWheel;
class UBombStatusWidget;
class UBombInteractWidget;
class UImage;
class USoundCue;

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

	// ========================================================================
	// 集中预创建：BeginPlay 时一次性创建所有 Widget（默认隐藏），后续只需 Show/Hide
	// ========================================================================
	void InitializeHUD();

	// ========================================================================
	// Widget 蓝图类（蓝图中配置）
	// ========================================================================
	UPROPERTY(EditAnywhere, Category="Player HUD")
	TSubclassOf<class UUserWidget> CharacterOverlayClass;
	UPROPERTY()
	class UCharacteroverlay* CharacterOverlay;

	UPROPERTY(EditAnywhere, Category="Announcements")
	TSubclassOf<UUserWidget> AnnouncementClass;
	UPROPERTY()
	class UAnnouncement* Announcement;

	UPROPERTY(EditAnywhere, Category = "Player HUD")
	TSubclassOf<UUserWidget> BuyMenuClass;
	UPROPERTY()
	class UBuyMenu* BuyMenu;

	UPROPERTY(EditAnywhere, Category = "Player HUD")
	TSubclassOf<UUserWidget> ThrowableWheelClass;
	UPROPERTY()
	UThrowableSelectionWheel* ThrowableWheel;

	UPROPERTY(EditAnywhere, Category = "Player HUD")
	TSubclassOf<UUserWidget> RoundOverlayClass;
	UPROPERTY()
	URoundOverlay* RoundOverlay;

	// ── 闪光弹致盲 ──
	// 独立于 CharacterOverlay，始终在视口最顶层，不受回合切换影响
	UPROPERTY(EditAnywhere, Category = "Player HUD")
	TSubclassOf<UUserWidget> FlashOverlayClass;
	UPROPERTY()
	UImage* FlashOverlay;

	UPROPERTY(EditAnywhere, Category = "Throwable|Flashbang")
	USoundCue* FlashbangTinnitusSound;

	void ShowFlashEffect(float Duration);

	// ── 炸弹 UI（BombMode Phase 4）──
	UPROPERTY(EditAnywhere, Category = "Bomb UI")
	TSubclassOf<UUserWidget> BombStatusWidgetClass;
	UPROPERTY()
	UBombStatusWidget* BombStatusWidget;

	UPROPERTY(EditAnywhere, Category = "Bomb UI")
	TSubclassOf<UUserWidget> BombInteractWidgetClass;
	UPROPERTY()
	UBombInteractWidget* BombInteractWidget;

	void CreateBombWidgets();       // 集中预创建
	void ShowBombStatus(bool bShow);
	void ShowBombInteract(bool bShow);
	UBombStatusWidget* GetBombStatusWidget() const { return BombStatusWidget; }
	UBombInteractWidget* GetBombInteractWidget() const { return BombInteractWidget; }

	// ========================================================================
	// Show/Hide API —— Widget 已在 InitializeHUD 中创建，这里只控制可见性
	// ========================================================================
	void ShowCharacterOverlay();
	void HideCharacterOverlay();

	// 公告面板（返回已存在的实例，首次调用时创建以兼容旧代码）
	void EnsureAnnouncement();

	// 购买菜单
	void CreateBuyMenu();     // 内部调用，集中创建用
	void ShowBuyMenu();
	void HideBuyMenu();

	// 投掷物选择
	void CreateThrowableWheel();
	void ShowThrowableWheel();
	void HideThrowableWheel();

	// 回合面板
	void CreateRoundOverlay();
	void ShowRoundOverlay();
	void HideRoundOverlay();

private:

	FHUDPackage HUDPackage;

	// 闪光淡出
	FTimerHandle FlashFadeTimer;
	float FlashEffectStartTime = 0.f;
	float FlashEffectDuration = 0.f;
	void TickFlashFade();
	void HideFlashEffect();

	void DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor CrosshairColor);
public:
	FORCEINLINE void SetHUDPackage(const FHUDPackage& Package) { HUDPackage = Package; }
	FORCEINLINE FHUDPackage GetHUDPackage() const { return HUDPackage; }
};
