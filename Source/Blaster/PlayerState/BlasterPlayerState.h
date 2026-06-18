// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BlasterPlayerState.generated.h"


class ABlasterCharacter;
class ABlasterPlayerController;
/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_Score() override;
	void AddToScore(float ScoreAmount);

	UFUNCTION()
	virtual void OnRep_Defeats();
	void AddToDefeats(int32 DefeatsAmount);
private:
	UPROPERTY()
	ABlasterCharacter* Character;//玩家角色——UPROPERTY 防止 Pawn 销毁后变成野指针
	UPROPERTY()
	ABlasterPlayerController* Controller;//玩家控制器——同上

	UPROPERTY(ReplicatedUsing = OnRep_Defeats)
	int32 Defeats;
};
