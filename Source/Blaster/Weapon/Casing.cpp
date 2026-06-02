// Fill out your copyright notice in the Description page of Project Settings.


#include "Casing.h"
#include "Components/StaticMeshComponent.h"

ACasing::ACasing()
{
	PrimaryActorTick.bCanEverTick = true;
	CasingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CasingMesh"));
	SetRootComponent(CasingMesh);
}
void ACasing::BeginPlay()
{
	Super::BeginPlay();
	
}


void ACasing::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

