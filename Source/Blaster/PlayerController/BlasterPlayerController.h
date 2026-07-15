// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BlasterPlayerController.generated.h"

class ABlasterHud;
class USoundCue;
enum class EThrowableType : uint8;

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

	// 投掷物选择面板：按住 G 键显示，点击图标确认选择
	void ShowThrowablePanel();
	void HideThrowablePanel();

	// 点击选中回调：由 ThrowableSelectionWheel 的 OnTypeClicked 委托触发
	UFUNCTION()
	void OnThrowableTypeClicked(EThrowableType Type);

	// 投掷物烹饪倒计时 HUD 推送：每帧由 ThrowableComponent::TickComponent 调用
	// bIsCooking=true → RemainingSeconds 为剩余秒数（如 1.3），显示倒计时文本
	// bIsCooking=false → 隐藏倒计时文本
	void SetHUDThrowableCooking(bool bIsCooking, float RemainingSeconds);

	// 闪光弹致盲 Client RPC：服务器调用，客户端触发全屏白屏淡出
	UFUNCTION(Client, Reliable)
	void ClientApplyFlashEffect(float Duration);

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

	// 投掷物径向选择面板是否正在显示，ShowThrowablePanel/HideThrowablePanel 维护此标志
	bool bThrowablePanelOpen = false;

	// 闪光弹配置
	UPROPERTY(EditAnywhere, Category = "Throwable|Flashbang")
	USoundCue* FlashbangTinnitusSound;

	// 闪光弹白屏淡出状态
	FTimerHandle FlashFadeTimer;
	float FlashEffectStartTime = 0.f;
	float FlashEffectDuration = 0.f;

	// 每帧更新白屏透明度，Alpha ≤ 0 时隐藏 FlashOverlay
	void TickFlashFade();

	FString GetInfoText(const TArray<class ABlasterPlayerState*>& Players);
};
