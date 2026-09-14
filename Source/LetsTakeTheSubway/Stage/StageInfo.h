// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "StageInfo.generated.h"

/**
 * 이 레벨이 몇 번째 스테이지이고 구역이 몇 개인지 적어 두는 액터. 레벨당 하나 놓는다.
 *
 * WorldSettings 파생 클래스나 데이터 애셋 대신 액터로 둔 이유: 기존 레벨의 WorldSettings 클래스를
 * 바꾸지 않아도 되고, 그리드·퍼즐 구간처럼 "레벨에 하나 놓는다"는 팀 관례와 같다. AInfo를 상속해
 * 에디터에서만 아이콘이 보이고 월드 파티션 스트리밍에서 빠진다.
 *
 * 둘 이상 놓으면 먼저 등록된 것만 쓰고 경고한다.
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

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
