// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BlasterHud.generated.h"
class UTexture2D;
class UCharacteroverlay;
class UUserWidget;

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

	UPROPERTY(EditAnywhere, Category="Player HUD")
	TSubclassOf<class UUserWidget> CharacterOverlayClass;

	class UCharacteroverlay* CharacterOverlay;
private:
	void AddCharacterOverlay();

	FHUDPackage HUDPackage;

	void DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor CrosshairColor);
public:
	FORCEINLINE void SetHUDPackage(const FHUDPackage& Package) { HUDPackage = Package; }
	FORCEINLINE FHUDPackage GetHUDPackage() const { return HUDPackage; }
};
