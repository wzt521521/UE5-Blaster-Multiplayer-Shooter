// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Engine/Engine.h"          // GEngine

// 定义日志分类（在头文件中声明，此处定义）
DEFINE_LOG_CATEGORY(LogMultiplayerSessions);

// ===== CONSTRUCTOR =====
UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem() :
    CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UMultiplayerSessionsSubsystem::OnCreateSessionComplete)),
    FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UMultiplayerSessionsSubsystem::OnFindSessionsComplete)),
    JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UMultiplayerSessionsSubsystem::OnJoinSessionComplete)),
    DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UMultiplayerSessionsSubsystem::OnDestroySessionComplete)),
    StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &UMultiplayerSessionsSubsystem::OnStartSessionComplete))
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();

    // 读取配置中的 SteamDevAppId，便于判断是否按预期走了 Steam
    int32 SteamDevAppId = 0;
    GConfig->GetInt(TEXT("OnlineSubsystemSteam"), TEXT("SteamDevAppId"), SteamDevAppId, GEngineIni);

    if (Subsystem)
    {
        SessionInterface = Subsystem->GetSessionInterface();
        // ★ 核心诊断：OSS=Steam 才正常；OSS=NULL 说明 Steam 初始化失败，UE 引擎静默回退到 NULL OSS
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Subsystem] Init | OSS=%s | SessionInterface=%s | SteamDevAppId=%d   <-- OSS=NULL 代表 Steam 加载失败已回退!"),
            *Subsystem->GetSubsystemName().ToString(),
            SessionInterface.IsValid() ? TEXT("Valid") : TEXT("INVALID"),
            SteamDevAppId);
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] Init FAILED | OnlineSubsystem is null! Steam may not be running."));
    }

    // 打印已绑定的原生委托（构造时绑定，生命周期内不变化）
    UE_LOG(LogMultiplayerSessions, Log,
        TEXT("[Subsystem] Init | Delegates bound: CreateSession/FindSessions/JoinSession/DestroySession/StartSession"));
}

// ===== CREATE SESSION =====
void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType, bool bIsDedicatedServer)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] CreateSession ENTER | NumPublicConnections=%d | MatchType=%s | bIsDedicated=%d"),
        NumPublicConnections, *MatchType, bIsDedicatedServer);

    // ★ 每次创建会话都明确打印当前 OSS：NULL=Steam 未加载(只能 LAN 发现，PIE 内必然 0 结果)
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] CreateSession | 当前 OSS = %s  <-- 必须为 Steam，否则联机发现必然失败"),
        IOnlineSubsystem::Get() ? *IOnlineSubsystem::Get()->GetSubsystemName().ToString() : TEXT("nullptr"));

    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] CreateSession ABORT | SessionInterface is invalid!"));
        return;
    }

    // 检查是否已有同名会话：如果有，先销毁旧会话再创建新会话
    auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession);
    if (ExistingSession != nullptr)
    {
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Subsystem] CreateSession | Existing session found → Destroy then recreate | NumConnections=%d | MatchType=%s"),
            NumPublicConnections, *MatchType);
        bCreateSessionOnDestroy = true;
        LastNumPublicConnections = NumPublicConnections;
        LastMatchType = MatchType;
        DestroySession();
        return;
    }

    // 注册原生委托：Steam OSS 完成后回调 OnCreateSessionComplete
    CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
    UE_LOG(LogMultiplayerSessions, Log,
        TEXT("[Subsystem] CreateSession | Delegate registered"));

    // 填充会话设置
    const bool bIsNULL_OSS = (IOnlineSubsystem::Get()->GetSubsystemName() == "NULL");
    // NULL OSS（编辑器内测试）只能走 LAN Beacon 发现，不存在 Steam Master Server，
    // bIsDedicated/bUseLobbiesIfAvailable 仅对真实 OSS（Steam/EOS）有意义，NULL 下保持 false/true
    const bool bIsDedicatedSetting = bIsNULL_OSS ? false : bIsDedicatedServer;
    const bool bUseLobbies = bIsNULL_OSS ? true : !bIsDedicatedServer;

    LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
    LastSessionSettings->bIsLANMatch = bIsNULL_OSS;                    // NULL OSS → LAN；Steam → 互联网
    LastSessionSettings->bIsDedicated = bIsDedicatedSetting;           // NULL OSS 无 Master Server，不标记 Dedicated
    LastSessionSettings->NumPublicConnections = NumPublicConnections;
    LastSessionSettings->bAllowJoinInProgress = true;
    LastSessionSettings->bAllowJoinViaPresence = true;
    LastSessionSettings->bShouldAdvertise = true;                      // 服务器列表可见（Steam）或 LAN 广播（NULL）
    LastSessionSettings->bUsesPresence = true;                         // 好友可通过 Presence 发现
    LastSessionSettings->bUseLobbiesIfAvailable = bUseLobbies;         // NULL OSS 不用 Lobby（无此功能）
    LastSessionSettings->BuildUniqueId = 1;                            // 确保每次创建的会话 ID 不同
    LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] CreateSession | Settings: bIsLAN=%d | bIsDedicated=%d(raw=%d) | NumPublic=%d | bAdvertise=%d | bPresence=%d | bLobbies=%d | BuildId=%d"),
        bIsNULL_OSS, bIsDedicatedSetting, bIsDedicatedServer, NumPublicConnections,
        LastSessionSettings->bShouldAdvertise,
        LastSessionSettings->bUsesPresence,
        LastSessionSettings->bUseLobbiesIfAvailable,
        LastSessionSettings->BuildUniqueId);

    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (!LocalPlayer)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] CreateSession ABORT | LocalPlayer is null! No player controller?"));
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
        MultiplayerOnCreateSessionComplete.Broadcast(false);
        return;
    }

    // 发起创建请求
    const bool bResult = SessionInterface->CreateSession(
        *LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings);

    if (!bResult)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] CreateSession FAILED | Synchronous call returned false → broadcasting failure"));
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
        MultiplayerOnCreateSessionComplete.Broadcast(false);
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Subsystem] CreateSession | Request sent to Steam... waiting for callback"));
    }
}

// ===== FIND SESSIONS =====
void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults, bool bSearchDedicated)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] FindSessions ENTER | MaxSearchResults=%d | bSearchDedicated=%d"), MaxSearchResults, bSearchDedicated);

    // ★ 同 CreateSession：打印当前 OSS，判断是 Steam 还是 NULL 回退
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] FindSessions | 当前 OSS = %s  <-- 必须为 Steam"),
        IOnlineSubsystem::Get() ? *IOnlineSubsystem::Get()->GetSubsystemName().ToString() : TEXT("nullptr"));

    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] FindSessions ABORT | SessionInterface is invalid!"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("[Subsystem] SessionInterface invalid!"));
        }
        return;
    }

    // 注册原生委托：Steam 搜索完成后回调 OnFindSessionsComplete
    FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
    UE_LOG(LogMultiplayerSessions, Log,
        TEXT("[Subsystem] FindSessions | Delegate registered"));

    // 填充搜索配置
    const bool bIsNULL_OSS = (IOnlineSubsystem::Get()->GetSubsystemName() == "NULL");
    // ★ 搜索方式必须与目标会话类型匹配：
    //   - DS 模式（bSearchDedicated=true）：DS 创建的是 LAN 会话（bIsLANMatch=true），客户端必须走 LAN 发现（bIsLanQuery=true）
    //   - NULL OSS：本就走 LAN
    //   - Listen 模式：走互联网（Steam Lobby 搜索）
    const bool bIsLAN = bSearchDedicated || bIsNULL_OSS;
    LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = MaxSearchResults;
    LastSessionSearch->bIsLanQuery = bIsLAN;
    // SEARCH_PRESENCE 保持 true：Steam 的 FindLANSession 有 Presence 才走 LAN beacon 搜索（与 DS 的 LAN beacon 匹配）
    LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] FindSessions | Settings: bIsLanQuery=%d | MaxResults=%d | SEARCH_PRESENCE=true | bSearchDedicated=%d"),
        bIsLAN, MaxSearchResults, bSearchDedicated);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
            FString::Printf(TEXT("[Subsystem] Finding sessions (Max: %d, LAN: %s)..."),
                MaxSearchResults, bIsLAN ? TEXT("true") : TEXT("false")));
    }

    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (!LocalPlayer)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] FindSessions ABORT | LocalPlayer is null!"));
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
        MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
        return;
    }

    // 发起搜索请求
    const bool bResult = SessionInterface->FindSessions(
        *LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef());

    if (!bResult)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] FindSessions FAILED | Synchronous call returned false → broadcasting empty result"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("[Subsystem] FindSessions failed synchronously!"));
        }
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
        MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Subsystem] FindSessions | Request sent to Steam... waiting for callback"));
    }
}

// ===== JOIN SESSION =====
void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] JoinSession ENTER"));

    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] JoinSession ABORT | SessionInterface is invalid!"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("[Subsystem] SessionInterface invalid in JoinSession!"));
        }
        MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
        return;
    }

    // 打印目标会话的简要信息
    FString TargetMatchType;
    SessionResult.Session.SessionSettings.Get(FName("MatchType"), TargetMatchType);
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] JoinSession | Target: Owner=%s | MatchType=%s | NumOpen=%d | Ping=%d"),
        *SessionResult.Session.OwningUserName,
        *TargetMatchType,
        SessionResult.Session.NumOpenPublicConnections,
        SessionResult.PingInMs);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
            TEXT("[Subsystem] Joining session..."));
    }

    // 注册原生委托：Steam 加入完成后回调 OnJoinSessionComplete
    JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
    UE_LOG(LogMultiplayerSessions, Log,
        TEXT("[Subsystem] JoinSession | Delegate registered"));

    const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (!LocalPlayer)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] JoinSession ABORT | LocalPlayer is null!"));
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
        MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
        return;
    }

    // 发起加入请求
    const bool bResult = SessionInterface->JoinSession(
        *LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult);

    if (!bResult)
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] JoinSession FAILED | Synchronous call returned false"));
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                TEXT("[Subsystem] JoinSession failed synchronously!"));
        }
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
        MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Subsystem] JoinSession | Request sent to Steam... waiting for callback"));
    }
}

// ===== DESTROY SESSION =====
void UMultiplayerSessionsSubsystem::DestroySession()
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] DestroySession ENTER | bCreateSessionOnDestroy=%d"),
        bCreateSessionOnDestroy);

    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] DestroySession ABORT | SessionInterface is invalid!"));
        MultiplayerOnDestroySessionComplete.Broadcast(false);
        return;
    }

    DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);
    UE_LOG(LogMultiplayerSessions, Log,
        TEXT("[Subsystem] DestroySession | Delegate registered"));

    if (!SessionInterface->DestroySession(NAME_GameSession))
    {
        UE_LOG(LogMultiplayerSessions, Error,
            TEXT("[Subsystem] DestroySession FAILED | Synchronous call returned false"));
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
        MultiplayerOnDestroySessionComplete.Broadcast(false);
    }
    else
    {
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Subsystem] DestroySession | Request sent... waiting for callback"));
    }
}

// ===== START SESSION =====
void UMultiplayerSessionsSubsystem::StartSession()
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] StartSession | Not implemented yet — no-op"));
}

// =================================================================================================
// 原生委托回调函数（Steam OSS 异步返回）
// 注意：每个回调都会先清除注册的原生委托 Handle，再广播自定义委托给 Menu 层
// =================================================================================================

// ── OnCreateSessionComplete ──
void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (SessionInterface)
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
    }

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] OnCreateSessionComplete CALLBACK | SessionName=%s | Success=%d | → Broadcasting to Menu"),
        *SessionName.ToString(), bWasSuccessful);

    // 广播自定义委托，Menu::OnCreateSession 接收并决定是否 ServerTravel
    MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

// ── OnFindSessionsComplete ──
void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
    // 清理原生委托注册
    if (SessionInterface)
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
    }

    const int32 NumResults = LastSessionSearch.IsValid() ? LastSessionSearch->SearchResults.Num() : 0;

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] OnFindSessionsComplete CALLBACK | Success=%d | Results=%d | → Broadcasting to Menu"),
        bWasSuccessful, NumResults);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
            FString::Printf(TEXT("[Subsystem] OnFindSessionsComplete: success=%d, results=%d"),
                bWasSuccessful, NumResults));
    }

    // 搜索结果为空：广播空数组 + 失败标志，让 Menu 层知道没有可用房间
    if (NumResults <= 0)
    {
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Subsystem] OnFindSessionsComplete | No results → broadcasting empty array"));
        MultiplayerOnFindSessionsComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
        return;
    }

    // 打印每个搜索结果的 MatchType，方便排查过滤问题
    for (int32 i = 0; i < NumResults; ++i)
    {
        const auto& Result = LastSessionSearch->SearchResults[i];
        FString SettingValue;
        Result.Session.SessionSettings.Get(FName("MatchType"), SettingValue);
        UE_LOG(LogMultiplayerSessions, Log,
            TEXT("[Subsystem] OnFindSessionsComplete | [%d/%d] Owner=%s | MatchType=%s | Ping=%d | NumOpen=%d"),
            i + 1, NumResults,
            *Result.Session.OwningUserName,
            *SettingValue,
            Result.PingInMs,
            Result.Session.NumOpenPublicConnections);
    }

    // 广播搜索结果，由 Menu::OnFindSessions 按 MatchType 过滤
    MultiplayerOnFindSessionsComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

// ── OnJoinSessionComplete ──
void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    // 清理原生委托注册
    if (SessionInterface)
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
    }

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] OnJoinSessionComplete CALLBACK | SessionName=%s | Result=%d (%s) | → Broadcasting to Menu"),
        *SessionName.ToString(),
        (int32)Result,
        Result == EOnJoinSessionCompleteResult::Success ? TEXT("Success") :
        Result == EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("SessionIsFull") :
        Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? TEXT("SessionDoesNotExist") :
        Result == EOnJoinSessionCompleteResult::CouldNotRetrieveAddress ? TEXT("CouldNotRetrieveAddress") :
        Result == EOnJoinSessionCompleteResult::AlreadyInSession ? TEXT("AlreadyInSession") :
        TEXT("UnknownError"));

    // 广播加入结果，由 Menu::OnJoinSession 处理（成功 → ClientTravel）
    MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

// ── OnDestroySessionComplete ──
void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (SessionInterface)
    {
        SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
    }

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] OnDestroySessionComplete CALLBACK | SessionName=%s | Success=%d | bRecreate=%d"),
        *SessionName.ToString(), bWasSuccessful, bCreateSessionOnDestroy);

    // 如果销毁是为了重建（CreateSession 检测到已有会话时的"先销毁再创建"流程）
    if (bWasSuccessful && bCreateSessionOnDestroy)
    {
        UE_LOG(LogMultiplayerSessions, Warning,
            TEXT("[Subsystem] OnDestroySessionComplete | Destroyed for recreate → Calling CreateSession(%d, %s)"),
            LastNumPublicConnections, *LastMatchType);
        bCreateSessionOnDestroy = false;
        CreateSession(LastNumPublicConnections, LastMatchType);
    }

    // 广播自定义委托
    MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

// ── OnStartSessionComplete ──
void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] OnStartSessionComplete CALLBACK | SessionName=%s | Success=%d | (no-op)"),
        *SessionName.ToString(), bWasSuccessful);

    // StartSession 暂未使用，保留空实现
}
