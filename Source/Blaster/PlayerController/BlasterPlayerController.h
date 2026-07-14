// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BlasterPlayerController.generated.h"

class ABlasterHud;

UCLASS()
class BLASTER_API ABlasterPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDShield(float Shield, float MaxShield);
	void SetHUDScore(float Score);
	void SetHUDDefeats(int32 Defeats);
	void SetHUDWeaponAmmo(int32 Ammo);
	void SetHUDCarriedAmmo(int32 Ammo);
void SetHUDMatchCountdown(float CountdownTime);

	// 弹药类型不匹配提示：显示绿色消息2秒后自动隐藏
	// 由 BlasterCharacter::ClientAmmoMismatchNotification RPC 调用
	void SetHUDMismatchNotification(const FString& Message);
	void SetHUDAnnouncementCountdown(float CountdownTime);
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float GetServerTime();
	virtual void ReceivedPlayer() override;
	void OnMatchStateSet(FName State, bool bTeamsMatch = false);
	void HandleMatchHasStarted(bool bTeamsMatch = false);
	void HandleCooldown();

	// 购买菜单生命周期：热身开始自动打开，B 键切换，比赛开始强制关闭
	void OpenBuyMenuOnWarmup();
	void ShowBuyMenu();
	void HideBuyMenu();
	void ToggleBuyMenu();

	float SingleTripTime = 0.f;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	void SetHUDTime();
	void PollInit();

	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);

	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float TimeOfClientRequest, float TimeServerReceivedClientRequest);

	float ClientServerDelta = 0.f;

	UPROPERTY(EditAnywhere, Category = Time)
	float TimeSyncFrequency = 5.f;

	float TimeSyncRunningTime = 0.f;
	void CheckTimeSync(float DeltaTime);

	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState();

	UFUNCTION(Client, Reliable)
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime);

private:
	UPROPERTY()
	ABlasterHud* BlasterHud;

	UPROPERTY()
	class UCharacteroverlay* CharacterOverlay;

	UPROPERTY()
	class ABlasterGameMode* BlasterGameMode;

	float LevelStartingTime = 0.f;
	float MatchTime = 0.f;
	float WarmupTime = 0.f;
	float CooldownTime = 0.f;
	uint32 CountdownInt = 0;

	UPROPERTY(ReplicatedUsing = OnRep_MatchState)
	FName MatchState;

	UFUNCTION()
	void OnRep_MatchState();

	float HUDHealth;
	bool bInitializeHealth = false;
	float HUDMaxHealth;
	float HUDShield;
	bool bInitializeShield = false;
	float HUDMaxShield;
	float HUDScore;
	bool bInitializeScore = false;
	int32 HUDDefeats;
	bool bInitializeDefeats = false;
	float HUDMatchCountdown;
	bool bInitializeMatchCountdown = false;
	int32 HUDCarriedAmmo;
	bool bInitializeCarriedAmmo = false;

	// 不匹配提示自动隐藏 Timer（2秒）
	FTimerHandle MismatchNotificationTimer;
	void HideMismatchNotification();

	// 购买菜单是否正在显示，ShowBuyMenu/HideBuyMenu 维护此标志
	bool bBuyMenuOpen = false;

	FString GetInfoText(const TArray<class ABlasterPlayerState*>& Players);
};
