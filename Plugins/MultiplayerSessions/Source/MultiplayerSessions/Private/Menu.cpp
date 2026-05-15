// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
void UMenu::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch,FString LobbyPath)
{
    PathToLobby = FString::Printf(TEXT("%s?listen"),*LobbyPath);
    NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;
    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);
    bIsFocusable = true;
    //获取玩家控制器并设置输入模式为UIOnly
    UWorld* World = GetWorld();
    if (World)   {
        APlayerController* PlayerController = World->GetFirstPlayerController();
        if(PlayerController) {
            FInputModeUIOnly InputModeData;
            InputModeData.SetWidgetToFocus(TakeWidget());
            InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PlayerController->SetInputMode(InputModeData);
            PlayerController->SetShowMouseCursor(true);
        }
    }
    //获取GameInstance并从中获取MultiplayerSessionsSubsystem
     
    UGameInstance* GameInstance = GetGameInstance();
    if(GameInstance) {
        MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
    }

    //绑定委托，后面当广播时会自动调用对应函数（关键）
    if(MultiplayerSessionsSubsystem) {
        MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &UMenu::OnCreateSession);
        MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &UMenu::OnFindSessions);
        MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &UMenu::OnJoinSession);
        MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &UMenu::OnDestroySession);
        MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &UMenu::OnStartSession);
    }
}

void UMenu::HostButtonClicked()
{
    HostButton->SetIsEnabled(false);
    //调用MultiplayerSessionsSubsystem的CreateSession函数创建会话并且切换地图
    if(MultiplayerSessionsSubsystem) {
        MultiplayerSessionsSubsystem->CreateSession(NumPublicConnections, MatchType);

    }
}


void UMenu::JoinButtonClicked()
{
    JoinButton->SetIsEnabled(false);
    if(GEngine) {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("Join button clicked. Starting search..."));
    }
    if(MultiplayerSessionsSubsystem) {
        MultiplayerSessionsSubsystem->FindSessions(10000);
    }
    else {
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: MultiplayerSessionsSubsystem is null!"));
        }
    }
}

bool UMenu::Initialize()
{
    
    if(!Super::Initialize()) {
        return false;
    }

    //绑定按钮点击事件
    if(HostButton) {
        HostButton->OnClicked.AddDynamic(this, 
            &UMenu::HostButtonClicked);
    }
    //绑定按钮点击事件
    if(JoinButton) {
        JoinButton->OnClicked.AddDynamic(this, 
            &UMenu::JoinButtonClicked);
    }

    return true;
}

void UMenu::OnLevelRemovedFromWorld(ULevel *InLevel, UWorld *InWorld)
{
    // 只在持久关卡移除时才清理 UI，避免 World Partition 流式 Cell 卸载时误触发
    if (InLevel && InWorld && InLevel == InWorld->PersistentLevel)
    {
        MenuTearDown();
    }
    Super::OnLevelRemovedFromWorld(InLevel, InWorld);
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
    if(bWasSuccessful){
        MenuTearDown();
        UWorld* World = GetWorld();
        if(World) {
            World->ServerTravel(PathToLobby);
        }
    }
    else{
        if (GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to create session"));
        }
        HostButton->SetIsEnabled(true);
        MenuTearDown();
    }
}

void UMenu::MenuTearDown()
{

    //从视口中移除菜单并设置输入模式为游戏模式
    RemoveFromParent();
    UWorld* World = GetWorld();
    if(World) {
        APlayerController* PlayerController = World->GetFirstPlayerController();
        if(PlayerController) {
            FInputModeGameOnly InputModeData;
            PlayerController->SetInputMode(InputModeData);
            PlayerController->SetShowMouseCursor(false);
        }
    }
}

void UMenu::OnFindSessions(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
    if(MultiplayerSessionsSubsystem == nullptr) {
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: MultiplayerSessionsSubsystem is null in OnFindSessions!"));
        }
        return;
    }

    if(!bWasSuccessful) {
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Find sessions failed! Check network connection."));
        }
        return;
    }

    // if(SessionResults.Num() == 0) {
    //     if(GEngine) {
    //         GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("No sessions found. No one has created a room yet."));
    //     }
    //     return;
    // }

    // if(GEngine) {
    //     GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
    //         FString::Printf(TEXT("Found %d session(s), searching for MatchType: %s"), SessionResults.Num(), *MatchType));
    // }

    for(auto Result : SessionResults) {
        FString SettingValue;
        Result.Session.SessionSettings.Get(FName("MatchType"), SettingValue);
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
                FString::Printf(TEXT("  Session has MatchType: %s"), *SettingValue));
        }
        if(SettingValue == MatchType) {
            if(GEngine) {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Match found! Joining session..."));
            }
            MultiplayerSessionsSubsystem->JoinSession(Result);
            return;
        }
    }

    if(GEngine) {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow,
            FString::Printf(TEXT("No session matched MatchType: %s"), *MatchType));
    }
    if(!bWasSuccessful||SessionResults.Num() == 0) {
        JoinButton->SetIsEnabled(true);

    }
}
void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
    if(Result != EOnJoinSessionCompleteResult::Success) {
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
                FString::Printf(TEXT("Join session failed! Result: %d"), (int32)Result));
        }
        return;
    }

    if(GEngine) {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Join session success! Resolving address..."));
    }

    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if(!Subsystem) {
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: OnlineSubsystem is null!"));
        }
        return;
    }

    IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
    if(!SessionInterface.IsValid()) {
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: SessionInterface is invalid!"));
        }
        return;
    }

    FString Address;
    if(!SessionInterface->GetResolvedConnectString(NAME_GameSession, Address)) {
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: Failed to resolve connect string!"));
        }
        return;
    }

    if(Address.IsEmpty()) {
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: Resolved address is empty!"));
        }
        return;
    }

    if(GEngine) {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
            FString::Printf(TEXT("Traveling to: %s"), *Address));
    }

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    MenuTearDown();
    if(PlayerController) {
        PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
    }
    else {
        if(GEngine) {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: PlayerController is null!"));
        }
    }

    if(Result != EOnJoinSessionCompleteResult::Success) {
        JoinButton->SetIsEnabled(true);
    }
}

void UMenu::OnDestroySession(bool bWasSuccessful)
{

}

void UMenu::OnStartSession(bool bWasSuccessful)
{

}