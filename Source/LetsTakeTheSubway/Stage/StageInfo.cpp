// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stage/StageInfo.h"

#include "Stage/StageSubsystem.h"

#include "Camera/CameraActor.h"

AStageInfo::AStageInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// AInfo가 에디터 전용 아이콘, 틱 끔, 스트리밍 제외(bIsSpatiallyLoaded = false)를 이미 해 둔다.
}

ACameraActor* AStageInfo::GetZoneCamera(int32 ZoneIndex) const
{
	const int32 ArrayIndex = ZoneIndex - 1;
	return ZoneCameras.IsValidIndex(ArrayIndex) ? ZoneCameras[ArrayIndex].Get() : nullptr;
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
