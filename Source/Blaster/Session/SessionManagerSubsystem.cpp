#include "SessionManagerSubsystem.h"

#include "GameFramework/PlayerController.h"   // GetPlayerState 模板所在（AController）
#include "GameFramework/PlayerState.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Engine/Engine.h"                    // GEngine（Get() 用）
#include "HAL/FileManager.h"                  // IFileManager::MakeDirectory
#include "HAL/IConsoleManager.h"              // FAutoConsoleCommand
#include "Misc/FileHelper.h"                  // FFileHelper::LoadFileToString / SaveStringToFile
#include "Misc/Paths.h"                       // FPaths

void UBlasterSessionManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 服务端控制台命令：打印待重连表（P0 为空；P3 填充后用于排查断线留场状态）。
	// 静态对象常驻进程，lambda 捕获 this（子系统存活于整个引擎生命周期）。
	static FAutoConsoleCommand DumpCmd(
		TEXT("BlasterDumpPendingSessions"),
		TEXT("打印待重连表（token → 断线留场状态）"),
		FConsoleCommandDelegate::CreateLambda([this]()
		{
			UE_LOG(LogTemp, Log, TEXT("[Session] PendingSessions.Num=%d"), PendingSessions.Num());
			for (const TPair<FString, FPendingSession>& Pair : PendingSessions)
			{
				UE_LOG(LogTemp, Log,
					TEXT("[Session]   token=%s | PS=%s | Team=%d | LT=%d | Money=%d | bInMatch=%d"),
					*Pair.Key,
					*GetNameSafe(Pair.Value.PlayerState.Get()),   // TObjectPtr 需 .Get() 显式取裸指针，否则 GetNameSafe 模板推导歧义
					(int32)Pair.Value.TeamID,
					(int32)Pair.Value.LogicalTeam,
					Pair.Value.Money,
					Pair.Value.bInMatch);
			}
		})
	);

	UE_LOG(LogTemp, Log, TEXT("[Session] UBlasterSessionManager 初始化完成（引擎子系统，跨 ServerTravel 存活）"));
}

UBlasterSessionManager* UBlasterSessionManager::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UBlasterSessionManager>() : nullptr;
}

FString UBlasterSessionManager::IssueToken(APlayerController* PC)
{
	// 签发逻辑（服务器执行）：
	// 幂等 —— PS 已有 token 则原样返回不覆盖（见 P0 计划 2.5）。原因：重连时旧 token
	// 是待重连表的 key、也是客户端认证出示的凭证，若被新 token 覆盖，下次断线 Logout
	// 用新 token 做 key 与表里旧 key 对不上 → 状态错乱。
	// 无 token 才生成 FGuid 写入 PS->SessionToken（服务器权威、不复制）。
	ABlasterPlayerState* PS = PC ? PC->GetPlayerState<ABlasterPlayerState>() : nullptr;
	if (PS && !PS->GetSessionToken().IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[Session] IssueToken → 已有 token，幂等返回（不覆盖）| PC=%s | token=%s"),
			*GetNameSafe(PC), *PS->GetSessionToken());
		return PS->GetSessionToken();
	}

	const FString Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (PS)
	{
		PS->SetSessionToken(Token);
	}
	UE_LOG(LogTemp, Log, TEXT("[Session] IssueToken → 签发新 token=%s | PC=%s | PS=%s"),
		*Token, *GetNameSafe(PC), *GetNameSafe(PS));
	return Token;
}

FPendingSession* UBlasterSessionManager::FindPendingSession(const FString& Token)
{
	// 查待重连表（服务器执行）：ServerAuthenticateSession 用。
	// P0 表恒为空 → 永远返回 nullptr（新玩家分支）；P3 Logout 注册后才可能命中。
	return PendingSessions.Find(Token);
}

void UBlasterSessionManager::RegisterPendingSession(const FString& Token, FPendingSession&& Session)
{
	// 注册断线留场（服务器执行）：Logout 时把 PS/Pawn/身份数据存进表。
	// TObjectPtr 强引用防止断线玩家失去 Controller 后被 GC（P6 风险 3）。
	PendingSessions.Add(Token, MoveTemp(Session));
}

void UBlasterSessionManager::RemovePendingSession(const FString& Token)
{
	// 重连消费 / 清理：命中恢复或对局结束（P4）时移除，避免脏数据。
	PendingSessions.Remove(Token);
}

void UBlasterSessionManager::ClearPendingSessions()
{
	// P4：ReturnToLobby 对局结束清理全部待重连条目。
	PendingSessions.Reset();
}

bool UBlasterSessionManager::SaveLocalToken(const FString& Token)
{
	if (Token.IsEmpty()) return false;

	// 客户端落盘（仅客户端执行）：重连时 LoadLocalToken 读取并出示。
	// 路径：Saved/Session/SessionToken.txt —— 同机多客户端用 -saveddir 分离各自 Saved 树
	// （否则两个客户端共享 SavedDir，token 文件互相覆盖，无法验证"各不相同"）。
	const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Session"), TEXT("SessionToken.txt"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	if (FFileHelper::SaveStringToFile(Token, *File))
	{
		UE_LOG(LogTemp, Log, TEXT("[Session] 客户端已保存 token=%s → %s"), *Token, *File);
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("[Session] 客户端 token 落盘失败: %s"), *File);
	return false;
}

FString UBlasterSessionManager::LoadLocalToken()
{
	// 客户端读取（重连出示用）：读不到返回空串（首次启动），服务器对空 token 直接忽略。
	const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Session"), TEXT("SessionToken.txt"));
	FString Stored;
	if (FFileHelper::LoadFileToString(Stored, *File))
	{
		Stored.TrimStartAndEndInline();
		if (!Stored.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("[Session] 客户端读取本地 token=%s"), *Stored);
			return Stored;
		}
	}
	return FString();
}
