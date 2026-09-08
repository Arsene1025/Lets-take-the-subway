// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Puzzle/PuzzleBlock.h"
#include "PuzzleElevatorBlock.generated.h"

class AGridPawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorBoarded, APuzzleElevatorBlock*, Elevator, APawn*, Pawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorArrived, APuzzleElevatorBlock*, Elevator, APawn*, Pawn);

/**
 * 목표 조각: 마주 보는 두 면에 문이 달린 4x4 엘리베이터.
 *
 * 기획대로 문은 양쪽에 있다. 그래서 회전판 위에서 어느 쪽으로 돌든 상관이 없고, 대신
 * 이동은 문이 놓인 **축**으로만 가능하다 -- 이것이 퍼즐을 퍼즐답게 만든다. 플레이어에게
 * 닿는 일은 아무 데나 밀어 오는 게 아니라, 회전판에 올려 문의 축을 바꾸는 일이다.
 *
 * 수평 이동(드래그)과 수직 이동(승강)은 전혀 다른 일이다. 수평은 플레이어가 미는 퍼즐이고,
 * 수직은 엘리베이터 구조물(APuzzleElevatorDock) 위에서만 일어나는 연출이다. 구조물이
 * 목표 높이와 출구를 정해 StartVerticalTravel을 부르고, 그 뒤로는 이 클래스가 폰을 태우고
 * 옮기고 내려 준다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleElevatorBlock : public APuzzleBlock
{
	GENERATED_BODY()

public:
	APuzzleElevatorBlock();

	/**
	 * 블록 자신의 프레임 기준으로 문이 놓인 면. 반대편에도 같은 문이 있다.
	 *
	 * 회전하면 블록과 함께 돈다. 양쪽에 문이 있으므로 North와 South는 결과가 같고, 중요한
	 * 것은 문이 어느 축에 놓였는지뿐이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Elevator")
	EGridDirection DoorDirection = EGridDirection::North;

	/** 폰이 들어서면 발생한다. */
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorBoarded OnBoarded;

	/** 폰이 목적 층에 내려서 조작이 돌아왔을 때 발생한다. */
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorArrived OnArrived;

	// ---------------------------------------------------------------- 문

	/** 지금 그리드에 놓인 상태에서 문 하나가 향한 방향. 반대편에도 문이 있다. */
	EGridDirection GetWorldDoorDirection() const;

	/** 지정된 MoveAxis가 무엇이든 블록은 문의 축으로만 이동한다. */
	virtual EPuzzleMoveAxis GetWorldMoveAxis() const override;

	/** 문 바로 바깥의 셀. 양쪽 문 앞을 모두 돌려준다. */
	void GetDoorFrontCells(TArray<FIntPoint>& OutCells) const;

	/** 한쪽 문 앞의 셀만. 구조물이 출구 쪽을 고를 때 쓴다. */
	void GetDoorFrontCells(EGridDirection Door, TArray<FIntPoint>& OutCells) const;

	/** 폰이 문 앞 셀 중 하나에 서 있으면 true. */
	bool IsPawnAtDoor(const AGridPawn* Pawn) const;

	/** 폰을 태우거나, 왜 못 타는지 설명한다. 실제 승강은 구조물이 이어서 지시한다. */
	bool TryBoard(AGridPawn* Pawn, FText* OutReason = nullptr);

	// ---------------------------------------------------------------- 승강

	/**
	 * 폰을 태우고 TargetZ까지 올라가거나 내려간 뒤, ExitWorld 근처에서 내려 준다.
	 *
	 * 구조물이 부른다. 순서는 태우기(문 앞 -> 차체 안 직선 보행) -> Z 이동 -> 잠시 정지 ->
	 * 내리기(차체 -> 그리드 직선 보행)이며, 그동안 AnimState는 Lifting이라 입력이 잠긴다.
	 */
	bool StartVerticalTravel(double TargetZ, const FVector& ExitWorld, float Speed, float DwellSeconds, AGridPawn* Rider);

	bool IsTravelling() const { return TravelPhase != ETravelPhase::None; }

	/** 지금 이 차체에 타고 있거나 타는 중인 폰. */
	AGridPawn* GetRider() const { return Rider.Get(); }

	// ---------------------------------------------------------------- 이동 제약

	/** 누가 타고 있거나 승강 중이면 밀 수 없다. */
	virtual bool CanStartMoving(FText* OutReason = nullptr) const override;

	/** 지금 서 있는 층과 같은 높이의 바닥으로만 나갈 수 있다. */
	virtual bool CanOccupyRect(const FGridRect& Rect, FText* OutReason = nullptr) const override;

	// ---------------------------------------------------------------- 생명주기

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

protected:
	virtual void RefreshVisual() override;

	/** 그레이박스에서 출입구를 알아볼 수 있도록 양쪽 문 면에 붙인 슬랩. */
	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UStaticMeshComponent> FarDoorMesh;

private:
	/** 승강 한 번의 진행 단계. */
	enum class ETravelPhase : uint8
	{
		None,

		/** 폰이 문 앞에서 차체 안으로 걸어 들어오는 중. */
		WaitingForRider,

		/** Z 이동 중. */
		Moving,

		/** 도착했고, 문이 열리기까지의 짧은 정지. */
		Dwelling,

		/** 폰이 차체에서 그리드로 걸어 나가는 중. */
		Unloading
	};

	/** 문 슬랩 하나의 위치와 크기를 맞춘다. Sign은 DoorDirection 쪽이면 +1, 반대쪽이면 -1. */
	void PlaceDoorMesh(UStaticMeshComponent* Mesh, int32 Sign) const;

	/** 승강을 끝내고 평소 상태로 돌아간다. */
	void FinishTravel();

	ETravelPhase TravelPhase = ETravelPhase::None;

	TWeakObjectPtr<AGridPawn> Rider;

	double TravelTargetZ = 0.0;
	FVector TravelExitWorld = FVector::ZeroVector;
	float TravelSpeed = 200.0f;
	float TravelDwellSeconds = 0.5f;
	float TravelDwellElapsed = 0.0f;
};
