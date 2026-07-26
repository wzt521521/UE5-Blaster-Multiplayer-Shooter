#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoundOverlay.generated.h"

class UTextBlock;

// 回合信息面板：回合数、存活人数、大比分、阵营标识
// 由 BlasterPlayerController 的 SetHUD* 函数推送数据
UCLASS()
class BLASTER_API URoundOverlay : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* RoundNumberText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* AttackerAliveText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DefenderAliveText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ScoreText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TeamLabel;
};
