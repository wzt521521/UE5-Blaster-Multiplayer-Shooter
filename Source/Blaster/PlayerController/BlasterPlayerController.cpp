// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterPlayerController.h"
#include "Blaster/HUD/BlasterHud.h"
#include "Blaster/HUD/Characteroverlay.h"
#include "Blaster/HUD/Announcement.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/BlasterTypes/Announcement.h"
#include "Kismet/GameplayStatics.h"

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

void ABlasterPlayerController::SetHUDScore(float Score)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay && BlasterHud->CharacterOverlay->ScoreAmount;
	if (bHUDValid)
	{
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		BlasterHud->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	}
	else
	{
		bInitializeScore = true;
		HUDScore = Score;
	}
}

void ABlasterPlayerController::SetHUDDefeats(int32 Defeats)
{
	BlasterHud = BlasterHud == nullptr ? Cast<ABlasterHud>(GetHUD()) : BlasterHud;
	bool bHUDValid = BlasterHud && BlasterHud->CharacterOverlay && BlasterHud->CharacterOverlay->DefeatsAmount;
	if (bHUDValid)
	{
		FString DefeatsText = FString::Printf(TEXT("%d"), Defeats);
		BlasterHud->CharacterOverlay->DefeatsAmount->SetText(FText::FromString(DefeatsText));
	}
	else
	{
		bInitializeDefeats = true;
		HUDDefeats = Defeats;
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

	// 各阶段剩余时间 = 本阶段截止时间点 - 当前服务器时间
	// 截止时间 = 之前所有阶段时长之和 + 本阶段时长 + LevelStartingTime
	if (MatchState == MatchState::WaitingToStart)
		TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::InProgress)
		TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::Cooldown)
		TimeLeft = CooldownTime + WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;

	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	// 服务器端：直接使用 GameMode 里计算好的权威倒计时，保证精确
	if (HasAuthority())
	{
		if (BlasterGameMode == nullptr)
		{
			BlasterGameMode = Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this));
			LevelStartingTime = BlasterGameMode->LevelStartingTime;
		}
		BlasterGameMode = BlasterGameMode == nullptr
			? Cast<ABlasterGameMode>(UGameplayStatics::GetGameMode(this))
			: BlasterGameMode;
		if (BlasterGameMode)
		{
			SecondsLeft = FMath::CeilToInt(BlasterGameMode->GetCountdownTime() + LevelStartingTime);
		}
	}

	// 仅在秒数变化时才更新 UI，避免每帧做无效的字符串格式化
	if (CountdownInt != SecondsLeft)
	{
		// 热身和冷却 → 更新公告面板倒计时（WarmupTime 文本控件）
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown)
		{
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		// 比赛中 → 更新战斗 HUD 倒计时（MatchCountdownText 文本控件）
		if (MatchState == MatchState::InProgress)
		{
			SetHUDMatchCountdown(TimeLeft);
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
				if (bInitializeScore) SetHUDScore(HUDScore);
					if (bInitializeShield) SetHUDShield(HUDShield, HUDMaxShield);
				if (bInitializeDefeats) SetHUDDefeats(HUDDefeats);
				if (bInitializeMatchCountdown) SetHUDMatchCountdown(HUDMatchCountdown);
				if (bInitializeCarriedAmmo) SetHUDCarriedAmmo(HUDCarriedAmmo);
			}
		}
	}
}
