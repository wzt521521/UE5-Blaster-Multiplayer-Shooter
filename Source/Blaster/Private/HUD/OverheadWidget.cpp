// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/OverheadWidget.h"
#include "Components/TextBlock.h"

void UOverheadWidget::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	RemoveFromParent();
	Super::OnLevelRemovedFromWorld(InLevel, InWorld);
}

void UOverheadWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn)
{
	if (InPawn == nullptr) return;
	//如果把localrole换成remoterole，两者有什么区别吗 
	ENetRole LocalRole = InPawn->GetLocalRole();
	FString RoleString;
	switch (LocalRole)
	{
	case ENetRole::ROLE_Authority:
		RoleString = FString("Authority");
		break;
	case ENetRole::ROLE_AutonomousProxy:
		RoleString = FString("AutonomousProxy");
		break;
	case ENetRole::ROLE_SimulatedProxy:
		RoleString = FString("SimulatedProxy");
		break;
	case ENetRole::ROLE_None:
		RoleString = FString("None");
		break;
	}

	FString DisplayString = FString::Printf(TEXT("Local Role: %s"), *RoleString);
	SetDisplayText(RoleString);
}