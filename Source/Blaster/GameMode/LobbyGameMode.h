// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
 
#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "LobbyGameMode.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API ALobbyGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 自动开局人数阈值（默认2，最小值2）
	UPROPERTY(EditDefaultsOnly, Category = "Lobby", meta = (ClampMin = "2"))
	int32 AimPeople = 2;

	// 目标游戏地图路径（支持 ?listen 等参数）
	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	FString GameMapPath = TEXT("/Game/Maps/Bomb_Defusal?listen");
};
