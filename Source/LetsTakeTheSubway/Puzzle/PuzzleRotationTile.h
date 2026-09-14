// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Puzzle/PuzzleFloorTile.h"
#include "Puzzle/PuzzleTypes.h"
#include "PuzzleRotationTile.generated.h"

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
class LETSTAKETHESUBWAY_API APuzzleRotationTile : public APuzzleFloorTile
{
	GENERATED_BODY()

public:
	APuzzleRotationTile();

	/**
	 * 위에서 본 회전 방향. 시계 방향이 디자인 문서의 도해와 일치한다.
	 *
	 * Free는 번갈아 돌린다는 뜻이다: 발동할 때마다 시계, 반시계, 다시 시계 순서로 바뀐다.
	 * 타일은 블록이 올라오면 저혼자 도는 장치라 플레이어가 방향을 고를 입력이 없으므로, 무작위보다
	 * 예측할 수 있는 교대가 푸는 대상이 된다. 거부된 회전은 순서를 소모하지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Rotation Tile")
	EPuzzleRotationDirection Direction = EPuzzleRotationDirection::Clockwise;

	UPROPERTY(EditAnywhere, Category = "Rotation Tile", meta = (ClampMin = 0.05))
	float RotateDuration = 0.4f;

	bool IsRotating() const { return bRotating; }

	/**
	 * 공간을 돌리거나, 왜 돌릴 수 없는지 이유를 알려준다.
	 *
	 * 블록이 경계에 걸쳐 있을 때, 회전한 블록이 걸을 수 있는 바닥이 아닌 곳에 놓이게 될 때,
	 * 안에 서 있는 폰이 설 수 없는 곳으로 옮겨지게 될 때 거부한다.
	 */
	bool TryRotate(FText* OutReason = nullptr);

	virtual bool IsBusy() const override { return bRotating; }

	/** 블록이 타일 안에서 멈췄다. 회전판의 반응은 공간을 돌리는 것이다. */
	virtual void OnBlockCameToRest(APuzzleBlock& Block) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void PostLoad() override;

protected:
	virtual void RefreshVisual() override;
	virtual FString DescribeTile() const override;
	virtual void OnTileReady() override;

private:
	/** 한쪽 모서리에 놓여, 레벨에서 회전 방향이 한눈에 읽히게 한다. */
	UPROPERTY(VisibleAnywhere, Category = "Rotation Tile")
	TObjectPtr<UStaticMeshComponent> CornerMesh;

	/** 이번 발동에 쓸 TurnSign. Free가 아니면 설정된 방향이 그대로 나온다. */
	int32 ResolveTurnSign() const;

	/** Free일 때 다음에 돌 방향. 직렬화하지 않는다: 한 플레이 안에서만 의미가 있다. */
	int32 NextFreeTurnSign = 1;

	/**
	 * 구버전의 bool. 직렬화된 값을 PostLoad에서 Direction으로 옮기기 위해서만 남아 있다.
	 * 에디터에는 노출되지 않으며 새 코드는 이 값을 읽지 않는다.
	 */
	UPROPERTY()
	bool bClockwise_DEPRECATED = true;

	bool bRotating = false;
	float RotationElapsed = 0.0f;

	/** 폰이 최종적으로 놓일 셀. 회전이 끝난 뒤 적용해 폰이 공간과 함께 이동하게 한다. */
	TOptional<FIntPoint> PendingPawnCell;
};
