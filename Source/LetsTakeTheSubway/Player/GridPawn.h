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

private:
	bool EnsureGrid();
	bool PlanPath(FIntPoint Goal);
	FVector CellStandLocation(FIntPoint Cell) const;
	void ReportFeedback(const FString& Message, const FLinearColor& Color) const;
	void RefreshPathDebug() const;
	void RollBody(const FVector& Delta);

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
};
