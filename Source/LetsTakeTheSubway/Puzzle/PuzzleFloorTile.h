// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridFootprint.h"
#include "PuzzleFloorTile.generated.h"

class AGridActor;
class APuzzleBlock;
class UStaticMeshComponent;

/**
 * 블록이 그 위에 완전히 올라서면 반응하는 정사각형 바닥 패치.
 *
 * 회전판과 엘리베이터 구조물이 공유하는 뼈대다. 둘은 반응이 다를 뿐(하나는 공간을 돌리고
 * 하나는 층을 옮긴다) 나머지가 같다: 셀에 맞춰 놓이고, 자기 영역을 계산하고, 무엇이 자기
 * 안에 완전히 들어왔는지 판정하고, 블록이 드래그를 마칠 때 알림을 받는다.
 *
 * 정사각형인 이유는 회전판에서 왔다. 회전판은 돌고 나서도 자기 내용물을 담아야 하므로
 * 정사각형이어야 하고, 구조물은 4x4 엘리베이터를 담기만 하면 되므로 정사각형이어도 무방하다.
 *
 * 타일끼리 겹쳐서는 안 된다. 겹치면 그 사이에 멈춘 블록을 양쪽이 자기 것이라 주장하고,
 * 어느 쪽이 이기는지를 등록 순서가 정하게 된다.
 */
UCLASS(Abstract, HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleFloorTile : public AActor
{
	GENERATED_BODY()

public:
	APuzzleFloorTile();

	/** 한 변의 길이(셀 단위). */
	UPROPERTY(EditAnywhere, Category = "Floor Tile", meta = (ClampMin = 2, ClampMax = 16))
	int32 SizeInCells = 4;

	// ---------------------------------------------------------------- 조회

	FGridRect GetRegion() const { return Region; }

	/** 영역 중심의 월드 위치, 바닥 높이 기준. 회전축이자 좌석 위치다. */
	FVector GetPivotWorld() const { return PivotWorld; }

	bool IsDisabled() const { return bDisabled; }

	/** 블록의 모든 셀이 영역 안에 들어 있으면(완전히 포함하면) true. */
	bool FullyContains(const APuzzleBlock& Block) const;

	/** 블록이 영역 일부를 덮지만 가장자리 밖으로 삐져나와 있으면(걸침) true. */
	bool Straddles(const APuzzleBlock& Block) const;

	/**
	 * 타일이 지금 무언가를 하고 있어 입력을 막아야 하면 true.
	 *
	 * 회전판은 도는 동안, 구조물은 엘리베이터가 층을 오가는 동안이다.
	 */
	virtual bool IsBusy() const { return false; }

	/**
	 * 블록이 이 타일 안에서 드래그를 마치고 멈췄다.
	 *
	 * 서브시스템이 블록을 완전히 담는 타일을 찾아 한 번 부른다. 회전판은 여기서 공간을
	 * 돌리고, 엘리베이터 구조물은 도킹한다.
	 */
	virtual void OnBlockCameToRest(APuzzleBlock& Block) {}

	// ---------------------------------------------------------------- 생명주기

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** 패드와 장식의 크기를 SizeInCells에 맞춘다. */
	virtual void RefreshVisual();

	/** BeginPlay 로그 끝에 붙는 한 마디. 파생 클래스가 자기 설정을 알린다. */
	virtual FString DescribeTile() const { return FString(); }

	/** 영역이 확정되고 등록까지 끝난 뒤. 파생 클래스의 추가 초기화 자리다. */
	virtual void OnTileReady() {}

	AGridActor* GetGrid() const { return Grid; }

	UPROPERTY(VisibleAnywhere, Category = "Floor Tile")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 영역을 표시하는 평평한 패드. 클릭 트레이스와 바닥 트레이스가 통과하도록 콜리전이 없다. */
	UPROPERTY(VisibleAnywhere, Category = "Floor Tile")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	FGridRect Region;

	FVector PivotWorld = FVector::ZeroVector;

	bool bDisabled = false;
};
