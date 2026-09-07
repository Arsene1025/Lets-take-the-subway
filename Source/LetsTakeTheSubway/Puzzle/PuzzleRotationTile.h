// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridFootprint.h"
#include "PuzzleRotationTile.generated.h"

class AGridActor;
class APuzzleBlock;
class UStaticMeshComponent;

/**
 * 위에 서 있는 것을 90도 돌리는 정사각형 바닥 패치.
 *
 * 디자인에서는 이것을 "공간을 회전시킨다"라고 부르는데, 말 그대로 그렇게 동작한다: 영역 안에
 * 완전히 들어 있는 모든 블록이 영역 중심을 축으로 함께 실려 돌므로 서로의 상대 배치가
 * 유지되고 어떤 둘도 충돌할 수 없다. 여기서 엘리베이터의 가치는 문이 새로운 방향을 향하게
 * 된다는 점이다.
 *
 * 영역에 반만 걸친 블록은 말이 되는 목적지가 없으므로, 피스를 옆으로 밀어내는 대신 회전을
 * 아예 거부하고 플레이어에게 이유를 알려준다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleRotationTile : public AActor
{
	GENERATED_BODY()

public:
	APuzzleRotationTile();

	/** 한 변의 길이(셀 단위). 직사각형은 한 번 돌고 나면 자기 내용물을 담을 수 없으므로 정사각형이다. */
	UPROPERTY(EditAnywhere, Category = "Rotation Tile", meta = (ClampMin = 2, ClampMax = 16))
	int32 SizeInCells = 4;

	/** 위에서 본 회전 방향. 시계 방향이 디자인 문서의 도해와 일치한다. */
	UPROPERTY(EditAnywhere, Category = "Rotation Tile")
	bool bClockwise = true;

	UPROPERTY(EditAnywhere, Category = "Rotation Tile", meta = (ClampMin = 0.05))
	float RotateDuration = 0.4f;

	FGridRect GetRegion() const { return Region; }

	bool IsRotating() const { return bRotating; }

	/** 블록의 모든 셀이 영역 안에 들어 있으면(완전히 포함하면) true. */
	bool FullyContains(const APuzzleBlock& Block) const;

	/** 블록이 영역 일부를 덮지만 가장자리 밖으로 삐져나와 있으면(걸침) true. */
	bool Straddles(const APuzzleBlock& Block) const;

	/**
	 * 공간을 돌리거나, 왜 돌릴 수 없는지 이유를 알려준다.
	 *
	 * 블록이 경계에 걸쳐 있을 때, 회전한 블록이 걸을 수 있는 바닥이 아닌 곳에 놓이게 될 때,
	 * 안에 서 있는 폰이 설 수 없는 곳으로 옮겨지게 될 때 거부한다.
	 */
	bool TryRotate(FText* OutReason = nullptr);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void RefreshVisual();

	UPROPERTY(VisibleAnywhere, Category = "Rotation Tile")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 영역을 표시하는 평평한 패드. 클릭 트레이스와 바닥 트레이스가 통과하도록 콜리전이 없다. */
	UPROPERTY(VisibleAnywhere, Category = "Rotation Tile")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	/** 한쪽 모서리에 놓여, 레벨에서 회전 방향이 한눈에 읽히게 한다. */
	UPROPERTY(VisibleAnywhere, Category = "Rotation Tile")
	TObjectPtr<UStaticMeshComponent> CornerMesh;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	FGridRect Region;

	FVector PivotWorld = FVector::ZeroVector;

	bool bRotating = false;
	bool bDisabled = false;
	float RotationElapsed = 0.0f;

	/** 폰이 최종적으로 놓일 셀. 회전이 끝난 뒤 적용해 폰이 공간과 함께 이동하게 한다. */
	TOptional<FIntPoint> PendingPawnCell;
};
