// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Engine.h"

void ALobbyGameMode::PostLogin(APlayerController *NewPlayer)
{
    Super::PostLogin(NewPlayer);

    const FString DebugMsg = FString::Printf(TEXT("[LobbyGameMode] PostLogin called"));
    UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMsg);
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, DebugMsg);

    if (!GameState)
    {
        UE_LOG(LogTemp, Error, TEXT("[LobbyGameMode] GameState is null!"));
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[LobbyGameMode] GameState is null!"));
        return;
    }

    int32 NumOfPlayers = GameState->PlayerArray.Num();
    const FString PlayerCountMsg = FString::Printf(TEXT("[LobbyGameMode] NumOfPlayers = %d"), NumOfPlayers);
    UE_LOG(LogTemp, Warning, TEXT("%s"), *PlayerCountMsg);
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, PlayerCountMsg);

    if (NumOfPlayers == 2)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            const FString TravelMsg = TEXT("[LobbyGameMode] ServerTravel -> BlasterMap");
            UE_LOG(LogTemp, Warning, TEXT("%s"), *TravelMsg);
            GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, TravelMsg);

            bUseSeamlessTravel = true;
            World->ServerTravel(FString("/Game/Maps/BlasterMap?listen"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[LobbyGameMode] GetWorld() is null!"));
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("[LobbyGameMode] GetWorld() is null!"));
        }
    }
}
