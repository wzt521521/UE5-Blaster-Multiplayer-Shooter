#include "Announcement.h"
#include "Blaster/GameState/BlasterGameState.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Components/TextBlock.h"

// ------------------------------------------------------------
// NativeConstruct：尝试绑定委托到 GameState。
// 客户端 GameState 复制可能晚于 Widget 创建（BeginPlay → InitializeHUD），
// 若 GS 尚未就绪则下一帧重试，确保委托一定被绑定
// ------------------------------------------------------------
void UAnnouncement::NativeConstruct()
{
	Super::NativeConstruct();
	TryBindAndRefresh();
}

void UAnnouncement::NativeDestruct()
{
	// 清除可能还在等待的重试定时器
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RetryTimerHandle);
	}

	// 仅在委托已绑定时才解绑（GS 可能从未就绪）
	if (bDelegatesBound)
	{
		if (ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>())
		{
			GS->OnRoundInfoChanged.RemoveDynamic(this, &UAnnouncement::RefreshRoundInfo);
			GS->OnRoundResultChanged.RemoveDynamic(this, &UAnnouncement::RefreshRoundResult);
			GS->OnMatchResultChanged.RemoveDynamic(this, &UAnnouncement::RefreshMatchResult);
		}
	}

	Super::NativeDestruct();
}

// ------------------------------------------------------------
// 延迟绑定 + 初始刷新：
// 客户端 BeginPlay 时 GameState 代理可能尚未复制完成，
// 若 GS 为空则 SetTimerForNextTick 重试，直到成功绑定为止
// ------------------------------------------------------------
void UAnnouncement::TryBindAndRefresh()
{
	if (bDelegatesBound) return;

	ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>();
	if (!GS)
	{
		// GameState 尚未就绪 → 下一帧重试
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &UAnnouncement::TryBindAndRefresh));
		}
		return;
	}

	// 绑定三个委托：回合信息、回合结果、比赛结果
	GS->OnRoundInfoChanged.AddDynamic(this, &UAnnouncement::RefreshRoundInfo);
	GS->OnRoundResultChanged.AddDynamic(this, &UAnnouncement::RefreshRoundResult);
	GS->OnMatchResultChanged.AddDynamic(this, &UAnnouncement::RefreshMatchResult);

	bDelegatesBound = true;

	// 初始刷新：读取当前 GameState 数据填充控件
	// ETI_None 守卫在 RefreshRoundResult / RefreshMatchResult 内部处理
	RefreshRoundInfo(GS->CurrentRoundNumber, GS->TeamARoundWins, GS->TeamBRoundWins);
	RefreshRoundResult(GS->LastRoundWinner, GS->TeamARoundWins, GS->TeamBRoundWins);
	RefreshMatchResult(GS->LastMatchWinner, GS->TeamARoundWins, GS->TeamBRoundWins);
}

// ------------------------------------------------------------
// RefreshRoundInfo：回合准备阶段 → "第X回合" + "你是攻击者/保卫者"
// ------------------------------------------------------------
void UAnnouncement::RefreshRoundInfo(int32 RoundNumber, int32 TeamAWins, int32 TeamBWins)
{
	// RoundNumber ≤ 0 说明 GameState 数据尚未同步（初始默认值），
	// 此时不更新 AnnouncementText，避免显示"第0回合"——等委托推送真实值
	if (AnnouncementText && RoundNumber > 0)
	{
		AnnouncementText->SetText(
			FText::FromString(FString::Printf(TEXT("第%d回合"), RoundNumber)));
	}
	if (InfoText)
	{
		// 从本地 PlayerState 获取阵营信息（已由服务器复制到客户端）
		ABlasterPlayerState* PS = GetOwningPlayer()->GetPlayerState<ABlasterPlayerState>();

		// 客户端补救：PC 上的 PlayerState 指针可能尚未复制完成，
		// 此时从 GameState::PlayerArray 中查找归属关系匹配的条目
		if (!PS)
		{
			if (ABlasterGameState* GS = GetWorld()->GetGameState<ABlasterGameState>())
			{
				for (APlayerState* ArrayPS : GS->PlayerArray)
				{
					if (ArrayPS->GetOwningController() == GetOwningPlayer())
					{
						PS = Cast<ABlasterPlayerState>(ArrayPS);
						break;
					}
				}
			}
		}

		// TeamID 为 ETI_None 说明阵营尚未分配或数据尚未同步，
		// 此时不更新 InfoText，等委托推送真实值
		if (PS && PS->TeamID != ETeamID::ETI_None)
		{
			const FString TeamStr = (PS->TeamID == ETeamID::ETI_Attacker)
				? TEXT("攻击者") : TEXT("保卫者");
			InfoText->SetText(FText::FromString(FString::Printf(TEXT("你是%s"), *TeamStr)));
		}
	}
}

// ------------------------------------------------------------
// RefreshRoundResult：回合结束 → "X赢得本回合!" + 比分
// ------------------------------------------------------------
void UAnnouncement::RefreshRoundResult(ETeamID Winner, int32 TeamAWins, int32 TeamBWins)
{
	// ETI_None 守卫：尚未有任何回合结束时跳过，避免 else 分支误显示"保卫者赢得本回合!"
	if (Winner == ETeamID::ETI_None) return;

	if (AnnouncementText)
	{
		const FString WinnerStr = (Winner == ETeamID::ETI_Attacker)
			? TEXT("攻击者赢得本回合!") : TEXT("保卫者赢得本回合!");
		AnnouncementText->SetText(FText::FromString(WinnerStr));
	}
	if (InfoText)
	{
		InfoText->SetText(
			FText::FromString(FString::Printf(TEXT("攻击者 %d - %d 保卫者"), TeamAWins, TeamBWins)));
	}
}

// ------------------------------------------------------------
// RefreshMatchResult：比赛结束 → "X赢得比赛!" + 最终比分
// ------------------------------------------------------------
void UAnnouncement::RefreshMatchResult(ETeamID Winner, int32 TeamAWins, int32 TeamBWins)
{
	// ETI_None 守卫：尚未有比赛结束时跳过，避免 else 分支误显示"保卫者赢得比赛!"
	if (Winner == ETeamID::ETI_None) return;

	if (AnnouncementText)
	{
		const FString WinnerStr = (Winner == ETeamID::ETI_Attacker)
			? TEXT("攻击者赢得比赛!") : TEXT("保卫者赢得比赛!");
		AnnouncementText->SetText(FText::FromString(WinnerStr));
	}
	if (InfoText)
	{
		InfoText->SetText(
			FText::FromString(FString::Printf(TEXT("最终比分 TeamA %d - %d TeamB\n返回大厅..."), TeamAWins, TeamBWins)));
	}
}
