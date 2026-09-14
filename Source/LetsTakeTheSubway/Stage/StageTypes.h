// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "StageTypes.generated.h"

/**
 * UI가 읽는 스테이지·구역 상태 한 벌.
 *
 * 값을 하나씩 따로 방송하지 않고 스냅샷으로 묶는 이유는, UI가 "스테이지 1 · 구역 2 / 5"를 그릴 때
 * 세 값이 항상 같은 순간의 것이어야 하기 때문이다. 필드가 하나라도 바뀌면
 * UStageSubsystem::OnStageStateChanged가 이 구조체 전체를 보낸다.
 *
 * 번호는 전부 1부터 센다. 0은 "아직 없음"이다.
 */
USTRUCT(BlueprintType)
struct LETSTAKETHESUBWAY_API FStageState
{
	GENERATED_BODY()

	/** 현재 스테이지 번호(1부터). 레벨에 AStageInfo가 없으면 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Stage")
	int32 StageIndex = 0;

	/** AStageInfo에 적은 스테이지 표시 이름. */
	UPROPERTY(BlueprintReadOnly, Category = "Stage")
	FText StageName;

	/** 이 스테이지의 총 구역 수. AStageInfo::ZoneCount가 정본이고, 0이면 배치된 볼륨에서 유도한다. */
	UPROPERTY(BlueprintReadOnly, Category = "Stage")
	int32 ZoneCount = 0;

	/** 플레이어가 지금 있는 구역 번호(1부터). 아직 어느 구역에도 들어간 적이 없으면 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Stage")
	int32 CurrentZoneIndex = 0;

	/** 현재 구역 볼륨에 적은 표시 이름. */
	UPROPERTY(BlueprintReadOnly, Category = "Stage")
	FText CurrentZoneName;

	/**
	 * 레벨 시작 판정을 마쳤는지.
	 *
	 * UI 서브시스템의 PlayerControllerChanged는 액터 BeginPlay보다 먼저 불리므로, 그 시점에 읽은
	 * 상태는 비어 있을 수 있다. false인 동안은 첫 OnStageStateChanged를 기다린다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Stage")
	bool bResolved = false;

	bool IsSameAs(const FStageState& Other) const
	{
		return StageIndex == Other.StageIndex
			&& ZoneCount == Other.ZoneCount
			&& CurrentZoneIndex == Other.CurrentZoneIndex
			&& bResolved == Other.bResolved
			&& StageName.EqualTo(Other.StageName)
			&& CurrentZoneName.EqualTo(Other.CurrentZoneName);
	}
};

/** 플레이어가 다른 구역으로 넘어갔다. 레벨 시작 판정 때는 PrevZoneIndex 0으로 한 번 온다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageZoneChanged, int32, PrevZoneIndex, int32, NewZoneIndex);

/** FStageState의 필드가 하나라도 바뀌었다. 레벨 시작 판정 때 반드시 한 번 온다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStageStateChanged, const FStageState&, State);

namespace LTTSStage
{
	/**
	 * 구역 볼륨 전용 오브젝트 채널. Config/DefaultEngine.ini에서 "StageZone"으로 이름 붙였다.
	 *
	 * 볼륨을 WorldStatic이나 WorldDynamic으로 두지 않는 이유: 폰이 그 타입에 Overlap으로 응답해야
	 * 하는데, 그러면 역 안의 아트 메시(기본 BlockAll, 오버랩 이벤트 켜짐) 전부와 오버랩이 난다.
	 * 전용 채널은 기본 응답이 Ignore라 볼륨과 폰의 프로브만 서로를 본다.
	 *
	 * 채널 번호를 바꾸려면 ini와 이 줄을 함께 바꾼다. 다른 곳에서 ECC_GameTraceChannel1을 직접 쓰지 않는다.
	 */
	inline constexpr ECollisionChannel ZoneObjectChannel = ECC_GameTraceChannel1;
}
