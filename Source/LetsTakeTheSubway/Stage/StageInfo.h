// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Info.h"
#include "StageInfo.generated.h"

class ACameraActor;

/**
 * 이 레벨이 몇 번째 스테이지이고 구역이 몇 개인지 적어 두는 액터. 레벨당 하나 놓는다.
 *
 * WorldSettings 파생 클래스나 데이터 애셋 대신 액터로 둔 이유: 기존 레벨의 WorldSettings 클래스를
 * 바꾸지 않아도 되고, 그리드·퍼즐 구간처럼 "레벨에 하나 놓는다"는 팀 관례와 같다. AInfo를 상속해
 * 에디터에서만 아이콘이 보이고 월드 파티션 스트리밍에서 빠진다.
 *
 * 둘 이상 놓으면 먼저 등록된 것만 쓰고 경고한다.
 *
 * 구역 카메라: ZoneCameras에 구역 순서대로 카메라 액터를 넣으면, 플레이어가 구역을 옮길 때
 * AGridPlayerController가 그 카메라로 시점을 보간해 옮긴다. 카메라 액터의 AutoActivateForPlayer는
 * 반드시 Disabled로 둔다. 켜져 있으면 레벨 시작 때 엔진이 그 카메라를 먼저 잡아 구역 카메라와 싸운다.
 */
UCLASS(Blueprintable)
class LETSTAKETHESUBWAY_API AStageInfo : public AInfo
{
	GENERATED_BODY()

public:
	AStageInfo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 스테이지 번호. 1부터 센다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Info", meta = (ClampMin = 1))
	int32 StageIndex = 1;

	/** UI에 보여 줄 스테이지 이름. 비워 둬도 동작한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Info")
	FText StageName;

	/**
	 * 이 스테이지의 총 구역 수.
	 *
	 * 0이면 배치된 AStageZoneVolume의 가장 큰 ZoneIndex를 쓴다. 값을 적어 두면 그것이 정본이 되고,
	 * 볼륨 배치와 어긋나면 시작할 때 경고한다. 스테이지마다 명시해 두기를 권한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Info", meta = (ClampMin = 0))
	int32 ZoneCount = 0;

	// ---------------------------------------------------------------- 구역 카메라

	/**
	 * 구역마다 시점을 맡을 카메라. 배열 0번이 구역 1이다.
	 *
	 * 비어 있는 칸의 구역에 들어가면 시점을 바꾸지 않고 직전 카메라를 유지한다(경고 로그).
	 * 폰을 따라가는 디버그 카메라(ltts.PawnCamera 1)가 켜져 있으면 이 배열보다 우선한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Info|Camera")
	TArray<TObjectPtr<ACameraActor>> ZoneCameras;

	/** 구역을 옮길 때 이전 카메라에서 다음 카메라로 시점이 옮겨 가는 시간(초). 0이면 즉시 바뀐다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Info|Camera", meta = (ClampMin = 0.0, Units = "s"))
	float CameraBlendTime = 1.0f;

	/** 시점 보간 곡선. 기본은 선형(Linear). 출발과 도착을 부드럽게 하려면 EaseInOut. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Info|Camera")
	TEnumAsByte<EViewTargetBlendFunction> CameraBlendFunction = VTBlend_Linear;

	/** 구역 번호(1부터)의 카메라. 범위 밖이거나 칸이 비어 있으면 null. */
	UFUNCTION(BlueprintPure, Category = "Stage Info|Camera")
	ACameraActor* GetZoneCamera(int32 ZoneIndex) const;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
