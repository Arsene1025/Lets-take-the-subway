// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Grid/GridTypes.h"
#include "GridPawn.generated.h"

class AGridActor;
class UCameraComponent;
class USphereComponent;
class USceneComponent;
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

	/**
	 * 이동 키를 누르고 있는 동안 계속 걸을 방향. 비어 있으면 지금 향하던 셀에 서고 멈춘다.
	 *
	 * 길찾기가 아니라 방향인 이유는 조작이 키보드이기 때문이다(2026-09-11). 플레이어가
	 * 목적지를 고르는 것이 아니라 폰을 직접 민다. 컨트롤러가 매 틱 이것을 갱신하고, 칸과
	 * 칸을 이어 붙이는 일은 폰이 자기 Tick에서 한다 -- 도착한 프레임을 그냥 끝내면 칸마다
	 * 한 프레임씩 서게 되어 걸음이 셀 경계마다 끊긴다.
	 */
	void SetHeldDirection(TOptional<EGridDirection> Dir);

	/** 지금 계속 걸으라고 지시받은 방향. */
	TOptional<EGridDirection> GetHeldDirection() const { return HeldDirection; }

	/**
	 * 그 방향으로 한 칸만 걷는다. 이동 키를 한 번 눌렀다 뗀 것과 같다.
	 *
	 * 콘솔 명령(ltts.PawnStep)이 쓰는 시험용 입구다. 키보드와 정확히 같은 함수를 지나므로,
	 * 이 명령이 통과했다는 것은 그 경로가 통과했다는 뜻이다.
	 */
	bool StepOnce(EGridDirection Dir);

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
	 *
	 * FollowComponent를 주면 액터 원점 대신 그 컴포넌트를 따라간다. 에스컬레이터처럼 액터
	 * 자체는 가만히 있고 그 위의 한 점만 움직이는 탈것을 위한 것이다. 비워 두면 지금까지처럼
	 * 탈것 액터의 원점을 따라간다.
	 */
	void BoardVehicle(AActor* InVehicle, const FVector& SeatWorld, USceneComponent* FollowComponent = nullptr);

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

	/**
	 * 카메라가 내려다보는 방향. 스프링암의 월드 회전으로 그대로 들어간다.
	 *
	 * 아트가 레벨에 놓아 둔 고정 카메라(`CAM_ToonIso`, `CAM_Isometric`)와 같은 값이다.
	 * pitch -35.264도는 정사각 아이소메트릭 각(atan(1/√2))이고, yaw -135도는 역을 북동쪽에서
	 * 내려다본다. 이전 값(-55, 45)은 반대편인 남서쪽에서 보는 것이라 승강장 벽이 화면 앞을
	 * 가려 그 뒤의 셀을 클릭할 수 없었다.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid Pawn|Camera")
	FRotator CameraRotation = FRotator(-35.264f, -135.0f, 0.0f);

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

	/** 좌석 오프셋의 기준점. 앵커 컴포넌트가 있으면 그것, 없으면 탈것 액터의 원점이다. */
	FVector RideBaseLocation() const;

	void TickBump(float DeltaSeconds);

	/** 그리드 위 한 셀씩 걷는 기존 루프. */
	void TickGridStep(float DeltaSeconds);

	/**
	 * 지시받은 방향으로 한 칸 나아가려 한다. 경로 한 칸을 세워 두면 성공이다.
	 *
	 * 길찾기를 거치지 않는 이유는 키가 가리키는 방향이 곧 명령이기 때문이다. 막혔으면
	 * 돌아가는 길을 찾는 대신 그 자리에 선다 -- 키보드 조작에서 폰이 제멋대로 우회하면
	 * 플레이어가 폰을 미는 감각이 사라진다.
	 */
	bool TryStepHeldDirection();

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

	/** 누르고 있는 이동 키의 방향 (SetHeldDirection). 비어 있으면 다음 셀에서 멈춘다. */
	TOptional<EGridDirection> HeldDirection;

	/**
	 * 지금 방향이 막혔다고 이미 알렸는지.
	 *
	 * 키를 누르고 있는 동안 매 틱 같은 줄을 다시 쓰지 않기 위해서다. 방향이 바뀌면 지운다.
	 */
	bool bHeldDirectionBlockedReported = false;

	// ---------------------------------------------------------------- 탑승 상태

	ERideState RideState = ERideState::OnGrid;

	TWeakObjectPtr<AActor> Vehicle;

	/** 탈것 안에서 따라갈 지점 (있다면). 없으면 탈것 액터의 원점을 따른다. */
	TWeakObjectPtr<USceneComponent> RideAnchor;

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
