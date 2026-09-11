// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridTypes.h"
#include "GridCellMarker.generated.h"

class AGridActor;
class UBoxComponent;
class UGridCellRule;

/**
 * 자동 생성된 그리드 위에 얹는 에디터 전용 오버라이드.
 *
 * 레벨에 하나 놓고 원하는 위치로 드래그한 뒤(가장 가까운 셀에 스냅된다) 셀 타입을
 * 고른다. 마커가 움직이거나 바뀌는 즉시 그리드가 오버라이드를 다시 굽기 때문에
 * 뷰포트 미리보기가 항상 실제와 일치한다.
 *
 * 마커는 쿡 시 제거된다. 남는 것은 그리드 액터에 구운 FGridCellOverride 배열과
 * 룰 오브젝트의 복사본이다 -- AGridActor::BakeOverridesFromMarkers 참고.
 */
UCLASS(Abstract, HideCategories = (Rendering, Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, WorldPartition, Replication))
class LETSTAKETHESUBWAY_API AGridCellMarkerBase : public AActor
{
	GENERATED_BODY()

public:
	AGridCellMarkerBase();

	/** 덮은 셀이 어떤 타입이 될지. NoFloor는 생성 결과 상태라 직접 지정할 수 없다. */
	UPROPERTY(EditAnywhere, Category = "Grid Override", meta = (InvalidEnumValues = "NoFloor"))
	EGridCellType CellType = EGridCellType::Blocked;

	/** 런타임 진입 판정. Conditional 셀에서만 의미가 있다. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Grid Override",
		meta = (EditCondition = "CellType == EGridCellType::Conditional", EditConditionHides))
	TObjectPtr<UGridCellRule> Rule;

	/**
	 * Rule이 비어 있을 때 대신 만들 규칙 클래스.
	 *
	 * Rule은 Instanced 서브오브젝트라 손으로 클래스를 고르는 것 말고는 채울 방법이 없다.
	 * 개찰구 한 줄처럼 같은 규칙을 여러 마커에 반복해 주는 배치는 그렇게 하기에 번거롭고,
	 * 에디터 자동화에서는 아예 불가능하다. 이 슬롯을 채우면 굽는 시점에 기본값 그대로의
	 * 규칙 오브젝트가 만들어진다.
	 *
	 * Rule이 있으면 그쪽이 이긴다 -- 손으로 값을 조정한 규칙을 클래스 기본값이 덮어쓰지
	 * 않게 하기 위해서다.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid Override",
		meta = (EditCondition = "CellType == EGridCellType::Conditional", EditConditionHides))
	TSubclassOf<UGridCellRule> RuleClass;

	/** 마커가 겹치면 Priority가 높은 쪽이 이긴다. */
	UPROPERTY(EditAnywhere, Category = "Grid Override")
	int32 Priority = 0;

	/** 이 마커가 덮는 셀들, 그리드 좌표 기준. */
	virtual void GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const PURE_VIRTUAL(AGridCellMarkerBase::GatherCells, );

	/** 단일 셀 마커는 박스 마커와 비길 때 이긴다 -- 넓은 영역 위에 작은 수정을 얹는 용도다. */
	virtual bool IsSingleCellMarker() const { return true; }

	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void Destroyed() override;

	/** 덮는 셀이 모호하지 않도록 그리드 위로 옮긴다. */
	virtual void SnapToGrid(const AGridActor& Grid);

	/** 레벨의 모든 그리드에 다시 구우라고 알린다. */
	void NotifyGrid();

	/** CellType에 맞게 박스 색을 바꾼다. */
	void UpdateVisual();
#endif

protected:
	UPROPERTY(VisibleAnywhere, Category = "Grid Override")
	TObjectPtr<UBoxComponent> Box;
};

/** 액터 바로 아래의 셀 하나만 오버라이드한다. */
UCLASS(meta = (DisplayName = "Grid Cell Marker"))
class LETSTAKETHESUBWAY_API AGridCellMarker : public AGridCellMarkerBase
{
	GENERATED_BODY()

public:
	AGridCellMarker();

	virtual void GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const override;
};

/**
 * 직사각형 범위의 셀을 오버라이드한다.
 *
 * 크기를 월드 스케일이 아니라 셀 수로 두는 건 의도적이다: 스케일된 박스는 가장자리가
 * 실수 값이라 "이 셀이 반만 덮였나?"가 판단 문제가 되고 굽기 결과가 비결정적이 된다.
 */
UCLASS(meta = (DisplayName = "Grid Box Marker"))
class LETSTAKETHESUBWAY_API AGridBoxMarker : public AGridCellMarkerBase
{
	GENERATED_BODY()

public:
	AGridBoxMarker();

	UPROPERTY(EditAnywhere, Category = "Grid Override", meta = (ClampMin = 1, ClampMax = 512))
	FIntPoint SizeInCells = FIntPoint(3, 3);

	virtual void GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const override;
	virtual bool IsSingleCellMarker() const override { return false; }

#if WITH_EDITOR
	virtual void SnapToGrid(const AGridActor& Grid) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** 덮는 직사각형의 최소 모서리 셀. 액터 위치에서 계산한다. */
	FIntPoint GetMinCell(const AGridActor& Grid) const;
};
