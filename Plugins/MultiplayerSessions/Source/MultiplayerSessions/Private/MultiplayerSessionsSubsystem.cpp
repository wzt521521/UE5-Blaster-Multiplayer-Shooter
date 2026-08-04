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
    if (Subsystem)
    {
        SessionInterface = Subsystem->GetSessionInterface();
        UE_LOG(LogMultiplayerSessions, Log,
            TEXT("[Subsystem] Init | OSS=%s | SessionInterface=%s"),
            *Subsystem->GetSubsystemName().ToString(),
            SessionInterface.IsValid() ? TEXT("Valid") : TEXT("INVALID"));
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
    const bool bIsLAN = (IOnlineSubsystem::Get()->GetSubsystemName() == "NULL");
    LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
    LastSessionSettings->bIsLANMatch = bIsLAN;
    LastSessionSettings->bIsDedicated = bIsDedicatedServer;            // DS 标记：Steam 服务器列表区分类别
    LastSessionSettings->NumPublicConnections = NumPublicConnections;
    LastSessionSettings->bAllowJoinInProgress = true;
    LastSessionSettings->bAllowJoinViaPresence = true;
    LastSessionSettings->bShouldAdvertise = true;                      // Steam 服务器列表可见
    LastSessionSettings->bUsesPresence = true;                         // 好友可通过 Presence 发现
    LastSessionSettings->bUseLobbiesIfAvailable = !bIsDedicatedServer; // DS 不使用 Lobby，走经典服务器列表
    LastSessionSettings->BuildUniqueId = 1;                            // 确保每次创建的会话 ID 不同
    LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] CreateSession | Settings: bIsLAN=%d | bIsDedicated=%d | NumPublic=%d | bAdvertise=%d | bPresence=%d | bLobbies=%d | BuildId=%d"),
        bIsLAN, bIsDedicatedServer, NumPublicConnections,
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
void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults)
{
    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] FindSessions ENTER | MaxSearchResults=%d"), MaxSearchResults);

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
    const bool bIsLAN = (IOnlineSubsystem::Get()->GetSubsystemName() == "NULL");
    LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
    LastSessionSearch->MaxSearchResults = MaxSearchResults;
    LastSessionSearch->bIsLanQuery = bIsLAN;
    LastSessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

    UE_LOG(LogMultiplayerSessions, Warning,
        TEXT("[Subsystem] FindSessions | Settings: bIsLanQuery=%d | MaxResults=%d | SEARCH_PRESENCE=true"),
        bIsLAN, MaxSearchResults);

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
