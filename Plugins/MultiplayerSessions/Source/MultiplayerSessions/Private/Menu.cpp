// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "Engine/Engine.h"          // GEngine
#include "GameFramework/PlayerController.h"

// ===== MenuSetup — 由蓝图在 GameStartupMap 关卡蓝图调用，初始化菜单 =====
void UMenu::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch, FString LobbyPath)
{
    PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
    NumPublicConnections = NumberOfPublicConnections;
    MatchType = TypeOfMatch;

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] MenuSetup | NumConnections=%d | MatchType=%s | LobbyPath=%s → PathToLobby=%s"),
        NumberOfPublicConnections, *TypeOfMatch, *LobbyPath, *PathToLobby);

    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);
    bIsFocusable = true;

    // 获取玩家控制器并设为 UI Only 输入模式
    UWorld* World = GetWorld();
    if (World)
    {
        APlayerController* PlayerController = World->GetFirstPlayerController();
        if (PlayerController)
        {
            FInputModeUIOnly InputModeData;
            InputModeData.SetWidgetToFocus(TakeWidget());
            InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PlayerController->SetInputMode(InputModeData);
            PlayerController->SetShowMouseCursor(true);
            UE_LOG(LogMultiplayerSessions, Log,
                TEXT("[Menu] MenuSetup | InputMode → UIOnly | MouseCursor=true"));
        }
        else
        {
            UE_LOG(LogMultiplayerSessions, Error,
                TEXT("[Menu] MenuSetup | PlayerController is null!"));
        }
    }

    // 从 GameInstance 获取 MultiplayerSessionsSubsystem
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
        if (MultiplayerSessionsSubsystem)
        {
            UE_LOG(LogMultiplayerSessions, Log,
                TEXT("[Menu] MenuSetup | Subsystem obtained | Binding delegates..."));
        }
        else
        {
            UE_LOG(LogMultiplayerSessions, Error,
                TEXT("[Menu] MenuSetup | Subsystem is null! Check GameInstance config."));
        }
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] MenuSetup | GameInstance is null!"));
    }

    // 绑定委托：Subsystem 异步完成后回调对应函数
    if (MultiplayerSessionsSubsystem)
    {
        MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &UMenu::OnCreateSession);
        MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &UMenu::OnFindSessions);
        MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &UMenu::OnJoinSession);
        MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &UMenu::OnDestroySession);
        MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &UMenu::OnStartSession);

        UE_LOG(LogMultiplayerSessions, Log,
            TEXT("[Menu] MenuSetup | 5 delegates bound: Create/Find/Join/Destroy/Start"));
    }
}

// ===== Initialize — Widget 构造后自动调用，绑定按钮点击事件 =====
bool UMenu::Initialize()
{
    if (!Super::Initialize())
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] Initialize | Super::Initialize failed!"));
        return false;
    }

    // 绑定 Host 按钮
    if (HostButton)
    {
        HostButton->OnClicked.AddDynamic(this, &UMenu::HostButtonClicked);
        UE_LOG(LogMultiplayerSessions, Log,
            TEXT("[Menu] Initialize | HostButton bound"));
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] Initialize | HostButton is null! Check BindWidget in BP."));
    }

    // 绑定 Join 按钮
    if (JoinButton)
    {
        JoinButton->OnClicked.AddDynamic(this, &UMenu::JoinButtonClicked);
        UE_LOG(LogMultiplayerSessions, Log,
            TEXT("[Menu] Initialize | JoinButton bound"));
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] Initialize | JoinButton is null! Check BindWidget in BP."));
    }

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] Initialize | Complete | Widget ready for input"));
    return true;
}

// ===== HostButtonClicked — 点击 [Host] 按钮 =====
void UMenu::HostButtonClicked()
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] HostButton CLICKED | Disabling button → CreateSession(%d, %s)"),
        NumPublicConnections, *MatchType);

    HostButton->SetIsEnabled(false);

    if (MultiplayerSessionsSubsystem)
    {
        MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType);
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] HostButton | Subsystem is null! Cannot create session."));
        HostButton->SetIsEnabled(true);
    }
}

// ===== JoinButtonClicked — 点击 [Join] 按钮 =====
void UMenu::JoinButtonClicked()
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] JoinButton CLICKED | Disabling button → FindSessions(10000)"));

    JoinButton->SetIsEnabled(false);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
            TEXT("Join button clicked. Starting search..."));
    }

    if (MultiplayerSessionsSubsystem)
    {
        MultiplayerSessionsSubsystem->FindSessions(10000);
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] JoinButton | Subsystem is null! Cannot search."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("ERROR: MultiplayerSessionsSubsystem is null!"));
        }
        JoinButton->SetIsEnabled(true);
    }
}

// ===== OnLevelRemovedFromWorld — 关卡卸载时清理 =====
void UMenu::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
    // 只在持久关卡移除时才清理 UI，避免 World Partition 流式 Cell 卸载时误触发
    if (InLevel && InWorld && InLevel == InWorld->PersistentLevel)
    {
        UE_LOG(LogMultiplayerSessions, Log,
            TEXT("[Menu] OnLevelRemovedFromWorld | PersistentLevel removed → MenuTearDown"));
        MenuTearDown();
    }
    Super::OnLevelRemovedFromWorld(InLevel, InWorld);
}

// ===== OnCreateSession — Subsystem 创建会话完成的回调 =====
void UMenu::OnCreateSession(bool bWasSuccessful)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] OnCreateSession CALLBACK | Success=%d"), bWasSuccessful);

    if (bWasSuccessful)
    {
        // 成功：移除菜单 → ServerTravel 到 Lobby 地图（Listen Server）
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Menu] OnCreateSession SUCCESS | MenuTearDown → ServerTravel(%s)"),
            *PathToLobby);

        MenuTearDown();

        UWorld* World = GetWorld();
        if (World)
        {
            World->ServerTravel(PathToLobby);
        }
        else
        {
            UE_LOG(LogMultiplayerSessions, Error,
                TEXT("[Menu] OnCreateSession | World is null! Cannot ServerTravel."));
        }
    }
    else
    {
        // 失败：打印错误、重新启用按钮、保留菜单让玩家重试
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] OnCreateSession FAILED | Re-enabling HostButton, keeping menu open"));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("Failed to create session"));
        }
        HostButton->SetIsEnabled(true);
    }
}

// ===== OnFindSessions — Subsystem 搜索完成后的回调 =====
void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] OnFindSessions CALLBACK | Success=%d | Results=%d | SearchingMatchType=%s"),
        bWasSuccessful, SessionResults.Num(), *MatchType);

    // 子系统空检查
    if (MultiplayerSessionsSubsystem == nullptr)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] OnFindSessions | Subsystem is null! Re-enabling button."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("ERROR: MultiplayerSessionsSubsystem is null in OnFindSessions!"));
        }
        JoinButton->SetIsEnabled(true);
        return;
    }

    // 搜索失败（网络问题/Steam 不可用等）
    if (!bWasSuccessful)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] OnFindSessions | Search failed! Re-enabling button."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("Find sessions failed! Check network connection."));
        }
        JoinButton->SetIsEnabled(true);
        return;
    }

    // 遍历搜索结果，按 MatchType 过滤，加入第一个匹配的会话
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] OnFindSessions | Scanning %d results for MatchType=%s..."),
        SessionResults.Num(), *MatchType);

    for (int32 i = 0; i < SessionResults.Num(); ++i)
    {
        const auto& Result = SessionResults[i];
        FString SettingValue;
        Result.Session.SessionSettings.Get(FName("MatchType"), SettingValue);

        UE_LOG(LogMultiplayerSessions, Log,
            TEXT("[Menu] OnFindSessions | [%d/%d] Owner=%s | MatchType=%s | Ping=%d | NumOpen=%d"),
            i + 1, SessionResults.Num(),
            *Result.Session.OwningUserName,
            *SettingValue,
            Result.PingInMs,
            Result.Session.NumOpenPublicConnections);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
                FString::Printf(TEXT("  Session has MatchType: %s"), *SettingValue));
        }

        if (SettingValue == MatchType)
        {
            UE_LOG(LogMultiplayerSessions, Warning,
                TEXT("[Menu] OnFindSessions | MATCH FOUND [%d] | Owner=%s | → JoinSession"),
                i, *Result.Session.OwningUserName);

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
                    TEXT("Match found! Joining session..."));
            }
            MultiplayerSessionsSubsystem->JoinSession(Result);
            return; // 找到第一个匹配就加入，不再继续遍历
        }
    }

    // 没有匹配的会话
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] OnFindSessions | No session matched MatchType=%s | Re-enabling button"),
        *MatchType);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
            FString::Printf(TEXT("No session matched MatchType: %s"), *MatchType));
    }
    JoinButton->SetIsEnabled(true);
}

// ===== OnJoinSession — Subsystem 加入会话完成的回调 =====
void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] OnJoinSession CALLBACK | Result=%d (%s)"),
        (int32)Result,
        Result == EOnJoinSessionCompleteResult::Success ? TEXT("Success") :
        Result == EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("SessionIsFull") :
        Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? TEXT("SessionDoesNotExist") :
        Result == EOnJoinSessionCompleteResult::CouldNotRetrieveAddress ? TEXT("CouldNotRetrieveAddress") :
        Result == EOnJoinSessionCompleteResult::AlreadyInSession ? TEXT("AlreadyInSession") :
        TEXT("UnknownError"));

    // 加入失败：重新启用按钮让玩家重试
    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] OnJoinSession FAILED | Result=%d | Re-enabling JoinButton"),
            (int32)Result);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                FString::Printf(TEXT("Join session failed! Result: %d"), (int32)Result));
        }
        JoinButton->SetIsEnabled(true);
        return;
    }

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] OnJoinSession SUCCESS | Resolving connect string..."));

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            TEXT("Join session success! Resolving address..."));
    }

    // ── 解析阶段 1：获取 OnlineSubsystem ──
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (!Subsystem)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] OnJoinSession | OnlineSubsystem is null!"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("ERROR: OnlineSubsystem is null!"));
        }
        JoinButton->SetIsEnabled(true);
        return;
    }

    // ── 解析阶段 2：获取 SessionInterface ──
    IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] OnJoinSession | SessionInterface is invalid!"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("ERROR: SessionInterface is invalid!"));
        }
        JoinButton->SetIsEnabled(true);
        return;
    }

    // ── 解析阶段 3：解析连接地址 ──
    FString Address;
    if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] OnJoinSession | Failed to resolve connect string!"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("ERROR: Failed to resolve connect string!"));
        }
        JoinButton->SetIsEnabled(true);
        return;
    }

    // ── 解析阶段 4：地址空检查 ──
    if (Address.IsEmpty())
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] OnJoinSession | Resolved address is empty!"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("ERROR: Resolved address is empty!"));
        }
        JoinButton->SetIsEnabled(true);
        return;
    }

    // ── 解析成功：移除菜单、跳转到主机地址 ──
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] OnJoinSession | Address resolved: %s | MenuTearDown → ClientTravel"),
        *Address);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            FString::Printf(TEXT("Traveling to: %s"), *Address));
    }

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    MenuTearDown();

    if (PlayerController)
    {
        PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Menu] OnJoinSession | ClientTravel issued → %s"), *Address);
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Menu] OnJoinSession | PlayerController is null after MenuTearDown! Travel aborted."));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("ERROR: PlayerController is null!"));
        }
    }
}

// ===== MenuTearDown — 移除菜单 Widget，切回游戏输入模式 =====
void UMenu::MenuTearDown()
{
    UE_LOG(LogMultiplayerSessions, Log,
        TEXT("[Menu] MenuTearDown | Removing widget from viewport"));

    RemoveFromParent();

    UWorld* World = GetWorld();
    if (World)
    {
        APlayerController* PlayerController = World->GetFirstPlayerController();
        if (PlayerController)
        {
            FInputModeGameOnly InputModeData;
            PlayerController->SetInputMode(InputModeData);
            PlayerController->SetShowMouseCursor(false);
            UE_LOG(LogMultiplayerSessions, Log,
                TEXT("[Menu] MenuTearDown | InputMode → GameOnly | MouseCursor=false"));
        }
    }
}

// ===== OnDestroySession / OnStartSession — 目前无 UI 响应，仅日志 =====
void UMenu::OnDestroySession(bool bWasSuccessful)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] OnDestroySession CALLBACK | Success=%d | (no-op in UI)"),
        bWasSuccessful);
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Menu] OnStartSession CALLBACK | Success=%d | (no-op in UI)"),
        bWasSuccessful);
}
