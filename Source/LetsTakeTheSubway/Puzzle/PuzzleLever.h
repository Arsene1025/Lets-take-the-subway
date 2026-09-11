// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Puzzle/PuzzleTypes.h"
#include "PuzzleLever.generated.h"

class AGridActor;
class AGridPawn;
class APuzzleRotatingObstacle;
class UStaticMeshComponent;

/**
 * 돌아가는 장애물 하나를 돌리는 휠(레버 휠).
 *
 * 디자인 스케치 그대로 그린다: 기둥 위에 핸드휠이 얹혀 있고, 그것을 잡고 빙 돌려서 조작한다.
 * 커서를 얼마나 크게 휘두르든 드래그 한 번은 90도 회전 한 번이므로, 풀이는 플레이어가
 * 얼마나 세게 돌리느냐가 아니라 셀 수 있는 이동 횟수로 정해진다.
 *
 * 폰은 레버 옆에 서 있어야 한다. 그렇지 않으면 플랫폼 어디서든 구조물을 돌릴 수 있게 되고,
 * 장애물이 길을 막은 뒤 레버까지 걸어가는 과정 -- 이 퍼즐을 퍼즐답게 만드는 요소의
 * 대부분 -- 이 무의미해진다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleLever : public AActor
{
	GENERATED_BODY()

public:
	APuzzleLever();

	/** 이 휠이 돌리는 구조물. 클래스가 아니라 배치된 레버마다 설정한다. */
	UPROPERTY(EditInstanceOnly, Category = "Puzzle Lever")
	TObjectPtr<APuzzleRotatingObstacle> Target;

	/**
	 * 이 휠이 도는 방향.
	 *
	 * Free면 플레이어가 돌린 쪽으로 돌아간다. 한 방향으로 고정하면 반대쪽 드래그는 거부되고
	 * 휠도 따라 돌지 않는다. 한 장애물에 시계 레버와 반시계 레버를 따로 두면, 어느 레버까지
	 * 걸어갈 수 있느냐 자체가 퍼즐이 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Lever")
	EPuzzleRotationDirection Direction = EPuzzleRotationDirection::Free;

	/** 기둥 높이(cm). 휠은 그 위에 얹힌다. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Lever", meta = (ClampMin = 10.0))
	float PostHeight = 90.0f;

	/** 휠 지름(cm). */
	UPROPERTY(EditAnywhere, Category = "Puzzle Lever", meta = (ClampMin = 10.0))
	float WheelDiameter = 80.0f;

	/**
	 * 90도 회전이 발동하기까지 커서를 휠 둘레로 얼마나 돌려야 하는지.
	 *
	 * 의도적인 비틀기는 인식될 만큼 낮게, 클릭하다 살짝 흔들린 것만으로 방 하나만 한
	 * 구조물이 돌지는 않을 만큼 높게 잡는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Lever", meta = (ClampMin = 5.0, ClampMax = 180.0))
	float TurnThresholdDegrees = 60.0f;

	// ---------------------------------------------------------------- 조회

	FIntPoint GetCell() const { return Cell; }

	/** 휠의 월드 위치. 드래그는 이 점을 중심으로 잰다. */
	FVector GetWheelWorldLocation() const;

	/** 레버를 조작할 수 있는 네 개의 셀(조작 셀). */
	void GetOperatingCells(TArray<FIntPoint>& OutCells) const;

	bool IsPawnAdjacent(const AGridPawn* Pawn) const;

	/** 이 레버가 주어진 방향의 회전을 받는지. 드래그 프리뷰도 이걸 보고 휠을 돌릴지 정한다. */
	bool AllowsTurn(int32 TurnSign) const
	{
		return LTTSPuzzle::DirectionAllowsTurn(Direction, TurnSign);
	}

	// ---------------------------------------------------------------- 사용

	/** 대상을 90도 돌리거나 왜 안 되는지 알려준다. TurnSign +1이 시계 방향이다. */
	bool TryTurn(int32 TurnSign, const AGridPawn* Pawn, FText* OutReason = nullptr);

	/** 드래그하는 동안 휠 메시를 돌린다. 순전히 비주얼용이며 0을 주면 원위치한다. */
	void SetWheelPreviewAngle(float Degrees);

	// ---------------------------------------------------------------- 생명주기

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void RefreshVisual();

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Lever")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Lever")
	TObjectPtr<UStaticMeshComponent> PostMesh;

	/** 드래그로 돌아가는 부분. 도는 것은 전부 여기에 매달려 있어 회전 하나로 전체가 움직인다. */
	UPROPERTY(VisibleAnywhere, Category = "Puzzle Lever")
	TObjectPtr<USceneComponent> WheelPivot;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Lever")
	TObjectPtr<UStaticMeshComponent> WheelMesh;

	/** 테두리에 붙은 돌기. 이것이 없으면 둥근 모양이라 휠의 회전이 보이지 않는다. */
	UPROPERTY(VisibleAnywhere, Category = "Puzzle Lever")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	FIntPoint Cell = FIntPoint::ZeroValue;
};
