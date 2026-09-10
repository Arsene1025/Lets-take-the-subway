// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Puzzle/PuzzleBlock.h"
#include "PuzzleRotatingObstacle.generated.h"

class APuzzleLever;

/**
 * 가운데에 홈(채널)이 파인 커다란 사각 구조물로, 레버로 한 번에 90도씩 돌린다.
 *
 * 이 피스의 핵심은 홈이다. 홈 셀은 점유하지 않고 비워 두므로 일반 블록을 안으로 밀어 넣을 수
 * 있고 폰도 걸어 들어갈 수 있다. 홈의 안쪽 면 중 하나에 맞닿아 있는 것은 구조물이 돌 때
 * 함께 실려 돈다. 이것이 디자인의 빗금 친 면이다. 블록이 함께 이동하는지는 벽과의 접촉으로
 * 결정되지, 단순히 열린 공간 어딘가에 있다는 것만으로 결정되지 않는다.
 *
 * 슬라이드는 전혀 하지 않는다. 풋프린트 배치, 점유 관리, 회전 애니메이션을 위해
 * APuzzleBlock을 상속하며 MoveAxis는 Immovable로 고정된다.
 *
 * 풋프린트의 두 변은 홀짝이 같아야 한다(6x4 또는 7x3, 6x3은 안 됨). 두 변의 홀짝이 다른
 * 사각형은 중심이 그리드에서 반 셀 어긋나 있어, 그 중심을 축으로 돌린 블록은 반 셀 어긋난
 * 자리에 놓이게 된다. 그 시점부터는 밀 수도, 셀을 점유할 수도, 돌아서 지나갈 수도 없다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleRotatingObstacle : public APuzzleBlock
{
	GENERATED_BODY()

public:
	APuzzleRotatingObstacle();

	// ---------------------------------------------------------------- 저작

	/** 풋프린트 안에서 홈이 시작하는 낮은 쪽 모서리. 구조물 자체 좌표계 기준이다. */
	UPROPERTY(EditAnywhere, Category = "Rotating Obstacle", meta = (ClampMin = 0))
	FIntPoint ChannelOffset = FIntPoint(1, 1);

	/** 홈 크기(셀 단위). 풋프린트 가장자리에 닿으면 그쪽 끝이 열린다. */
	UPROPERTY(EditAnywhere, Category = "Rotating Obstacle", meta = (ClampMin = 1))
	FIntPoint ChannelSize = FIntPoint(5, 2);

	/** 90도 회전 한 번에 걸리는 시간(초). 덩치가 크므로 일반 블록보다 느리다. */
	UPROPERTY(EditAnywhere, Category = "Rotating Obstacle", meta = (ClampMin = 0.05))
	float RotateDuration = 0.6f;

	/** 홈 안쪽 빗금 면의 높이. 본체 높이에 대한 비율이다. */
	UPROPERTY(EditAnywhere, Category = "Rotating Obstacle", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float InnerFaceHeightRatio = 0.7f;

	// ---------------------------------------------------------------- 조회

	/** 지금 그리드 위에 놓인 상태의 홈 사각 영역. */
	FGridRect GetWorldChannelRect() const;

	/**
	 * 구조물이 그 안에서 도는 정사각 영역.
	 *
	 * 변의 길이 N은 풋프린트의 긴 변이며 풋프린트와 중심을 공유한다. 홀짝 규칙 덕분에 이
	 * 정사각형은 피스가 어느 방향을 보고 있든 셀 경계에 맞게 놓인다.
	 */
	FGridRect GetRegion() const;

	/** 회전축의 월드 위치. 바닥 높이 기준이다. */
	FVector GetPivotWorld() const;

	bool IsChannelCell(FIntPoint Cell) const { return GetWorldChannelRect().Contains(Cell); }

	/** 벽 셀(꽉 찬 셀)인지 확인한다: 풋프린트 안에 있으면서 홈 안은 아닌 셀. */
	bool IsSolidCell(FIntPoint Cell) const;

	/**
	 * 블록이 홈 안에 있고 셀 하나 이상이 안쪽 면에 맞닿아 있으면 true.
	 *
	 * 넓은 홈 한가운데에 떠 있는 블록은 붙어 있는 것이 아니므로 함께 이동하지 않는다. 그리고
	 * 그 자리에서 길을 막고 있는 셈이므로 오히려 회전을 막는다.
	 */
	virtual bool IsAttachedToInnerFace(const APuzzleBlock& Block) const;

	/** 회전 시 함께 실려 도는 모든 블록(동승 블록). 엘리베이터는 절대 포함되지 않는다. */
	void GatherRiders(TArray<APuzzleBlock*>& OutRiders) const;

	// ---------------------------------------------------------------- 회전

	/**
	 * 90도 회전하거나, 왜 돌 수 없는지 이유를 알려준다.
	 *
	 * TurnSign +1은 yaw +90도이며 위에서 보면 시계 방향이다. 함께 이동하지 않는 무언가가
	 * 구조물이 쓸고 지나가는 영역에 서 있을 때, 목적지가 바닥이 아닐 때, 폰이 길을 막고
	 * 있는데 밀려날 곳이 없을 때 거부한다.
	 */
	bool TryRotate(int32 TurnSign, FText* OutReason = nullptr);

	// ---------------------------------------------------------------- 생명주기

	virtual void GatherOccupiedCells(TArray<FIntPoint>& OutCells) const override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

protected:
	virtual void RefreshVisual() override;
	virtual void RegisterWithSubsystem(UPuzzleSubsystem& Subsystem) override;
	virtual void UnregisterFromSubsystem(UPuzzleSubsystem& Subsystem) override;

	/** 홀짝 규칙을 강제하고 홈이 풋프린트 안에 머물게 한다(저작 값 정규화). */
	virtual void NormaliseAuthoring();

	/**
	 * 본체 자신의 셀도 회전 경로 검사에 넣을지.
	 *
	 * 사각 본체는 모서리가 이웃 셀을 스치므로 넣어야 한다. 제자리에서 도는 원기둥처럼 본체가
	 * 이웃 셀을 전혀 침범하지 않는 파생 클래스는 false를 돌려 동승 블록만 검사하게 한다.
	 */
	virtual bool SweepsOwnCells() const { return true; }

	/** 본체. 홈 둘레에 최대 네 장의 슬랩으로 그린다. */
	UPROPERTY(VisibleAnywhere, Category = "Rotating Obstacle")
	TArray<TObjectPtr<UStaticMeshComponent>> SlabMeshes;

	/** 홈 안쪽 벽에 붙는 얇은 패널: 디자인의 빗금 친 면. */
	UPROPERTY(VisibleAnywhere, Category = "Rotating Obstacle")
	TArray<TObjectPtr<UStaticMeshComponent>> InnerFaceMeshes;

private:


	/**
	 * 움직이는 부분이 돌면서 지나가는 셀들(회전 경로 셀).
	 *
	 * 움직이는 각 셀을 회전축 기준의 반지름 띠와 각도 부채꼴로 환원하면, 스윕은 그 부채꼴을
	 * 90도만큼 넓히는 것에 불과하다. 후보 셀을 그 두 범위와 비교하는 것만으로도, 디자이너가
	 * 남겨야 하는 여유 공간을 모서리에 실제로 필요한 만큼으로 줄일 수 있을 정도로 정확하다.
	 * 단순한 외접원으로는 그렇게 할 수 없다.
	 */
	void GatherSweptCells(const TArray<FIntPoint>& MovingCells, int32 TurnSign, TSet<FIntPoint>& OutCells) const;

	/** 회전이 건드리지 않는 셀 중 폰을 밀어낼 수 있는 가장 가까운 피난 셀. */
	bool FindPawnRefuge(FIntPoint From, const TSet<FIntPoint>& Swept, const TSet<FIntPoint>& Destinations, FIntPoint& OutCell) const;
};
