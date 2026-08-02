#include "Blaster/BombMode/BombInteractionComponent.h"
#include "Blaster/BombMode/BombActor.h"
#include "Blaster/BombMode/BombSite.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/PlayerState/BlasterPlayerState.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/BlasterTypes/TeamTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

UBombInteractionComponent::UBombInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBombInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ABlasterCharacter>(GetOwner());
}

// ========================================================================
// Tick
// ========================================================================
void UBombInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsInteracting)
	{
		TickInteractionProgress(DeltaTime);

		// 服务器权威校验：交互被打断（死亡/距离过远/炸弹被别人交互）
		if (GetOwnerRole() == ROLE_Authority)
		{
			if (!OwnerCharacter || OwnerCharacter->IsElimmed()) { ForceCancelInteraction(); return; }
			if (InteractionTarget)
			{
				float Dist = FVector::Dist(GetOwner()->GetActorLocation(),
					InteractionTarget->GetActorLocation());
				if (Dist > MaxInteractDistance + 50.f) { ForceCancelInteraction(); return; }
			}
			ABombActor* TargetBomb = Cast<ABombActor>(InteractionTarget);
			if (!TargetBomb)
			{
				TArray<AActor*> FoundBombs;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABombActor::StaticClass(), FoundBombs);
				for (AActor* Actor : FoundBombs)
				{
					ABombActor* Bomb = Cast<ABombActor>(Actor);
					if (Bomb && Bomb->GetOwner() == OwnerCharacter) { TargetBomb = Bomb; break; }
				}
			}
			if (TargetBomb && !TargetBomb->IsInteracting())
			{
				bIsInteracting = false;
				CurrentInteraction = EBombInteractionType::EBIT_None;
				InteractionElapsed = 0.f;
				InteractionTarget = nullptr;
				if (OwnerCharacter) OwnerCharacter->bDisableGameplayInput = false;
			}
		}
		// 本地玩家：交互中持续推送进度到 HUD（Listen Server 宿主 + 远程客户端）
		if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
		{
			PushInteractUI();
		}
	}
	// 本地玩家：非交互时检测附近目标 → 推送提示文字
	else if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
	{
		DetectNearbyTarget();
		PushInteractUI();
	}
}

// ========================================================================
// 推送交互 UI 到 HUD InteractWidget
// ========================================================================
void UBombInteractionComponent::PushInteractUI()
{
	ABlasterPlayerController* PC = OwnerCharacter
		? Cast<ABlasterPlayerController>(OwnerCharacter->GetController()) : nullptr;
	if (!PC) return;

	bool bShow = false;
	FString Prompt;
	float Progress = 0.f;

	// 从 InteractionTarget 解析点位名，用于拼接到提示文字中
	auto GetSiteName = [](AActor* Target) -> FString
	{
		if (!Target) return TEXT("");
		// InteractionTarget 可能是 BombSite（安包）或 BombActor（拆包/拾取）
		if (ABombSite* Site = Cast<ABombSite>(Target))
			return Site->SiteName;
		if (ABombActor* Bomb = Cast<ABombActor>(Target))
		{
			ABombSite* PlantedSite = Bomb->GetPlantedSite();
			if (PlantedSite) return PlantedSite->SiteName;
		}
		return TEXT("");
	};

	const FString SiteName = GetSiteName(InteractionTarget);
	const FString SiteSuffix = SiteName.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" - %s点"), *SiteName);

	if (bIsInteracting)
	{
		bShow = true;
		Progress = GetInteractionProgress();
		Prompt = (CurrentInteraction == EBombInteractionType::EBIT_Defusing)
			? FString::Printf(TEXT("[Q] 拆除炸弹%s"), *SiteSuffix)
			: FString::Printf(TEXT("[Q] 安放炸弹%s"), *SiteSuffix);
	}
	else if (InteractionTarget)
	{
		ABombActor* Bomb = Cast<ABombActor>(InteractionTarget);
		if (Bomb && Bomb->GetBombState() == EBombState::EBS_Carried && Bomb->GetOwner() == nullptr)
		{
			bShow = true;
			Prompt = TEXT("[Q] 拾取炸弹");
		}
		else if (CurrentInteraction == EBombInteractionType::EBIT_Planting)
		{
			bShow = true;
			Prompt = FString::Printf(TEXT("[Q] 安放炸弹%s"), *SiteSuffix);
		}
		else if (CurrentInteraction == EBombInteractionType::EBIT_Defusing)
		{
			bShow = true;
			Prompt = FString::Printf(TEXT("[Q] 拆除炸弹%s"), *SiteSuffix);
		}
	}

	// bIsInteracting 控制进度条显隐：靠近时隐藏，按住 Q 时显示
	PC->UpdateBombInteractUI(Progress, Prompt, bShow, bIsInteracting);
}

// ========================================================================
// 客户端扫描附近目标
// ========================================================================
void UBombInteractionComponent::DetectNearbyTarget()
{
	InteractionTarget = nullptr;
	CurrentInteraction = EBombInteractionType::EBIT_None;
	if (!OwnerCharacter) return;
	ABlasterPlayerState* PS = OwnerCharacter->GetPlayerState<ABlasterPlayerState>();
	if (!PS) return;

	if (PS->TeamID == ETeamID::ETI_Attacker)
	{
		if (!IsCarryingBomb())
		{
			ABombActor* DroppedBomb = FindNearestDroppedBomb();
			if (DroppedBomb) { InteractionTarget = DroppedBomb; return; }
		}
		ABombSite* Site = FindNearestBombSite();
		if (Site && !Site->bIsBombPlantedHere)
		{
			InteractionTarget = Site;
			CurrentInteraction = EBombInteractionType::EBIT_Planting;
		}
	}
	else if (PS->TeamID == ETeamID::ETI_Defender)
	{
		ABombActor* Bomb = FindNearestPlantedBomb();
		if (Bomb) { InteractionTarget = Bomb; CurrentInteraction = EBombInteractionType::EBIT_Defusing; }
	}
}

// ========================================================================
// 辅助
// ========================================================================
bool UBombInteractionComponent::IsCarryingBomb() const
{
	TArray<AActor*> FoundBombs;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABombActor::StaticClass(), FoundBombs);
	for (AActor* Actor : FoundBombs)
	{
		ABombActor* Bomb = Cast<ABombActor>(Actor);
		if (Bomb && Bomb->GetBombState() == EBombState::EBS_Carried && Bomb->GetOwner() == OwnerCharacter)
			return true;
	}
	return false;
}

ABombSite* UBombInteractionComponent::FindNearestBombSite() const
{
	if (!GetOwner()) return nullptr;
	TArray<AActor*> FoundSites;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABombSite::StaticClass(), FoundSites);
	ABombSite* Nearest = nullptr;
	float NearestDist = MaxInteractDistance;
	FVector MyLoc = GetOwner()->GetActorLocation();
	for (AActor* Actor : FoundSites)
	{
		ABombSite* Site = Cast<ABombSite>(Actor);
		if (!Site || Site->bIsBombPlantedHere) continue;
		float Dist = FVector::Dist(MyLoc, Site->GetActorLocation());
		if (Dist < NearestDist) { NearestDist = Dist; Nearest = Site; }
	}
	return Nearest;
}

ABombActor* UBombInteractionComponent::FindNearestPlantedBomb() const
{
	if (!GetOwner()) return nullptr;
	TArray<AActor*> FoundBombs;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABombActor::StaticClass(), FoundBombs);
	ABombActor* Nearest = nullptr;
	float NearestDist = MaxInteractDistance;
	FVector MyLoc = GetOwner()->GetActorLocation();
	for (AActor* Actor : FoundBombs)
	{
		ABombActor* Bomb = Cast<ABombActor>(Actor);
		if (!Bomb || Bomb->GetBombState() != EBombState::EBS_Planted) continue;
		float Dist = FVector::Dist(MyLoc, Bomb->GetActorLocation());
		if (Dist < NearestDist) { NearestDist = Dist; Nearest = Bomb; }
	}
	return Nearest;
}

ABombActor* UBombInteractionComponent::FindNearestDroppedBomb() const
{
	if (!GetOwner()) return nullptr;
	TArray<AActor*> FoundBombs;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABombActor::StaticClass(), FoundBombs);
	ABombActor* Nearest = nullptr;
	float NearestDist = MaxInteractDistance;
	FVector MyLoc = GetOwner()->GetActorLocation();
	for (AActor* Actor : FoundBombs)
	{
		ABombActor* Bomb = Cast<ABombActor>(Actor);
		if (!Bomb || Bomb->GetBombState() != EBombState::EBS_Carried) continue;
		if (Bomb->GetOwner() != nullptr) continue;
		float Dist = FVector::Dist(MyLoc, Bomb->GetActorLocation());
		if (Dist < NearestDist) { NearestDist = Dist; Nearest = Bomb; }
	}
	return Nearest;
}

// ========================================================================
// Q 键输入
// ========================================================================
void UBombInteractionComponent::OnInteractKeyPressed()
{
	if (bIsInteracting) return;
	if (!OwnerCharacter || OwnerCharacter->IsElimmed()) return;
	ABlasterPlayerState* PS = OwnerCharacter->GetPlayerState<ABlasterPlayerState>();
	if (!PS) return;

	if (PS->TeamID == ETeamID::ETI_Attacker)
	{
		if (!IsCarryingBomb())
		{
			ABombActor* DroppedBomb = FindNearestDroppedBomb();
			if (DroppedBomb) { Server_PickupBomb(DroppedBomb); return; }
			return;
		}
		ABombSite* Site = FindNearestBombSite();
		if (Site && !Site->bIsBombPlantedHere)
		{
			bIsInteracting = true;
			CurrentInteraction = EBombInteractionType::EBIT_Planting;
			InteractionTarget = Site;
			InteractionElapsed = 0.f;
			OwnerCharacter->bDisableGameplayInput = true;
			Server_StartPlant(Site);
		}
		return;
	}

	if (PS->TeamID == ETeamID::ETI_Defender)
	{
		ABombActor* Bomb = FindNearestPlantedBomb();
		if (Bomb && !Bomb->IsInteracting())
		{
			bIsInteracting = true;
			CurrentInteraction = EBombInteractionType::EBIT_Defusing;
			InteractionTarget = Bomb;
			InteractionDuration = Bomb->DefuseDuration;
			InteractionElapsed = 0.f;
			OwnerCharacter->bDisableGameplayInput = true;
			Server_StartDefuse(Bomb);
		}
	}
}

void UBombInteractionComponent::OnInteractKeyReleased()
{
	// Hold-to-interact：松开 Q 键即取消当前交互
	if (!bIsInteracting) return;

	ForceCancelInteraction(); // 本地重置：bIsInteracting、InteractionTarget、禁用输入

	if (GetOwnerRole() != ROLE_Authority)
	{
		Server_CancelInteraction(); // RPC 通知服务器取消 BombActor 端 Timer
	}
}

// ========================================================================
// 进度
// ========================================================================
void UBombInteractionComponent::TickInteractionProgress(float DeltaTime)
{
	InteractionElapsed = FMath::Min(InteractionElapsed + DeltaTime, InteractionDuration);
}

float UBombInteractionComponent::GetInteractionProgress() const
{
	return (InteractionDuration > 0.f)
		? FMath::Clamp(InteractionElapsed / InteractionDuration, 0.f, 1.f) : 0.f;
}

bool UBombInteractionComponent::CanPlant() const
{ return CurrentInteraction == EBombInteractionType::EBIT_Planting && InteractionTarget; }
bool UBombInteractionComponent::CanDefuse() const
{ return CurrentInteraction == EBombInteractionType::EBIT_Defusing && InteractionTarget; }
bool UBombInteractionComponent::CanPickup() const
{
	ABombActor* Bomb = Cast<ABombActor>(InteractionTarget);
	return Bomb && Bomb->GetBombState() == EBombState::EBS_Carried && Bomb->GetOwner() == nullptr;
}

// ========================================================================
// 强制取消
// ========================================================================
void UBombInteractionComponent::ForceCancelInteraction()
{
	if (!bIsInteracting) return;
	bIsInteracting = false;
	CurrentInteraction = EBombInteractionType::EBIT_None;
	InteractionElapsed = 0.f;
	InteractionTarget = nullptr;
	if (OwnerCharacter) OwnerCharacter->bDisableGameplayInput = false;
	if (GetOwnerRole() == ROLE_Authority)
	{
		TArray<AActor*> FoundBombs;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABombActor::StaticClass(), FoundBombs);
		for (AActor* Actor : FoundBombs)
		{
			ABombActor* Bomb = Cast<ABombActor>(Actor);
			if (Bomb && Bomb->IsInteracting()) { Bomb->Server_CancelInteraction(); break; }
		}
	}
}

// ========================================================================
// RPC
// ========================================================================
void UBombInteractionComponent::Server_PickupBomb_Implementation(ABombActor* Bomb)
{
	if (!OwnerCharacter || !Bomb) return;
	ABlasterPlayerState* PS = OwnerCharacter->GetPlayerState<ABlasterPlayerState>();
	if (!PS || PS->TeamID != ETeamID::ETI_Attacker) return;
	if (Bomb->GetBombState() != EBombState::EBS_Carried || Bomb->GetOwner() != nullptr) return;
	float Dist = FVector::Dist(OwnerCharacter->GetActorLocation(), Bomb->GetActorLocation());
	if (Dist > MaxInteractDistance) return;
	Bomb->AssignToCarrier(OwnerCharacter);
}

void UBombInteractionComponent::Server_StartPlant_Implementation(ABombSite* Site)
{
	if (!OwnerCharacter || !Site) return;
	ABlasterPlayerState* PS = OwnerCharacter->GetPlayerState<ABlasterPlayerState>();
	if (!PS || PS->TeamID != ETeamID::ETI_Attacker) return;
	if (Site->bIsBombPlantedHere) return;
	float Dist = FVector::Dist(OwnerCharacter->GetActorLocation(), Site->GetActorLocation());
	if (Dist > MaxInteractDistance) return;
	TArray<AActor*> FoundBombs;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABombActor::StaticClass(), FoundBombs);
	ABombActor* MyBomb = nullptr;
	for (AActor* Actor : FoundBombs)
	{
		ABombActor* Bomb = Cast<ABombActor>(Actor);
		if (Bomb && Bomb->GetBombState() == EBombState::EBS_Carried && Bomb->GetOwner() == OwnerCharacter)
			{ MyBomb = Bomb; break; }
	}
	if (!MyBomb) return;
	bIsInteracting = true;
	CurrentInteraction = EBombInteractionType::EBIT_Planting;
	InteractionTarget = Site;
	InteractionDuration = MyBomb->PlantDuration;
	InteractionElapsed = 0.f;
	OwnerCharacter->bDisableGameplayInput = true;
	MyBomb->Server_StartPlant(Site);
}

void UBombInteractionComponent::Server_StartDefuse_Implementation(ABombActor* Bomb)
{
	if (!OwnerCharacter || !Bomb) return;
	ABlasterPlayerState* PS = OwnerCharacter->GetPlayerState<ABlasterPlayerState>();
	if (!PS || PS->TeamID != ETeamID::ETI_Defender) return;
	if (Bomb->GetBombState() != EBombState::EBS_Planted || Bomb->IsInteracting()) return;
	float Dist = FVector::Dist(OwnerCharacter->GetActorLocation(), Bomb->GetActorLocation());
	if (Dist > MaxInteractDistance) return;
	bIsInteracting = true;
	CurrentInteraction = EBombInteractionType::EBIT_Defusing;
	InteractionTarget = Bomb;
	InteractionDuration = Bomb->DefuseDuration;
	InteractionElapsed = 0.f;
	OwnerCharacter->bDisableGameplayInput = true;
	Bomb->Server_StartDefuse();
}

void UBombInteractionComponent::Server_CancelInteraction_Implementation()
{
	ForceCancelInteraction();
}
