// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameStartupMenuActor.generated.h"

/**
 * 替代关卡蓝图的 C++ Actor — 放置在 GameStartupMap 关卡中
 *
 * BeginPlay 在每个进程（服务端/客户端）独立执行：
 *   - 有本地 PlayerController → 创建 Menu Widget，调用 MenuSetup
 *   - 无本地 PlayerController → 跳过（无头 DS）
 *
 * 放置方式：直接拖入 GameStartupMap 关卡即可，无需额外配置
 */
UCLASS()
class BLASTER_API AGameStartupMenuActor : public AActor
{
    GENERATED_BODY()

public:
    AGameStartupMenuActor();

protected:
    virtual void BeginPlay() override;

    // ── 以下参数可在 Details 面板或蓝图子类中覆盖 ──

    /** Menu 控件蓝图类（自动从 /MultiplayerSessions/WBP_Menu 加载） */
    UPROPERTY(EditDefaultsOnly, Category = "Startup")
    TSubclassOf<class UUserWidget> MenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Startup")
    int32 NumPublicConnections = 4;

    UPROPERTY(EditDefaultsOnly, Category = "Startup")
    FString MatchType = TEXT("FreeForAll");

    UPROPERTY(EditDefaultsOnly, Category = "Startup")
    FString LobbyPath = TEXT("/Game/Maps/Lobby");

    /** true=DS 模式 | false=Listen Server 模式 */
    UPROPERTY(EditDefaultsOnly, Category = "Startup")
    bool bIsDedicatedServer = true;
};
