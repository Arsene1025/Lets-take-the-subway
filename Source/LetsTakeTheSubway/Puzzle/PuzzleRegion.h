// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridFootprint.h"
#include "PuzzleRegion.generated.h"

class AGridActor;
class UStaticMeshComponent;

/**
 * 퍼즐 조각이 돌아다닐 수 있는 셀 사각 영역.
 *
 * 기획의 "장애물이 이동 가능한 그리드"다. 두 번째 AGridActor로 만들지 않은 이유는 점유
 * 레이어에 있다: 그리드가 둘이면 폰과 블록이 서로를 보려고 양쪽에 이중으로 등록해야 하고,
 * 월드의 첫 그리드를 쓰는 기존 코드가 전부 어느 쪽을 뜻하는지 모호해진다. 대신 구간은
 * 서브시스템에 등록되는 평범한 액터이고, 제한은 APuzzleBlock::CanSlide가 한 줄로 건다.
 *
 * 블록은 BeginPlay에서 자기 사각형을 완전히 담는 구간 하나에 소속된다. 어느 구간에도 들지
 * 않는 블록은 경고를 남기고 제한 없이 움직인다 -- 구간을 배치하지 않은 기존 레벨이 그대로
 * 동작해야 하기 때문이다.
 *
 * 구간끼리 겹쳐서는 안 된다. 겹치면 블록이 어느 쪽에 속하는지가 등록 순서로 정해진다.
 */
UCLASS(HideCategories = (Rendering, Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleRegion : public AActor
{
	GENERATED_BODY()

public:
	APuzzleRegion();

	// ---------------------------------------------------------------- 저작

	/** 셀 단위 크기. 액터 위치가 이 사각형의 중심이다(박스 마커·회전판과 같은 규칙). */
	UPROPERTY(EditAnywhere, Category = "Puzzle Region", meta = (ClampMin = 1, ClampMax = 128))
	FIntPoint SizeInCells = FIntPoint(16, 16);

	/**
	 * 소속 블록을 이 사각형 안에 가둔다.
	 *
	 * 끄면 구간은 순수한 표식이 된다. 레벨을 정리하는 중에 잠시 제한을 풀어 보는 용도이며,
	 * 배치가 끝나면 다시 켠다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Region")
	bool bClampBlocks = true;

	/** 로그와 피드백에 쓰는 이름. 비워 두면 액터 이름을 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Region")
	FName RegionName;

	// ---------------------------------------------------------------- 조회

	FGridRect GetRect() const { return Region; }

	bool IsDisabled() const { return bDisabled; }

	/** 사각형 전체가 이 구간 안에 들어오는지. 블록의 목적지 검사가 이것을 쓴다. */
	bool ContainsRect(const FGridRect& Rect) const { return !bDisabled && Region.ContainsRect(Rect); }

	bool ContainsCell(FIntPoint Cell) const { return !bDisabled && Region.Contains(Cell); }

	/** 로그용 표시 이름. */
	FString GetDisplayName() const;

	// ---------------------------------------------------------------- 생명주기

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** 테두리 네 변을 크기에 맞춘다. 에디터에서만 보인다. */
	void RefreshVisual();

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Region")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 영역 경계를 보여 주는 얇은 막대 넷. 게임에서는 숨는다. */
	UPROPERTY(VisibleAnywhere, Category = "Puzzle Region")
	TArray<TObjectPtr<UStaticMeshComponent>> BorderMeshes;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	FGridRect Region;

	bool bDisabled = false;
};
