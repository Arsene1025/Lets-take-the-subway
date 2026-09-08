// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GridPawn.generated.h"

class AGridActor;
class UCameraComponent;
class USphereComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/**
 * 그리드를 한 번에 한 셀씩 걷는다.
 *
 * CharacterMovement도 콜리전도 없다: 위치는 전적으로 그리드가 정하므로 물리는 방해만
 * 될 뿐이다. Z는 각 셀의 바닥 높이에서 오며, 덕분에 계단과 경사로가 별도 코드 없이
 * 동작한다.
 *
 * 몸체는 공이다: 그리드 셀 하나에 들어가는 1 m 구체이고 이동하면서 굴러간다.
 */
UCLASS()
class LETSTAKETHESUBWAY_API AGridPawn : public APawn
{
	GENERATED_BODY()

public:
	AGridPawn();

	/**
	 * 폰이 그리드 위에 있는지, 움직이는 것에 실려 있는지.
	 *
	 * 그리드 밖으로 나가는 상태를 폰이 직접 들고 있는 이유는, 이 사실을 알아야 하는 곳이
	 * 여럿이기 때문이다: 퍼즐 블록은 실려 있는 폰의 셀을 비켜 갈 필요가 없고, 컨트롤러는
	 * 그동안 클릭을 받지 않으며, 호버 오버레이도 꺼져야 한다.
	 */
	enum class ERideState : uint8
	{
		/** 평소. 셀 위에 서 있거나 셀 사이를 걷는 중. */
		OnGrid,

		/** 탈것으로 걸어 들어가는 중. 셀 검사 없이 직선으로 간다. */
		Entering,

		/** 탈것에 실려 있다. 위치는 전적으로 탈것이 정한다. */
		Riding,

		/** 탈것에서 그리드로 걸어 나오는 중. 도착하면 그 셀에 선다. */
		Leaving
	};

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 지금 경로를 계획하거나, 셀 스텝이 진행 중이면 그 스텝이 끝난 뒤 계획한다. */
	void RequestMoveToCell(FIntPoint Goal);

	FIntPoint GetCurrentCell() const { return CurrentCell; }
	FIntPoint GetGoalCell() const { return GoalCell; }
	bool IsMoving() const { return !Path.IsEmpty(); }
	int32 GetRemainingSteps() const { return Path.Num(); }
	AGridActor* GetGrid() const { return Grid; }

	/**
	 * 폰이 들어가고 있는 셀 (있다면).
	 *
	 * 현재 셀과 합치면 다음 순간의 폰 풋프린트가 되며, 슬라이드하는 블록이 피해야 하는
	 * 영역이 바로 이것이다: 폰은 스텝 대부분의 시간 동안 두 셀 사이에 있고, 이미 진입을
	 * 확정한 셀로 블록을 밀어 넣으면 폰이 블록 안에 갇힌다.
	 */
	TOptional<FIntPoint> GetNextCell() const
	{
		return Path.IsEmpty() ? TOptional<FIntPoint>() : TOptional<FIntPoint>(Path[0]);
	}

	/** 현재 경로를 버리고 마지막으로 완전히 들어갔던 셀로 되돌아간다. */
	void StopAndSnapToCurrentCell(const FString& Reason, const FLinearColor& Color = FLinearColor(1.0f, 0.65f, 0.05f));

	/** 폰을 셀로 곧바로 옮긴다. 회전하는 공간이 폰을 함께 실어 나를 때 쓴다. */
	void TeleportToCell(FIntPoint Cell);

	// ---------------------------------------------------------------- 탑승과 하차

	/**
	 * 탈것에 올라탄다. 폰은 SeatWorld까지 직선으로 걸어간 뒤 탈것에 붙는다.
	 *
	 * 순간이동이 아니라 걸어 들어가는 이유는, 그래야 어느 문으로 들어갔는지가 화면에
	 * 보이고 탑승이 플레이어의 행동으로 읽히기 때문이다. 걷는 동안 조작은 잠긴다.
	 */
	void BoardVehicle(AActor* InVehicle, const FVector& SeatWorld);

	/**
	 * 탈것에서 내려 그리드로 걸어 나온다.
	 *
	 * NearWorld 근처에서 걸어 들어갈 셀을 그리드에게 물어본다(AGridActor::FindEntryCell).
	 * 셀을 못 찾으면 폰이 허공에 남지 않도록 마지막으로 알던 셀로 되돌린다.
	 */
	void WalkOntoGrid(const FVector& NearWorld, int32 SearchRadius = 8);

	/**
	 * 막힌 쪽으로 살짝 부딪혔다가 되돌아온다.
	 *
	 * 기획의 "지나갈 수 없다는 걸 보여 주는 연출"이다. 폰이 공이라 애니메이션이 없으므로
	 * 짧은 위치 흔들림으로 대신한다. 서 있을 때만 동작한다 -- 걷는 중에 흔들면 스텝 보간과
	 * 싸운다.
	 */
	void Bump(FIntPoint TowardCell);

	ERideState GetRideState() const { return RideState; }
	bool IsOnGrid() const { return RideState == ERideState::OnGrid; }
	bool IsRiding() const { return RideState == ERideState::Riding; }

	/** 지금 실려 있는(또는 걸어 들어가고 있는) 탈것. */
	AActor* GetVehicle() const { return Vehicle.Get(); }

	UPROPERTY(EditAnywhere, Category = "Grid Pawn", meta = (ClampMin = 1.0))
	float MoveSpeed = 400.0f;

	/** 셀 바닥에서 액터 원점까지의 거리. 공 반지름과 같아서 공이 바닥 위에 놓인다. */
	UPROPERTY(EditAnywhere, Category = "Grid Pawn")
	float HeightAboveFloor = 50.0f;

	/** 공 반지름 (cm). 몸체 메시는 엔진의 1 m 구체를 이 값에 맞게 스케일한 것이다. */
	UPROPERTY(EditAnywhere, Category = "Grid Pawn", meta = (ClampMin = 1.0))
	float BallRadius = 50.0f;

	/** 실제 공처럼 이동 거리만큼 공 메시를 굴린다. 순전히 시각 효과다. */
	UPROPERTY(EditAnywhere, Category = "Grid Pawn")
	bool bRollWhileMoving = true;

	UPROPERTY(EditAnywhere, Category = "Grid Pawn|Camera")
	float CameraArmLength = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Grid Pawn|Camera")
	FRotator CameraRotation = FRotator(-55.0f, 45.0f, 0.0f);

	/** 부딪힘 연출에서 막힌 쪽으로 나갔다 오는 거리(cm). */
	UPROPERTY(EditAnywhere, Category = "Grid Pawn|Feedback", meta = (ClampMin = 0.0))
	float BumpDistance = 15.0f;

	/** 부딪힘 연출 한 번에 걸리는 시간(초). */
	UPROPERTY(EditAnywhere, Category = "Grid Pawn|Feedback", meta = (ClampMin = 0.01))
	float BumpDuration = 0.2f;

private:
	bool EnsureGrid();
	bool PlanPath(FIntPoint Goal);
	FVector CellStandLocation(FIntPoint Cell) const;
	void ReportFeedback(const FString& Message, const FLinearColor& Color) const;
	void RefreshPathDebug() const;
	void RollBody(const FVector& Delta);

	/** Entering·Leaving의 직선 보행. 셀을 보지 않고 목표점까지 정속으로 간다. */
	void TickStraightWalk(float DeltaSeconds);

	/** Riding: 위치를 탈것에 맞춘다. */
	void TickRide(float DeltaSeconds);

	void TickBump(float DeltaSeconds);

	/** 그리드 위 한 셀씩 걷는 기존 루프. */
	void TickGridStep(float DeltaSeconds);

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	UPROPERTY(VisibleAnywhere, Category = "Grid Pawn")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Grid Pawn")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Grid Pawn|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Grid Pawn|Camera")
	TObjectPtr<UCameraComponent> Camera;

	FIntPoint CurrentCell = FIntPoint::ZeroValue;
	FIntPoint GoalCell = FIntPoint::ZeroValue;

	/** 아직 지나가야 할 셀들. 맨 앞이 바로 다음 스텝이다. */
	TArray<FIntPoint> Path;

	/** 스텝 도중에 받은 클릭; 폰이 다시 셀에 정렬되면 재계획한다. */
	TOptional<FIntPoint> PendingGoal;

	// ---------------------------------------------------------------- 탑승 상태

	ERideState RideState = ERideState::OnGrid;

	TWeakObjectPtr<AActor> Vehicle;

	/** 탈것 원점 기준의 좌석 위치. 붙은 순간에 재고, 그 뒤로는 이것만 더한다. */
	FVector RideOffset = FVector::ZeroVector;

	/** Entering·Leaving에서 걸어갈 목표점. */
	FVector WalkTarget = FVector::ZeroVector;

	/** Leaving이 끝나면 서게 될 셀. */
	FIntPoint LandingCell = FIntPoint::ZeroValue;

	// ---------------------------------------------------------------- 부딪힘 연출

	FVector BumpDirection = FVector::ZeroVector;
	float BumpElapsed = 0.0f;
	bool bBumping = false;
};
