#include "PlayerIdentity.h"

#include "HAL/FileManager.h"        // IFileManager::MakeDirectory
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"       // FCommandLine
#include "Misc/FileHelper.h"        // FFileHelper::LoadFileToString / SaveStringToFile
#include "Misc/Parse.h"             // FParse::Value
#include "Misc/Paths.h"             // FPaths

FString FBlasterPlayerIdentity::GetPlayerIdFile()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PlayerIdentity"), TEXT("PlayerId.txt"));
}

FString FBlasterPlayerIdentity::GetPlayerId()
{
	// ① 命令行优先：-BlasterPlayerId=<GUID>（同机多客户端测试强制区分身份）
	FString CmdId;
	if (FParse::Value(FCommandLine::Get(), TEXT("BlasterPlayerId="), CmdId) && !CmdId.IsEmpty())
	{
		return CmdId;
	}

	// ② 读本地持久文件（第二次启动起走这里）
	const FString File = GetPlayerIdFile();
	FString Stored;
	if (FFileHelper::LoadFileToString(Stored, *File))
	{
		Stored.TrimStartAndEndInline();
		if (!Stored.IsEmpty())
		{
			return Stored;
		}
	}

	// ③ 首次启动：生成 GUID 并落盘（先建目录再写文件）
	const FString NewId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	if (FFileHelper::SaveStringToFile(NewId, *File))
	{
		UE_LOG(LogTemp, Log, TEXT("[Identity] 已生成并保存持久 PlayerId=%s (%s)"), *NewId, *File);
		return NewId;
	}

	// 写盘失败兜底：本次会话用内存 GUID（跨局不再稳定，仅极少数磁盘异常场景）
	const FString Ephemeral = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UE_LOG(LogTemp, Warning, TEXT("[Identity] PlayerId 落盘失败，改用临时 %s"), *Ephemeral);
	return Ephemeral;
}
