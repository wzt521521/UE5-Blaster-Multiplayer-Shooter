// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#define ECC_SkeletalMesh ECollisionChannel::ECC_GameTraceChannel1//自定义一个碰撞通道，专门用来检测角色骨骼网格体的碰撞
class FBlasterModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
