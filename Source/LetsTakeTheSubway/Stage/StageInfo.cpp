// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stage/StageInfo.h"

#include "Stage/StageSubsystem.h"

AStageInfo::AStageInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// AInfo가 에디터 전용 아이콘, 틱 끔, 스트리밍 제외(bIsSpatiallyLoaded = false)를 이미 해 둔다.
}

void AStageInfo::BeginPlay()
{
	Super::BeginPlay();

	if (UStageSubsystem* Subsystem = UStageSubsystem::Get(this))
	{
		Subsystem->RegisterInfo(this);
	}
}

void AStageInfo::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UStageSubsystem* Subsystem = UStageSubsystem::Get(this))
	{
		Subsystem->UnregisterInfo(this);
	}

	Super::EndPlay(EndPlayReason);
}
