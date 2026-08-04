// Fill out your copyright notice in the Description page of Project Settings.

#include "GameStartupMenuActor.h"
#include "MultiplayerSessions/Public/Menu.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AGameStartupMenuActor::AGameStartupMenuActor()
{
    // 不需要 Tick
    PrimaryActorTick.bCanEverTick = false;

    // 尝试通过 Constructor 加载 WBP_Menu（延迟加载，即使插件 Content 未挂载也会稍后解析）
    static ConstructorHelpers::FClassFinder<UUserWidget> MenuBPClass(
        TEXT("/MultiplayerSessions/WBP_Menu"));
    if (MenuBPClass.Succeeded())
    {
        MenuWidgetClass = MenuBPClass.Class;
        UE_LOG(LogTemp, Log,
            TEXT("[StartupMenu] Constructor | WBP_Menu found via FClassFinder"));
    }
}

void AGameStartupMenuActor::BeginPlay()
{
    Super::BeginPlay();

    // ── 只在有本地玩家时创建 UI（无头 DS 跳过）──
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[StartupMenu] No PlayerController (NetMode=%d) — skipping"),
            (int32)GetNetMode());
        return;
    }

    // ── Constructor 失败时回退到同步 LoadClass ──
    if (!MenuWidgetClass)
    {
        MenuWidgetClass = LoadClass<UUserWidget>(
            nullptr, TEXT("/MultiplayerSessions/WBP_Menu.WBP_Menu_C"));
        if (MenuWidgetClass)
        {
            UE_LOG(LogTemp, Log, TEXT("[StartupMenu] WBP_Menu loaded via LoadClass fallback"));
        }
    }

    if (!MenuWidgetClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[StartupMenu] Cannot find WBP_Menu! Verify the path: /MultiplayerSessions/WBP_Menu"));
        return;
    }

    // ── 等价蓝图: Create WBP Menu Widget → Menu Setup ──
    UUserWidget* Widget = CreateWidget<UUserWidget>(PC, MenuWidgetClass);
    UMenu* MenuWidget = Cast<UMenu>(Widget);
    if (!MenuWidget)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[StartupMenu] Widget is not UMenu! Actual=%s"),
            Widget ? *Widget->GetClass()->GetName() : TEXT("null"));
        if (Widget) Widget->RemoveFromParent();
        return;
    }

    MenuWidget->MenuSetup(NumPublicConnections, MatchType, LobbyPath, bIsDedicatedServer);

    UE_LOG(LogTemp, Log,
        TEXT("[StartupMenu] Created | NetMode=%d | Connections=%d | MatchType=%s | Lobby=%s | bDS=%d"),
        (int32)GetNetMode(), NumPublicConnections, *MatchType, *LobbyPath, bIsDedicatedServer);
}
