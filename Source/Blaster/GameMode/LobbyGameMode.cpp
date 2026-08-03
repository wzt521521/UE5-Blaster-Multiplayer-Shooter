// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"       // GetPlayerName
#include "Blaster/BlasterTypes/MatchState.h" // LeavingMap 常量

DEFINE_LOG_CATEGORY(LogLobby);

// ===== LIFECYCLE =====

ALobbyGameMode::ALobbyGameMode()
{
    UE_LOG(LogLobby, Log, TEXT("[LobbyGameMode] Constructor — CDO created, bUseSeamlessTravel=%d, AimPeople=%d, GameMapPath=%s"),
        bUseSeamlessTravel, AimPeople, *GameMapPath);
}

void ALobbyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    // 打印 InitGame 上下文：谁在什么网络中用什么地图启动了
    const EWorldType::Type WT = GetWorld() ? GetWorld()->WorldType : EWorldType::None;
    const ENetMode NM = GetNetMode();
    UE_LOG(LogLobby, Warning,
        TEXT("[LobbyGameMode] InitGame | Map=%s | WorldType=%d | NetMode=%d | Options=%s | Error=%s"),
        *MapName, (int32)WT, (int32)NM, *Options, *ErrorMessage);

    // PIE 单进程下引擎禁止无缝切换（见 AGameModeBase::CanServerTravel），
    // 仅在非 PIE（打包/独立服务器）下启用
    if (GetWorld() && GetWorld()->WorldType != EWorldType::PIE)
    {
        bUseSeamlessTravel = true;
    }
    else
    {
        // 显式关闭：蓝图子类可能勾选了 bUseSeamlessTravel，PIE 下必须强制关掉
        bUseSeamlessTravel = false;
    }

    UE_LOG(LogLobby, Warning,
        TEXT("[LobbyGameMode] InitGame → bUseSeamlessTravel = %d (WorldType=%d)"),
        bUseSeamlessTravel, (int32)WT);
}

// ===== PLAYER JOIN / TRAVEL =====

void ALobbyGameMode::PostLogin(APlayerController *NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // 打印当前玩家：谁加入了
    const FString PlayerName = NewPlayer->PlayerState
        ? NewPlayer->PlayerState->GetPlayerName() : TEXT("Unknown");
    UE_LOG(LogLobby, Warning,
        TEXT("[LobbyGameMode] PostLogin | Player=%s | MatchState=%s"),
        *PlayerName, *MatchState.ToString());

    if (!GameState)
    {
        UE_LOG(LogLobby, Error, TEXT("[LobbyGameMode] PostLogin → ABORT: GameState is null!"));
        return;
    }

    // 防重入：ServerTravel 是帧末异步执行，若不切状态，后续 PostLogin/Tick 会重复调用
    if (MatchState == MatchState::LeavingMap)
    {
        UE_LOG(LogLobby, Warning,
            TEXT("[LobbyGameMode] PostLogin → IGNORE: Already in LeavingMap, travel in progress"));
        return;
    }

    // 当前大厅人数
    const int32 NumOfPlayers = GameState->PlayerArray.Num();

    // 大厅游戏状态
    const bool bEnoughPlayers = NumOfPlayers >= AimPeople;
    const FString LobbyState = bEnoughPlayers
        ? TEXT("ReadyToTravel")
        : TEXT("WaitingForPlayers");

    // 控制台打印：大厅人数 + 大厅状态（用 LogLobby 标签筛选：LogLobby）
    UE_LOG(LogLobby, Warning,
        TEXT("[LobbyGameMode] PostLogin | People=%d / Aim=%d | LobbyState=%s"),
        NumOfPlayers, AimPeople, *LobbyState);

    if (bEnoughPlayers)
    {
        UWorld* World = GetWorld();
        if (!World)
        {
            UE_LOG(LogLobby, Error, TEXT("[LobbyGameMode] PostLogin → ABORT: GetWorld() is null!"));
            return;
        }

        // 打印 ServerTravel 前的完整快照
        UE_LOG(LogLobby, Warning,
            TEXT("[LobbyGameMode] Pre-ServerTravel SNAPSHOT | WorldType=%d | NetMode=%d | bUseSeamlessTravel=%d | MapPath=%s | MatchState=%s"),
            (int32)World->WorldType, (int32)GetNetMode(),
            bUseSeamlessTravel, *GameMapPath, *MatchState.ToString());

        // 先切到 LeavingMap 防重入（与 BombDefusalGameMode::ReturnToLobby 一致）
        SetMatchState(MatchState::LeavingMap);
        UE_LOG(LogLobby, Warning,
            TEXT("[LobbyGameMode] PostLogin → SetMatchState(LeavingMap), calling ServerTravel..."));

        // ServerTravel 返回 false 表示失败（地图不存在/路径错误等）
        const bool bTravelSuccess = World->ServerTravel(GameMapPath);
        if (bTravelSuccess)
        {
            UE_LOG(LogLobby, Warning,
                TEXT("[LobbyGameMode] PostLogin → ServerTravel SUCCESS, travelling..."));
        }
        else
        {
            // 严重错误：打印全部诊断信息
            UE_LOG(LogLobby, Error,
                TEXT("[LobbyGameMode] PostLogin → ServerTravel FAILED!"));
            UE_LOG(LogLobby, Error,
                TEXT("  ↳ MapPath: %s"), *GameMapPath);
            UE_LOG(LogLobby, Error,
                TEXT("  ↳ WorldType: %d | NetMode: %d | bUseSeamlessTravel: %d"),
                (int32)World->WorldType, (int32)GetNetMode(), bUseSeamlessTravel);
            UE_LOG(LogLobby, Error,
                TEXT("  ↳ MatchState: %s | PlayerArray.Num: %d"),
                *MatchState.ToString(), GameState->PlayerArray.Num());
        }
    }
}
