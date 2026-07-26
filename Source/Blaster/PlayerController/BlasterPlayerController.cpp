// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerController.h"
#include "Blaster/HUD/BlasterHud.h"
#include "Blaster/HUD/Characteroverlay.h"
#include "Blaster/HUD/Announcement.h"
#include "Blaster/HUD/BuyMenu.h"
#include "Blaster/HUD/ThrowableSelectionWheel.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Sound/SoundCue.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/BlasterComponents/ThrowableComponent.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "Blaster/GameMode/BombDefusalGameMode.h"
#include "Blaster/HUD/RoundOverlay.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/BlasterTypes/Announcement.h"
#include "Kismet/GameplayStatics.h"

void ABlasterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		// B 键绑定购买菜单开关，仅在热身阶段生效（ToggleBuyMenu 内部检查 MatchState）
		InputComponent->BindAction("OpenBuyMenu", IE_Pressed, this, &ABlasterPlayerController::ToggleBuyMenu);
	}
}

void ABlasterPlayerController::BeginPlay()
{
	Super::BeginPlay();
	BlasterHud = Cast<ABlasterHud>(GetHUD());
	//应该添加annocuncement
	//announcement已经通过ServerCheckMatchState()由客户端独自添加
	ServerCheckMatchState();
}

void ABlasterPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SetHUDTime();
	CheckTimeSync(DeltaTime);
	PollInit();

	// 每帧更新客户端 ping 显示，PlayerState::GetPingInMilliseconds 引擎内置复制
	if (GetPlayerState<APlayerState>())
	{
		SetHUDPing(FMath::RoundToInt(GetPlayerState<APlayerState>()->GetPingInMilliseconds()));
	}
}

void ABlasterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABlasterPlayerController, MatchState);
}

void ABlasterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(InPawn);
	if (BlasterCharacter)
	{
		SetHUDHealth(BlasterCharacter->GetHealth(), BlasterCharacter->GetMaxHealth());
		SetHUDShield(BlasterCharacter->GetShield(), BlasterCharacter->GetMaxShield());
	}
}

void ABlasterPlayerController::SetHUDHealth(float Health, float MaxHealth)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay
		&& BlasterHud->CharacterOverlay->HealthBar && BlasterHud->CharacterOverlay->HealthText;

	if (bHUDValid)
	{
		const float HealthPercent = Health / MaxHealth;
		BlasterHud->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		FString HealthTextStr = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		BlasterHud->CharacterOverlay->HealthText->SetText(FText::FromString(HealthTextStr));
	}
	else
	{
		bInitializeHealth = true;
		HUDHealth = Health;
		HUDMaxHealth = MaxHealth;
	}
}

void ABlasterPlayerController::SetHUDShield(float Shield, float MaxShield)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay
		&& BlasterHud->CharacterOverlay->ShieldBar && BlasterHud->CharacterOverlay->ShieldText;

	if (bHUDValid)
	{
		const float ShieldPercent = Shield / MaxShield;
		BlasterHud->CharacterOverlay->ShieldBar->SetPercent(ShieldPercent);
		FString ShieldTextStr = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Shield), FMath::CeilToInt(MaxShield));
		BlasterHud->CharacterOverlay->ShieldText->SetText(FText::FromString(ShieldTextStr));
	}
	else
	{
		bInitializeShield = true;
		HUDShield = Shield;
		HUDMaxShield = MaxShield;
	}
}

void ABlasterPlayerController::SetHUDWeaponAmmo(int32 Ammo)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay && BlasterHud->CharacterOverlay->AmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHud->CharacterOverlay->AmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		// 延迟缓存：Overlay 尚未创建时缓存数据，PollInit 中推送
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = Ammo;
	}
}

void ABlasterPlayerController::SetHUDCarriedAmmo(int32 Ammo)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay && BlasterHud->CharacterOverlay->CarriedAmmoAmount;
	if (bHUDValid)
	{
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		BlasterHud->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeCarriedAmmo = true;
		HUDCarriedAmmo = Ammo;
	}
}

// ------------------------------------------------------------
// 延迟显示：每帧 Tick 读取 PlayerState::GetPingInMilliseconds()
// 引擎内置复制，客户端直接读取即可，无需额外网络同步
// ------------------------------------------------------------
void ABlasterPlayerController::SetHUDPing(int32 Ping)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay && BlasterHud->CharacterOverlay->PingText;
	if (bHUDValid)
	{
		FString PingStr = FString::Printf(TEXT("%d ms"), Ping);
		BlasterHud->CharacterOverlay->PingText->SetText(FText::FromString(PingStr));
	}
}

void ABlasterPlayerController::SetHUDMismatchNotification(const FString& Message)
{
	// 获取 HUD 和 CharacterOverlay，直接设置 MismatchNotificationText 控件
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay
		&& BlasterHud->CharacterOverlay->MismatchNotificationText;

	if (bHUDValid)
	{
		// 设置绿色提示文本并显示
		BlasterHud->CharacterOverlay->MismatchNotificationText->SetText(FText::FromString(Message));
		BlasterHud->CharacterOverlay->MismatchNotificationText->SetVisibility(ESlateVisibility::Visible);

		// 启动2秒 Timer，到期后调用 HideMismatchNotification 隐藏文本
		// SetTimer 会覆盖已有的 Timer，重复触发时自动重置倒计时
		GetWorldTimerManager().SetTimer(
			MismatchNotificationTimer,
			this,
			&ABlasterPlayerController::HideMismatchNotification,
			2.0f
		);
	}
}

void ABlasterPlayerController::HideMismatchNotification()
{
	// Timer 到期：隐藏不匹配提示文本
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud && BlasterHud->CharacterOverlay
		&& BlasterHud->CharacterOverlay->MismatchNotificationText)
	{
		BlasterHud->CharacterOverlay->MismatchNotificationText->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ABlasterPlayerController::SetHUDMatchCountdown(float CountdownTime)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud &&
		BlasterHud->CharacterOverlay &&
		BlasterHud->CharacterOverlay->MatchCountdownText;
	if (bHUDValid)
	{
		if (CountdownTime < 0.f)
		{
			BlasterHud->CharacterOverlay->MatchCountdownText->SetText(FText());
			return;
		}

		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHud->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
	else
	{
		bInitializeMatchCountdown = true;
		HUDMatchCountdown = CountdownTime;
	}
}

void ABlasterPlayerController::SetHUDAnnouncementCountdown(float CountdownTime)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	// 兜底：如果 Announcement 还未创建（ClientJoinMidgame 时 HUD 可能未就绪），在这里补创建
	if (BlasterHud && BlasterHud->Announcement == nullptr)
	{
		BlasterHud->AddAnnouncement();
	}
	bool bHUDValid = BlasterHud &&
		BlasterHud->Announcement &&
		BlasterHud->Announcement->WarmupTime;
	if (bHUDValid)
	{
		if (CountdownTime < 0.f)
		{
			BlasterHud->Announcement->WarmupTime->SetText(FText());
			return;
		}

		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;

		FString CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		BlasterHud->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

// ------------------------------------------------------------
// 时间同步：客户端定期向服务器请求时间，计算出 ClientServerDelta
// 然后 GetServerTime() 就能返回接近服务器的时间
// ------------------------------------------------------------
void ABlasterPlayerController::CheckTimeSync(float DeltaTime)
{
	TimeSyncRunningTime += DeltaTime;
	if (IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency)
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		TimeSyncRunningTime = 0.f;
	}
}

void ABlasterPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest)
{
	float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void ABlasterPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest, float TimeServerReceivedClientRequest)
{
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
	SingleTripTime = 0.5f * RoundTripTime;
	float CurrentServerTime = TimeServerReceivedClientRequest + SingleTripTime;
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

float ABlasterPlayerController::GetServerTime()
{
	if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	else return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

void ABlasterPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

// ------------------------------------------------------------
// 比赛状态：服务端检测状态发给客户端，客户端同步 Warmup/Match/Cooldown 时间
// ------------------------------------------------------------
// ------------------------------------------------------------
// 比赛状态同步：客户端 BeginPlay 时通过 RPC 向服务器请求当前比赛状态，
// 服务器从 GameMode 读取 Warmup/Match/Cooldown 时长和当前 MatchState，
// 再通过 ClientJoinMidgame RPC 发回客户端，驱动 HUD 初始化
// ------------------------------------------------------------
void ABlasterPlayerController::ServerCheckMatchState_Implementation()//客户端向服务器请求比赛状态，服务器从 GameMode 读取配置发回客户端
{
	// 从 GameMode 获取比赛配置和当前状态（GameMode 仅存在于服务器）
	ABlasterGameMode* GameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode)
	{
		WarmupTime = GameMode->WarmupTime;           // 热身阶段时长（默认10秒）
		MatchTime = GameMode->MatchTime;             // 比赛阶段时长（默认120秒）
		CooldownTime = GameMode->CooldownTime;       // 冷却阶段时长（默认10秒）
		LevelStartingTime = GameMode->LevelStartingTime; // 关卡开始的时间戳，用于计算剩余倒计时
		MatchState = GameMode->GetMatchState();      // 当前比赛状态（WaitingToStart/InProgress/Cooldown）
		// 将状态和时间打包发回客户端，客户端据此决定显示 Announcement 还是 CharacterOverlay
		ClientJoinMidgame(MatchState, WarmupTime, MatchTime, CooldownTime, LevelStartingTime);
	}
}

void ABlasterPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float Cooldown, float StartingTime)
{
	WarmupTime = Warmup;
	MatchTime = Match;
	CooldownTime = Cooldown;
	LevelStartingTime = StartingTime;
	// 只在 MatchState 尚未初始化时才设置，防止用 RPC 中的过时状态
	// 覆盖已通过属性复制到达的更新状态（竞态条件修复）
	if (MatchState == NAME_None)
	{
		MatchState = StateOfMatch;
	}
	// 根据当前实际的 MatchState 初始化 UI（而非 RPC 参数中的可能过时状态）
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud && MatchState == MatchState::WaitingToStart && BlasterHud->Announcement == nullptr)
	{
		BlasterHud->AddAnnouncement();
	}
	// 热身阶段自动打开购买菜单（仅首次，CreateBuyMenu 内部有重复创建保护）
	if (BlasterHud && MatchState == MatchState::WaitingToStart && BlasterHud->BuyMenu == nullptr)
	{
		OpenBuyMenuOnWarmup();
	}
}

void ABlasterPlayerController::OnMatchStateSet(FName State, bool bTeamsMatch)//负责初始化玩家状态
{
	MatchState = State;

	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted(bTeamsMatch);
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
	// 回合制阵营模式状态处理
	else if (MatchState == MatchState::AssignTeams)
	{
		HandleAssignTeams();
	}
	else if (MatchState == MatchState::RoundPrepare)
	{
		HandleRoundPrepare();
	}
	else if (MatchState == MatchState::RoundInProgress)
	{
		HandleRoundInProgress();
	}
	else if (MatchState == MatchState::RoundEnd)
	{
		HandleRoundEnd();
	}
	else if (MatchState == MatchState::MatchEnd)
	{
		HandleMatchEnd();
	}
}

void ABlasterPlayerController::OnRep_MatchState()//负责同步玩家状态，与OnMatchStateSet配合，一个负责初始化，一个负责后续同步
{
	if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::Cooldown)
	{
		HandleCooldown();
	}
	else if (MatchState == MatchState::WaitingToStart)
	{
		// 复制路径的 WaitingToStart：确保公告面板在热身阶段被创建
		BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
		if (BlasterHud && BlasterHud->Announcement == nullptr)
		{
			BlasterHud->AddAnnouncement();
		}
		// 热身阶段自动打开购买菜单（仅首次）
		if (BlasterHud && BlasterHud->BuyMenu == nullptr)
		{
			OpenBuyMenuOnWarmup();
		}
	}
	// 回合制阵营模式状态处理（复制路径）
	else if (MatchState == MatchState::AssignTeams)
	{
		HandleAssignTeams();
	}
	else if (MatchState == MatchState::RoundPrepare)
	{
		HandleRoundPrepare();
	}
	else if (MatchState == MatchState::RoundInProgress)
	{
		HandleRoundInProgress();
	}
	else if (MatchState == MatchState::RoundEnd)
	{
		HandleRoundEnd();
	}
	else if (MatchState == MatchState::MatchEnd)
	{
		HandleMatchEnd();
	}
}

void ABlasterPlayerController::HandleMatchHasStarted(bool bTeamsMatch)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud)
	{
		if (BlasterHud->CharacterOverlay == nullptr) BlasterHud->AddCharacterOverlay();//显示战斗ui
		if (BlasterHud->Announcement)
		{
			BlasterHud->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
		// 比赛开始，关闭购买菜单（此后 B 键不再生效，ToggleBuyMenu 检查 MatchState）
		if (bBuyMenuOpen)
		{
			HideBuyMenu();
		}
	}
}

void ABlasterPlayerController::HandleCooldown()
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud)
	{
		// 比赛结束，移除战斗 HUD（如果存在），显示公告面板
		if (BlasterHud->CharacterOverlay)
		{
			BlasterHud->CharacterOverlay->RemoveFromParent();
		}
		bool bHUDValid = BlasterHud->Announcement &&
			BlasterHud->Announcement->AnnouncementText &&
			BlasterHud->Announcement->InfoText;

		if (bHUDValid)
		{
			BlasterHud->Announcement->SetVisibility(ESlateVisibility::Visible);
			FString AnnouncementText = Announcement::NewMatchStartsIn;
			BlasterHud->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));

			// 从 GameState 读取服务器维护的 TopScoringPlayers，确保所有客户端显示一致的胜者
			ABlasterGameState* BlasterGameState = GetWorld()->GetGameState<ABlasterGameState>();
			if (BlasterGameState && BlasterGameState->TopScoringPlayers.Num() > 0)
			{
				FString InfoTextString = GetInfoText(BlasterGameState->TopScoringPlayers);
				BlasterHud->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
			}
		}
	}
}

// ------------------------------------------------------------
// 购买菜单生命周期：热身自动打开，B 键切换开关，比赛开始强制关闭
// ------------------------------------------------------------
void ABlasterPlayerController::OpenBuyMenuOnWarmup()
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud == nullptr) return;

	// 懒创建：首次热身时才实例化 widget
	if (BlasterHud->BuyMenu == nullptr)
	{
		BlasterHud->CreateBuyMenu();
	}
	ShowBuyMenu();
}

void ABlasterPlayerController::ToggleBuyMenu()
{
	// 仅热身阶段允许开关购买菜单，比赛开始后 B 键无效果
	if (MatchState != MatchState::WaitingToStart) return;

	if (bBuyMenuOpen)
	{
		HideBuyMenu();
	}
	else
	{
		ShowBuyMenu();
	}
}

void ABlasterPlayerController::ShowBuyMenu()
{
	if (bBuyMenuOpen) return;

	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud == nullptr || BlasterHud->BuyMenu == nullptr) return;

	BlasterHud->BuyMenu->AddToViewport();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);

	bBuyMenuOpen = true;
}

void ABlasterPlayerController::HideBuyMenu()
{
	if (!bBuyMenuOpen) return;

	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud && BlasterHud->BuyMenu)
	{
		BlasterHud->BuyMenu->RemoveFromParent();
	}

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);

	bBuyMenuOpen = false;
}

// ------------------------------------------------------------
// 投掷物选择面板生命周期：按住 G 显示，点击图标确认选择，松开 G 关闭（取消）
// ------------------------------------------------------------
void ABlasterPlayerController::ShowThrowablePanel()
{
	// Toggle：面板已打开则关闭（取消选择），未打开则打开
	if (bThrowablePanelOpen)
	{
		HideThrowablePanel();
		return;
	}

	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud == nullptr) return;

	// 懒创建：首次按 G 时才实例化 widget
	if (BlasterHud->ThrowableWheel == nullptr)
	{
		BlasterHud->CreateThrowableWheel();
	}
	if (BlasterHud->ThrowableWheel == nullptr) return;

	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	if (!BlasterCharacter) return;

	UThrowableComponent* ThrowableComp = BlasterCharacter->GetThrowable();
	if (!ThrowableComp) return;

	// 绑定点击委托：点击按钮 → OnThrowableTypeClicked → 选择类型 + 关闭面板
	BlasterHud->ThrowableWheel->OnTypeClicked.AddDynamic(this, &ABlasterPlayerController::OnThrowableTypeClicked);

	BlasterHud->ThrowableWheel->AddToViewport();
	BlasterHud->ThrowableWheel->Show(ThrowableComp);

	// 显示鼠标用于点击选择
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);

	bThrowablePanelOpen = true;
}

void ABlasterPlayerController::HideThrowablePanel()
{
	if (!bThrowablePanelOpen) return;

	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud == nullptr || BlasterHud->ThrowableWheel == nullptr) return;

	// 解除委托绑定，避免悬空引用
	BlasterHud->ThrowableWheel->OnTypeClicked.RemoveDynamic(this, &ABlasterPlayerController::OnThrowableTypeClicked);

	BlasterHud->ThrowableWheel->Hide();
	BlasterHud->ThrowableWheel->RemoveFromParent();

	// 恢复纯游戏输入
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	SetShowMouseCursor(false);

	bThrowablePanelOpen = false;
}

void ABlasterPlayerController::OnThrowableTypeClicked(EThrowableType Type)
{
	// 点击即确认：通知角色切换投掷物类型，然后关闭面板
	ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(GetPawn());
	if (BlasterCharacter)
	{
		BlasterCharacter->SelectThrowableType(Type);
	}

	HideThrowablePanel();
}

void ABlasterPlayerController::SetHUDThrowableCooking(bool bIsCooking, float RemainingSeconds)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay
		&& BlasterHud->CharacterOverlay->ThrowableCountdownText;

	if (bHUDValid)
	{
		if (bIsCooking && RemainingSeconds > 0.f)
		{
			FString CountdownText = FString::Printf(TEXT("%.1f"), RemainingSeconds);
			BlasterHud->CharacterOverlay->ThrowableCountdownText->SetText(FText::FromString(CountdownText));
			BlasterHud->CharacterOverlay->ThrowableCountdownText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			BlasterHud->CharacterOverlay->ThrowableCountdownText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

// ========================================================================
// 闪光弹致盲 Client RPC + 白屏淡出
// ========================================================================

void ABlasterPlayerController::ClientApplyFlashEffect_Implementation(float Duration)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (!BlasterHud || !BlasterHud->CharacterOverlay || !BlasterHud->CharacterOverlay->FlashOverlay) return;

	BlasterHud->CharacterOverlay->FlashOverlay->SetVisibility(ESlateVisibility::Visible);
	BlasterHud->CharacterOverlay->FlashOverlay->SetRenderOpacity(1.0f);

	if (FlashbangTinnitusSound)
	{
		UGameplayStatics::PlaySound2D(this, FlashbangTinnitusSound);
	}

	FlashEffectStartTime = GetWorld()->GetTimeSeconds();
	FlashEffectDuration = Duration;

	GetWorldTimerManager().SetTimer(
		FlashFadeTimer,
		this,
		&ABlasterPlayerController::TickFlashFade,
		0.05f,
		true
	);
}

void ABlasterPlayerController::TickFlashFade()
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (!BlasterHud || !BlasterHud->CharacterOverlay || !BlasterHud->CharacterOverlay->FlashOverlay) return;

	const float Elapsed = GetWorld()->GetTimeSeconds() - FlashEffectStartTime;
	const float Alpha = 1.0f - Elapsed / FlashEffectDuration;

	if (Alpha <= 0.f)
	{
		BlasterHud->CharacterOverlay->FlashOverlay->SetVisibility(ESlateVisibility::Hidden);
		GetWorldTimerManager().ClearTimer(FlashFadeTimer);
	}
	else
	{
		BlasterHud->CharacterOverlay->FlashOverlay->SetRenderOpacity(Alpha);
	}
}

FString ABlasterPlayerController::GetInfoText(const TArray<class ABlasterPlayerState*>& Players)
{
	ABlasterPlayerState* BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
	if (BlasterPlayerState == nullptr) return FString();
	FString InfoTextString;
	if (Players.Num() == 1 && Players[0] == BlasterPlayerState)
	{
		InfoTextString = TEXT("你是冠军!");
	}
	else if (Players.Num() == 1)
	{
		InfoTextString = FString::Printf(TEXT("胜者: \n%s"), *Players[0]->GetPlayerName());
	}
	else if (Players.Num() > 1)
	{
		InfoTextString = TEXT("并列胜者:\n");
		for (auto TiedPlayer : Players)
		{
			InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
		}
	}
	return InfoTextString;
}

// ------------------------------------------------------------
// 每帧驱动 HUD 倒计时：计算剩余秒数 → 变化时更新对应 UI 控件
// 服务器和客户端计算逻辑不同：
//   服务器：直接读 GameMode->GetCountdownTime()（权威数据）
//   客户端：用 GetServerTime() + 偏移公式推算出接近服务器的时间
// ------------------------------------------------------------
void ABlasterPlayerController::SetHUDTime()
{
	float TimeLeft = 0.f;

	// 回合制倒计时阶段：从 GameState 读取 RemainingCountdown（服务器/客户端均可用）
	if (MatchState == MatchState::RoundPrepare ||
	    MatchState == MatchState::RoundEnd ||
	    MatchState == MatchState::MatchEnd)
	{
		if (ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>())
		{
			TimeLeft = GS->RemainingCountdown;
		}
	}
	// 回合战斗倒计时：从 GameState 读取，推送到 MatchCountdownText
	else if (MatchState == MatchState::RoundInProgress)
	{
		if (ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>())
		{
			TimeLeft = GS->RemainingCountdown;
		}
	}
	// AssignTeams 是瞬间过渡状态，无倒计时
	else if (MatchState == MatchState::AssignTeams)
	{
	}
	else
	{
		// 原有 Deathmatch 倒计时逻辑（WaitingToStart / InProgress / Cooldown）
		// 仅当 GameMode 是 ABlasterGameMode 时生效；爆破模式下这些状态不适用
		const bool bIsBombDefusal = Cast<ABombDefusalGameMode>(UGameplayStatics::GetGameMode(this)) != nullptr;
		if (!bIsBombDefusal)
		{
			if (MatchState == MatchState::WaitingToStart)
				TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
			else if (MatchState == MatchState::InProgress)
				TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
			else if (MatchState == MatchState::Cooldown)
				TimeLeft = CooldownTime + WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;

			// 服务器端：直接使用 GameMode 里计算好的权威倒计时，保证精确
			if (HasAuthority())
			{
				if (BlasterGameMode == nullptr)
				{
					BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
					if (BlasterGameMode)
					{
						LevelStartingTime = BlasterGameMode->LevelStartingTime;
					}
				}
				BlasterGameMode = BlasterGameMode == nullptr
					? Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this))
					: BlasterGameMode;
				if (BlasterGameMode)
				{
					TimeLeft = BlasterGameMode->GetCountdownTime();
				}
			}
		}
	}

	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	// 仅在秒数变化时才更新 UI，避免每帧做无效的字符串格式化
	if (CountdownInt != SecondsLeft)
	{
		// 热身和冷却 → 更新公告面板倒计时
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown)
		{
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		// 比赛中 → 更新战斗 HUD 倒计时（Deathmatch InProgress + 爆破 RoundInProgress）
		if (MatchState == MatchState::InProgress || MatchState == MatchState::RoundInProgress)
		{
			SetHUDMatchCountdown(TimeLeft);
		}
		// 回合制倒计时：RoundPrepare / RoundEnd / MatchEnd 均显示在公告面板上
		if (MatchState == MatchState::RoundPrepare ||
		    MatchState == MatchState::RoundEnd ||
		    MatchState == MatchState::MatchEnd)
		{
			SetHUDAnnouncementCountdown(TimeLeft);
		}
	}

	CountdownInt = SecondsLeft;
}

void ABlasterPlayerController::PollInit()//推送缓存数据
{
	if (CharacterOverlay == nullptr)
	{
		if (BlasterHud && BlasterHud->CharacterOverlay)
		{
			CharacterOverlay = BlasterHud->CharacterOverlay;
			if (CharacterOverlay)
			{
				if (bInitializeHealth) SetHUDHealth(HUDHealth, HUDMaxHealth);
				if (bInitializeShield) SetHUDShield(HUDShield, HUDMaxShield);
				if (bInitializeMatchCountdown) SetHUDMatchCountdown(HUDMatchCountdown);
				if (bInitializeCarriedAmmo) SetHUDCarriedAmmo(HUDCarriedAmmo);
				if (bInitializeWeaponAmmo) SetHUDWeaponAmmo(HUDWeaponAmmo);
			}
		}
	}

	// 回合制模式下，每帧更新存活人数到 RoundOverlay（从 GameState 读取，服务器/客户端均可用）
	if (MatchState == MatchState::RoundInProgress)
	{
		if (ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>())
		{
			SetHUDAliveCount(GS->AttackerAliveCount, GS->DefenderAliveCount);
		}
	}

	// 客户端 GS 复制延迟补偿：公告阶段每帧从 GameState 刷公告文本，保证复制到达后立即显示
	if (!HasAuthority() && (MatchState == MatchState::RoundPrepare ||
		MatchState == MatchState::RoundEnd || MatchState == MatchState::MatchEnd))
	{
		RefreshAnnouncementFromGameState();
	}
}

// ========================================================================
// 回合制阵营模式：MatchState 处理器
// ========================================================================
void ABlasterPlayerController::HandleAssignTeams()
{
	ABlasterPlayerState* PS = GetPlayerState<ABlasterPlayerState>();
	if (!PS) return;

	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud && BlasterHud->Announcement == nullptr)
	{
		BlasterHud->AddAnnouncement();
	}
	if (BlasterHud && BlasterHud->Announcement)
	{
		BlasterHud->Announcement->SetVisibility(ESlateVisibility::Visible);

		const FString TeamStr = (PS->TeamID == ETeamID::ETI_Attacker)
			? TEXT("攻击者") : TEXT("保卫者");
		const FString AnnounceText = FString::Printf(TEXT("你是%s"), *TeamStr);
		BlasterHud->Announcement->AnnouncementText->SetText(FText::FromString(AnnounceText));
		BlasterHud->Announcement->InfoText->SetText(FText());
	}
}

void ABlasterPlayerController::HandleRoundPrepare()
{
	// 从 GameState 读取回合号/比分（服务器/客户端均可访问）
	ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>();
	ABlasterPlayerState* PS = GetPlayerState<ABlasterPlayerState>();
	if (!GS || !PS) return;

	const int32 RoundNum = GS->CurrentRoundNumber;
	const int32 AtkWins = GS->AttackerWins;
	const int32 DefWins = GS->DefenderWins;

	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud && BlasterHud->Announcement == nullptr)
	{
		BlasterHud->AddAnnouncement();
	}

	// 隐藏战斗 HUD 和回合面板，显示公告面板
	if (BlasterHud && BlasterHud->CharacterOverlay)
	{
		BlasterHud->CharacterOverlay->SetVisibility(ESlateVisibility::Hidden);
	}
	if (BlasterHud && BlasterHud->RoundOverlay)
	{
		BlasterHud->RoundOverlay->SetVisibility(ESlateVisibility::Hidden);
	}

	if (BlasterHud && BlasterHud->Announcement)
	{
		BlasterHud->Announcement->SetVisibility(ESlateVisibility::Visible);

		const FString TeamStr = (PS->TeamID == ETeamID::ETI_Attacker)
			? TEXT("攻击者") : TEXT("保卫者");
		const FString AnnounceText = FString::Printf(TEXT("第%d回合"), RoundNum);
		const FString InfoStr = FString::Printf(TEXT("你是%s"), *TeamStr);

		BlasterHud->Announcement->AnnouncementText->SetText(FText::FromString(AnnounceText));
		BlasterHud->Announcement->InfoText->SetText(FText::FromString(InfoStr));
	}

	// 推送回合信息到 RoundOverlay（此时隐藏中，切换到 RoundInProgress 时显示）
	SetHUDRoundInfo(RoundNum, PS->TeamID, AtkWins, DefWins);

	// 客户端：标记需要下一帧用最新 GameState 数据刷新公告（GS 复制可能滞后于 MatchState）
}

void ABlasterPlayerController::HandleRoundInProgress()
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud)
	{
		if (BlasterHud->CharacterOverlay == nullptr) BlasterHud->AddCharacterOverlay();
		if (BlasterHud->CharacterOverlay)
		{
			BlasterHud->CharacterOverlay->SetVisibility(ESlateVisibility::Visible);
		}
		// 懒创建 RoundOverlay（仅在进入战斗时第一次创建）
		if (BlasterHud->RoundOverlay == nullptr)
		{
			BlasterHud->CreateRoundOverlay();
		}
		if (BlasterHud->RoundOverlay)
		{
			BlasterHud->RoundOverlay->SetVisibility(ESlateVisibility::Visible);
			// 首回合补推送：HandleRoundPrepare 时 RoundOverlay 尚未创建，SetHUDRoundInfo 被跳过
			// 此处从 GameState 读取当前回合信息写入控件，避免显示蓝图默认文本
			if (ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>())
			{
				ABlasterPlayerState* PS = GetPlayerState<ABlasterPlayerState>();
				SetHUDRoundInfo(GS->CurrentRoundNumber,
					PS ? PS->TeamID : ETeamID::ETI_None,
					GS->AttackerWins, GS->DefenderWins);
			}
		}
		if (BlasterHud->Announcement)
		{
			BlasterHud->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void ABlasterPlayerController::HandleRoundEnd()
{
	// 从 GameState 读取回合结果（服务器/客户端均可访问）
	ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>();
	if (!GS) return;

	const ETeamID LastWinner = GS->LastRoundWinner;
	const int32 AtkWins = GS->AttackerWins;
	const int32 DefWins = GS->DefenderWins;

	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud && BlasterHud->Announcement == nullptr)
	{
		BlasterHud->AddAnnouncement();
	}

	if (BlasterHud && BlasterHud->CharacterOverlay)
	{
		BlasterHud->CharacterOverlay->SetVisibility(ESlateVisibility::Hidden);
	}
	if (BlasterHud && BlasterHud->RoundOverlay)
	{
		BlasterHud->RoundOverlay->SetVisibility(ESlateVisibility::Hidden);
	}

	if (BlasterHud && BlasterHud->Announcement)
	{
		BlasterHud->Announcement->SetVisibility(ESlateVisibility::Visible);

		FString WinnerStr = (LastWinner == ETeamID::ETI_Attacker)
			? TEXT("攻击者赢得本回合!") : TEXT("保卫者赢得本回合!");
		BlasterHud->Announcement->AnnouncementText->SetText(FText::FromString(WinnerStr));
		const FString InfoStr = FString::Printf(TEXT("攻击者 %d - %d 保卫者"), AtkWins, DefWins);
		BlasterHud->Announcement->InfoText->SetText(FText::FromString(InfoStr));
	}

	// 推送回合结果到 RoundOverlay
	SetHUDRoundResult(LastWinner, AtkWins, DefWins);

	// 客户端：标记需要下一帧用最新 GameState 数据刷新公告
}

void ABlasterPlayerController::HandleMatchEnd()
{
	// 从 GameState 读取比赛结果（服务器/客户端均可访问）
	ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>();
	if (!GS) return;

	const ETeamID MatchWinner = GS->LastMatchWinner;
	const int32 AtkWins = GS->AttackerWins;
	const int32 DefWins = GS->DefenderWins;

	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud && BlasterHud->Announcement == nullptr)
	{
		BlasterHud->AddAnnouncement();
	}

	if (BlasterHud && BlasterHud->CharacterOverlay)
	{
		BlasterHud->CharacterOverlay->SetVisibility(ESlateVisibility::Hidden);
	}
	if (BlasterHud && BlasterHud->RoundOverlay)
	{
		BlasterHud->RoundOverlay->SetVisibility(ESlateVisibility::Hidden);
	}

	FString WinnerStr = (MatchWinner == ETeamID::ETI_Attacker)
		? TEXT("攻击者赢得比赛!") : TEXT("保卫者赢得比赛!");

	if (BlasterHud && BlasterHud->Announcement)
	{
		BlasterHud->Announcement->SetVisibility(ESlateVisibility::Visible);
		BlasterHud->Announcement->AnnouncementText->SetText(FText::FromString(WinnerStr));
		const FString InfoStr = FString::Printf(TEXT("最终比分 %d - %d\n返回大厅..."), AtkWins, DefWins);
		BlasterHud->Announcement->InfoText->SetText(FText::FromString(InfoStr));
	}

	// 推送比赛结果到 RoundOverlay
	SetHUDMatchResult(MatchWinner);

	// 客户端：标记需要下一帧用最新 GameState 数据刷新公告
}

// ========================================================================
// 回合信息 HUD 推送 → RoundOverlay Widget
// ========================================================================
void ABlasterPlayerController::SetHUDRoundInfo(int32 InRoundNumber, ETeamID MyTeam,
	int32 AttackerWins, int32 DefenderWins)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (!BlasterHud || !BlasterHud->RoundOverlay) return;

	BlasterHud->RoundOverlay->RoundNumberText->SetText(
		FText::FromString(FString::Printf(TEXT("第%d回合"), InRoundNumber)));

	const FString TeamStr = (MyTeam == ETeamID::ETI_Attacker) ? TEXT("攻击者") : TEXT("保卫者");
	BlasterHud->RoundOverlay->TeamLabel->SetText(FText::FromString(TeamStr));

	BlasterHud->RoundOverlay->ScoreText->SetText(
		FText::FromString(FString::Printf(TEXT("攻击者 %d - %d 保卫者"), AttackerWins, DefenderWins)));
}

void ABlasterPlayerController::SetHUDRoundResult(ETeamID Winner, int32 AttackerWins, int32 DefenderWins)
{
	// 回合结束时更新比分显示在 RoundOverlay 上
	SetHUDRoundInfo(0, ETeamID::ETI_None, AttackerWins, DefenderWins);
	// RoundNumberText 同时显示胜者
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud && BlasterHud->RoundOverlay && BlasterHud->RoundOverlay->RoundNumberText)
	{
		const FString WinnerStr = (Winner == ETeamID::ETI_Attacker) ? TEXT("攻击者胜") : TEXT("保卫者胜");
		BlasterHud->RoundOverlay->RoundNumberText->SetText(FText::FromString(WinnerStr));
	}
}

void ABlasterPlayerController::SetHUDMatchResult(ETeamID Winner)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (BlasterHud && BlasterHud->RoundOverlay && BlasterHud->RoundOverlay->RoundNumberText)
	{
		const FString WinnerStr = (Winner == ETeamID::ETI_Attacker)
			? TEXT("攻击者赢得比赛!") : TEXT("保卫者赢得比赛!");
		BlasterHud->RoundOverlay->RoundNumberText->SetText(FText::FromString(WinnerStr));
	}
}

void ABlasterPlayerController::SetHUDAliveCount(int32 AttackersAlive, int32 DefendersAlive)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (!BlasterHud || !BlasterHud->RoundOverlay) return;

	BlasterHud->RoundOverlay->AttackerAliveText->SetText(
		FText::FromString(FString::Printf(TEXT("%d"), AttackersAlive)));
	BlasterHud->RoundOverlay->DefenderAliveText->SetText(
		FText::FromString(FString::Printf(TEXT("%d"), DefendersAlive)));
}

// ========================================================================
// 客户端 GameState 复制延迟补偿：HandleXxx 中 GS 数据可能尚未到达，
// PollInit 下一帧调用此函数用最新 GS 数据刷新公告文本
// ========================================================================
void ABlasterPlayerController::RefreshAnnouncementFromGameState()
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	if (!BlasterHud || !BlasterHud->Announcement) return;

	ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>();
	ABlasterPlayerState* PS = GetPlayerState<ABlasterPlayerState>();
	if (!GS || !PS) return;

	if (MatchState == MatchState::RoundPrepare)
	{
		const FString TeamStr = (PS->TeamID == ETeamID::ETI_Attacker)
			? TEXT("攻击者") : TEXT("保卫者");
		BlasterHud->Announcement->AnnouncementText->SetText(
			FText::FromString(FString::Printf(TEXT("第%d回合"), GS->CurrentRoundNumber)));
		BlasterHud->Announcement->InfoText->SetText(
			FText::FromString(FString::Printf(TEXT("你是%s"), *TeamStr)));
	}
	else if (MatchState == MatchState::RoundEnd)
	{
		const FString WinnerStr = (GS->LastRoundWinner == ETeamID::ETI_Attacker)
			? TEXT("攻击者赢得本回合!") : TEXT("保卫者赢得本回合!");
		BlasterHud->Announcement->AnnouncementText->SetText(FText::FromString(WinnerStr));
		BlasterHud->Announcement->InfoText->SetText(
			FText::FromString(FString::Printf(TEXT("攻击者 %d - %d 保卫者"),
				GS->AttackerWins, GS->DefenderWins)));
		SetHUDRoundResult(GS->LastRoundWinner, GS->AttackerWins, GS->DefenderWins);
	}
	else if (MatchState == MatchState::MatchEnd)
	{
		const FString WinnerStr = (GS->LastMatchWinner == ETeamID::ETI_Attacker)
			? TEXT("攻击者赢得比赛!") : TEXT("保卫者赢得比赛!");
		BlasterHud->Announcement->AnnouncementText->SetText(FText::FromString(WinnerStr));
		BlasterHud->Announcement->InfoText->SetText(
			FText::FromString(FString::Printf(TEXT("最终比分 %d - %d\n返回大厅..."),
				GS->AttackerWins, GS->DefenderWins)));
		SetHUDMatchResult(GS->LastMatchWinner);
	}
}
