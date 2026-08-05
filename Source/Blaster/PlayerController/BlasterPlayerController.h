// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
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
	void SetHUDWeaponAmmo(int32 Ammo);
	void SetHUDCarriedAmmo(int32 Ammo);
void SetHUDMatchCountdown(float CountdownTime);
	void SetHUDPing(int32 Ping);

	// 弹药类型不匹配提示：显示绿色消息2秒后自动隐藏
	// 由 BlasterCharacter::ClientAmmoMismatchNotification RPC 调用
	void SetHUDMismatchNotification(const FString& Message);
	void SetHUDAnnouncementCountdown(float CountdownTime);
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float GetServerTime();
	// P3 开火时间窗校验：是否已完成与服务端的时钟同步（服务端本地标志，不复制）。
	// 客户端每 5s 自动发起 ServerRequestServerTime，服务端收到请求即置 true；
	// 同步前跳过 ClientShotTime 窗口校验，避免热身期/刚进场误杀。
	FORCEINLINE bool HasSyncedServerTime() const { return bHasSyncedTime; }
	virtual void ReceivedPlayer() override;
	void OnMatchStateSet(FName State, bool bTeamsMatch = false);
	void HandleMatchHasStarted(bool bTeamsMatch = false);
	void HandleCooldown();

	// 回合制阵营模式：MatchState 处理
	void HandleAssignTeams();
	void HandleRoundPrepare();
	void HandleRoundInProgress();
	void HandleRoundEnd();
	void HandleMatchEnd();
	void HandleHalftimeSwap();

	// 回合信息 HUD 推送

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

	// ── 炸弹 UI 推送（BombMode Phase 4）──
	void UpdateBombStatusUI(float RemainingTime, float TotalTime, const FString& StatusText, const FString& SiteName);
	void UpdateBombInteractUI(float Progress, const FString& PromptText, bool bVisible, bool bShowProgress = false);
	void ShowBombPlantedAnnouncement(const FString& SiteName);
	void UpdateBombStatusFromWorld();  // Tick 中检查已安放炸弹 → 推 StatusWidget

	float SingleTripTime = 0.f;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	void SetHUDTime();
	void PollInit();

	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float TimeOfClientRequest);

	// 客户端上报本地持久 PlayerId（GUID，见 FBlasterPlayerIdentity）：
	// 服务器写入 PlayerState，供比赛结算时持久化到 SQLite（按人归集的键）
	UFUNCTION(Server, Reliable)
	void ServerSetPlayerId(const FString& InPlayerId);

	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float TimeOfClientRequest, float TimeServerReceivedClientRequest);

	float ClientServerDelta = 0.f;

	// P3 开火时间窗校验：服务端收到过时间同步请求 → true（见 HasSyncedServerTime）
	bool bHasSyncedTime = false;

	UPROPERTY(EditAnywhere, Category = Time)
	float TimeSyncFrequency = 5.f;

	float TimeSyncRunningTime = 0.f;
	void CheckTimeSync(float DeltaTime);

	UFUNCTION(Server, Reliable)
	void ServerCheckMatchState();

	// 购买请求 RPC（客户端 -> 服务器）
	// 客户端只传 ItemID，服务器从 DataTable 查表获取 Price/Category/Class
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestPurchase(int32 ItemID);
	friend class UBuyMenu;

	UFUNCTION(Client, Reliable)
	void ClientJoinMidgame(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime);

private:
	UPROPERTY()
	ABlasterHud* BlasterHud;

	// 客户端兜底：无缝切图时服务器的 ClientSetHUD RPC 可能丢失（时序问题），
	// 导致 GetHUD() 仍是默认 AHUD、BlasterHud 为 null → 无 HUD。
	// 此方法在客户端直接加载并生成 BP_BlasterHUD，不依赖服务器 RPC。
	void EnsureBlasterHud();

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
	float HUDMatchCountdown;
	bool bInitializeMatchCountdown = false;
	int32 HUDCarriedAmmo;
	bool bInitializeCarriedAmmo = false;
	int32 HUDWeaponAmmo;
	bool bInitializeWeaponAmmo = false;

	// 不匹配提示自动隐藏 Timer（2秒）
	FTimerHandle MismatchNotificationTimer;
	void HideMismatchNotification();

	// 购买菜单是否正在显示，ShowBuyMenu/HideBuyMenu 维护此标志
	bool bBuyMenuOpen = false;

	// 客户端 GameState 复制延迟补偿：PollInit 中公告阶段每帧刷新公告文本

	// 投掷物径向选择面板是否正在显示，ShowThrowablePanel/HideThrowablePanel 维护此标志
	bool bThrowablePanelOpen = false;

	// 闪光弹配置已迁移到 BlasterHud

	FString GetInfoText(const TArray<class ABlasterPlayerState*>& Players);
};
