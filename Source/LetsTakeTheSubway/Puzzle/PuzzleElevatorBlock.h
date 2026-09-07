// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Puzzle/PuzzleBlock.h"
#include "PuzzleElevatorBlock.generated.h"

class AGridPawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorBoarded, APuzzleElevatorBlock*, Elevator, APawn*, Pawn);

/**
 * 목표 조각: 한 면에 문이 달린 4x4 엘리베이터.
 *
 * 일반 블록과 달리 문이 향한 축으로만 이동할 수 있으며, 이것이 퍼즐을 퍼즐답게 만든다 --
 * 플레이어에게 닿는 일은 아무 데나 밀어 오는 게 아니라, 회전 타일 위에 올려서 문이
 * 닿을 수 있는 쪽을 향하게 만드는 일이다.
 *
 * 탑승은 당분간 일부러 알림에 그친다. 폰을 층 사이로 옮기려면 그레이박스 계획에 스케치된
 * 층별 그리드와 링크 셀이 필요하다. 그것이 생기면 OnBoarded를 구독하면 되고, 여기서는
 * 아무것도 바꿀 필요가 없다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleElevatorBlock : public APuzzleBlock
{
	GENERATED_BODY()

public:
	APuzzleElevatorBlock();

	/** 블록 자신의 프레임 기준으로 문이 있는 면. 회전하면 블록과 함께 돈다. */
	UPROPERTY(EditAnywhere, Category = "Elevator")
	EGridDirection DoorDirection = EGridDirection::North;

	/** 폰이 들어서면 발생한다. 층 이동은 여기에 매달린다. */
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorBoarded OnBoarded;

	/** 지금 그리드에 놓인 상태에서 문이 향한 방향. */
	EGridDirection GetWorldDoorDirection() const;

	/** 지정된 MoveAxis가 무엇이든 블록은 문의 축으로만 이동한다. */
	virtual EPuzzleMoveAxis GetWorldMoveAxis() const override;

	/** 문 바로 바깥의 셀 한 줄. 폰은 여기서 탑승한다. */
	void GetDoorFrontCells(TArray<FIntPoint>& OutCells) const;

	/** 폰이 문 앞 셀 중 하나에 서 있으면 true. */
	bool IsPawnAtDoor(const AGridPawn* Pawn) const;

	/** 폰을 태우거나, 왜 못 타는지 설명한다. */
	bool TryBoard(AGridPawn* Pawn, FText* OutReason = nullptr);

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

protected:
	virtual void RefreshVisual() override;

	/** 그레이박스에서 출입구를 알아볼 수 있도록 문 면에 붙인 슬랩. */
	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UStaticMeshComponent> DoorMesh;
};
