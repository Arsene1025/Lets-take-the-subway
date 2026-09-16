// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stage/StageInfo.h"

#include "Stage/StageSubsystem.h"

#include "Camera/CameraActor.h"

AStageInfo::AStageInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// AInfo가 에디터 전용 아이콘, 틱 끔, 스트리밍 제외(bIsSpatiallyLoaded = false)를 이미 해 둔다.

#if WITH_EDITORONLY_DATA
	// 뷰포트에서 끌리지 않게 잠근다(Lock Actor Movement). 아트가 역 구조물을 박스 선택으로
	// 옮길 때 StageInfo가 같이 끌려가 좌표가 어긋난 사고가 있었다(2026-09-14 노선 연장).
	// 옮겨야 하면 액터 우클릭 > Transform > Lock Actor Movement를 끄거나 디테일 패널에 값을 넣는다.
	bLockLocation = true;
#endif
}

ACameraActor* AStageInfo::GetZoneCamera(int32 ZoneIndex) const
{
	const int32 ArrayIndex = ZoneIndex - 1;
	return ZoneCameras.IsValidIndex(ArrayIndex) ? ZoneCameras[ArrayIndex].Get() : nullptr;
}

int32 AStageInfo::GetPathUIIndex(int32 ZoneIndex) const
{
	const int32 ArrayIndex = ZoneIndex - 1;
	return ZonePathUIIndices.IsValidIndex(ArrayIndex) ? ZonePathUIIndices[ArrayIndex] : INDEX_NONE;
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
