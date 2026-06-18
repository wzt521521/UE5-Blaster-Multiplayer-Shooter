// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BlasterGameMode.generated.h"
class ABlasterCharacter;
class ABlasterPlayerController;
class ACharacter;
class AController;
UCLASS()
class BLASTER_API ABlasterGameMode : public AGameMode
{
	GENERATED_BODY()
public:
	// EliminatedCharacter: 被淘汰的角色本体  VictimController: 被淘汰者Controller  AttackerController: 攻击者Controller
	virtual void PlayEliminated(
		ABlasterCharacter* EliminatedCharacter,
		ABlasterPlayerController* VictimController,
		ABlasterPlayerController* AttackerController
	);

	virtual void RequestRespawn(ACharacter* EliminatedCharacter, AController* EliminatedController);
};
