// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LobbyGameMode.generated.h"

// 大厅日志分类，控制台用 LogLobby 标签筛选
DECLARE_LOG_CATEGORY_EXTERN(LogLobby, Log, All);

/**
 * 
 */
UCLASS()
class BLASTER_API ALobbyGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	ALobbyGameMode();
	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// 自动开局人数阈值（默认2，最小值2）
	UPROPERTY(EditDefaultsOnly, Category = "Lobby", meta = (ClampMin = "2"))
	int32 AimPeople = 2;

	// 目标游戏地图路径（ServerTravel 自动保持 Listen 模式，无需 ?listen）
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FString GameMapPath = TEXT("/Game/Maps/BombDefusalGameMode");
};
