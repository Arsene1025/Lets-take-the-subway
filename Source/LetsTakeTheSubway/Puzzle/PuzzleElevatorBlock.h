// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Puzzle/PuzzleBlock.h"
#include "PuzzleElevatorBlock.generated.h"

class AGridPawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorBoarded, APuzzleElevatorBlock*, Elevator, APawn*, Pawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorArrived, APuzzleElevatorBlock*, Elevator, APawn*, Pawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorHoldReached, APuzzleElevatorBlock*, Elevator, APawn*, Pawn);

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
	 * 블록 자신의 프레임 기준으로 문이 놓인 **축**. 그 축의 양쪽 면에 같은 문이 하나씩 있다.
	 *
	 * 방향이 아니라 축인 이유는 문이 양면이기 때문이다. 방향으로 적으면 North와 South가
	 * 똑같은 배치를 뜻하는 서로 다른 값이 되어, 레벨에서 둘 중 무엇을 골라야 하는지 알 수
	 * 없고 회전 뒤에 어느 쪽이 "그" 문인지도 의미를 잃는다. AxisY는 (-Y, +Y) 두 짝,
	 * AxisX는 (-X, +X) 두 짝이다.
	 *
	 * 회전하면 블록과 함께 돈다(GetWorldDoorAxis).
	 */
	UPROPERTY(EditAnywhere, Category = "Elevator", meta = (InvalidEnumValues = "None, Both"))
	EPuzzleMoveAxis DoorAxis = EPuzzleMoveAxis::AxisY;

	/** 폰이 들어서면 발생한다. */
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorBoarded OnBoarded;

	/** 폰이 목적 층에 내려서 조작이 돌아왔을 때 발생한다. */
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorArrived OnArrived;

	/**
	 * 붙잡아 두는 승강(StartHoldingTravel)이 목표 높이에 닿았을 때 발생한다.
	 *
	 * OnArrived와 다른 사건이다. 저쪽은 "폰이 내려서 조작이 돌아왔다"이고, 이쪽은 "차체가
	 * 멈췄고 폰은 아직 안에 있다"이다. 스테이지 클리어 연출이 시작되는 자리다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorHoldReached OnHoldReached;

	// ---------------------------------------------------------------- 문

	/** 지금 그리드에 놓인 상태에서 문이 놓인 축. 그 축의 양쪽 면에 문이 하나씩 있다. */
	EPuzzleMoveAxis GetWorldDoorAxis() const;

	/**
	 * 지금 놓인 상태에서 문 하나가 향한 방향. 반대편에도 같은 문이 있다.
	 *
	 * 축의 양의 방향(AxisY면 North, AxisX면 East)을 돌려준다. 로그와 문 앞 셀 계산처럼
	 * 방향 하나를 집어야 하는 곳에서 쓰며, 반대쪽은 두 번 돌린 방향이다.
	 */
	EGridDirection GetWorldDoorDirection() const;

	/** 지정된 MoveAxis가 무엇이든 블록은 문의 축으로만 이동한다. */
	virtual EPuzzleMoveAxis GetWorldMoveAxis() const override;

	/** 문 바로 바깥의 셀. 양쪽 문 앞을 모두 돌려준다. */
	void GetDoorFrontCells(TArray<FIntPoint>& OutCells) const;

	/** 한쪽 문 앞의 셀만. 구조물이 출구 쪽을 고를 때 쓴다. */
	void GetDoorFrontCells(EGridDirection Door, TArray<FIntPoint>& OutCells) const;

	/**
	 * 지금 이 층에서 실제로 타고 내릴 수 있는 문 앞 셀.
	 *
	 * 문 앞 셀 전부가 쓸모 있는 것은 아니다. 샤프트에 걸린 차체의 한쪽 문 앞은 선로이거나
	 * 다른 층 바닥이고, 그런 셀에 폰을 보내면 걸어갈 수도 없고 보내 봤자 탈 수도 없다.
	 * 걸을 수 있고 차체와 같은 층인 셀만 남긴다.
	 */
	void GetBoardableDoorCells(TArray<FIntPoint>& OutCells) const;

	/** 폰이 타고 내릴 수 있는 문 앞 셀 중 하나에 서 있으면 true. */
	bool IsPawnAtDoor(const AGridPawn* Pawn) const;

	/**
	 * 폰이 타면 서게 되는 자리: **차체 한가운데**.
	 *
	 * 열차와 일부러 다르다. 열차는 45 m짜리 객차라 탄 문 앞에 그대로 서 있는 편이 자연스럽지만,
	 * 엘리베이터는 4 m짜리 방이고 문이 양쪽에 하나씩이라 어느 쪽으로 들어왔든 가운데 서는 것이
	 * 사람이 하는 짓에 가깝다.
	 */
	FVector GetSeatWorldFor(const AGridPawn& Pawn) const;

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

	/**
	 * 폰을 태우고 TargetZ까지 움직인 뒤 **그대로 멈춰 선다**. 내려 주지 않는다.
	 *
	 * 스테이지 클리어 연출용이다. 엔딩 승강기에는 도착할 층도, 내릴 바닥도 없다 -- 정해진
	 * 높이만큼 올라가다(또는 내려가다) 멈추고, 그 순간부터는 연출이 화면을 가져간다.
	 *
	 * 멈춘 뒤에도 IsTravelling()은 계속 true다. 그래서 입력 잠금 세 겹(폰이 그리드 밖,
	 * 차체가 Lifting, 구조물이 IsBusy)이 전부 살아 있고, 연출이 도는 동안 플레이어가
	 * 조각을 밀거나 폰을 걷게 할 수 없다.
	 *
	 * 도착 높이를 FloorZ로 삼지 않는다. 거기는 층이 아니라 허공이므로, 층으로 기록하면
	 * 구조물이 차체를 자기 것이 아니라고 판단해 도킹이 풀린다.
	 */
	bool StartHoldingTravel(double TargetZ, float Speed, AGridPawn* Rider);

	bool IsTravelling() const { return TravelPhase != ETravelPhase::None; }

	/** 붙잡아 두는 승강이 목표 높이에 닿아 멈춰 서 있으면 true. */
	bool IsHolding() const { return TravelPhase == ETravelPhase::Holding; }

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
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void RefreshVisual() override;

	/** 그레이박스에서 출입구를 알아볼 수 있도록 양쪽 문 면에 붙인 슬랩. */
	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UStaticMeshComponent> FarDoorMesh;

private:
	// ---------------------------------------------------------------- 구버전 저작 이전
	//
	// 예전에는 문을 방향 하나(EGridDirection)로 적었다. 이름을 바꾸지 않고 남겨 두는 이유는
	// 이름이 곧 직렬화 키이기 때문이다 -- 이름을 바꾸면 이미 배치된 엘리베이터의 저장된
	// 값이 조용히 버려지고, 동서로 열리던 문이 남북으로 돌아 퍼즐 푸는 방법이 통째로 바뀐다.

	/** 구버전의 문 방향. PostLoad에서 DoorAxis로 옮기기 위해서만 남아 있다. */
	UPROPERTY()
	EGridDirection DoorDirection = EGridDirection::North;

	/**
	 * DoorDirection이 이미 DoorAxis로 옮겨졌는지.
	 *
	 * 이전은 딱 한 번만 일어나야 한다. 표식이 없으면 로드할 때마다 옛 방향이 되살아나,
	 * 새로 배치한 엘리베이터에 지정한 DoorAxis를 다음 로드가 덮어써 버린다. 그래서 저작에서
	 * DoorAxis를 건드리는 순간(PostEditChangeProperty) 이 표식을 세운다.
	 */
	UPROPERTY()
	bool bDoorAxisMigrated = false;

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
		Unloading,

		/**
		 * 목표 높이에 멈춰 섰고, 폰은 아직 타고 있다. 종착 상태다.
		 *
		 * 여기서 빠져나가는 길은 없다 -- 연출이 씬을 넘기거나 레벨을 다시 여는 것으로 끝난다.
		 * 그것이 의도다: 엔딩 승강기는 돌아오지 않는다.
		 */
		Holding
	};

	/** 문 슬랩 하나의 위치와 크기를 맞춘다. Sign은 DoorDirection 쪽이면 +1, 반대쪽이면 -1. */
	void PlaceDoorMesh(UStaticMeshComponent* Mesh, int32 Sign) const;

	/** 승강을 끝내고 평소 상태로 돌아간다. */
	void FinishTravel();

	/** 두 승강 진입점이 공유하는 준비: 폰 태우기, 입력 잠그기, 첫 단계로 보내기. */
	bool BeginTravel(double TargetZ, float Speed, AGridPawn* Pawn);

	ETravelPhase TravelPhase = ETravelPhase::None;

	/** 목표 높이에서 내려 주지 않고 멈춰 설지. StartHoldingTravel이 세운다. */
	bool bHoldAtTarget = false;

	TWeakObjectPtr<AGridPawn> Rider;

	double TravelTargetZ = 0.0;
	FVector TravelExitWorld = FVector::ZeroVector;
	float TravelSpeed = 200.0f;
	float TravelDwellSeconds = 0.5f;
	float TravelDwellElapsed = 0.0f;
};
