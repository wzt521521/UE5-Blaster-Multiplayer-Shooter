// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerController.h"
#include "Blaster/HUD/BlasterHud.h"
#include "Blaster/HUD/Characteroverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void ABlasterPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 缓存 HUD 引用，避免每次更新血量时都做 Cast
    // PlayerController 和 HUD 是一一对应的——每个玩家都有自己的 PlayerController 和自己的 HUD
    BlasterHud = Cast<ABlasterHud>(GetHUD());
}

void ABlasterPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ABlasterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void ABlasterPlayerController::SetHUDHealth(float Health, float MaxHealth)
{
    // ————————————————————————————————————————————
    // 血量 UI 更新：BlasterCharacter 把 Health 复制过来，PlayerController 负责"翻译"成 UI 能理解的数据
    // PlayerController 是整个血量链路的"中转站"：
    //   BlasterCharacter → BlasterPlayerController → BlasterHud → CharacterOverlay → HealthBar/HealthText
    //
    // 角色是数据源（Health 变量），但他不该知道"血条怎么画"——他只管把数值丢给 Controller
    // Controller 负责桥接：找到 HUD → 找到 CharacterOverlay → 找到血条控件 → 填值
    // ————————————————————————————————————————————

    // 懒加载容错：如果 HUD 还没初始化（BeginPlay 还没跑完），这里再做一次 Cast
    BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;

    // 四层空指针检查：HUD 存在 → 角色覆盖层已创建 → 血条进度条存在 → 血量文本存在
    // 缺任何一环都跳过——比如 AddCharacterOverlay() 还没执行完的时候
    bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay
        && BlasterHud->CharacterOverlay->HealthBar && BlasterHud->CharacterOverlay->HealthText;

    if (bHUDValid)
    {
        // ① 血条进度条：0.0 = 空血，1.0 = 满血
        const float HealthPercent = Health / MaxHealth;
        BlasterHud->CharacterOverlay->HealthBar->SetPercent(HealthPercent);

        // ② 血量数字文本：格式化为 "90 / 100" 这种形式
        // FMath::CeilToInt 向上取整，保证哪怕只剩 0.1 也至少显示 "1"
        FString HealthTextStr = FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
        BlasterHud->CharacterOverlay->HealthText->SetText(FText::FromString(HealthTextStr));
    }
}
