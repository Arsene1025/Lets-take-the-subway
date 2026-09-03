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
 * Walks the grid one cell at a time.
 *
 * No CharacterMovement and no collision: position is entirely determined by the grid, so
 * physics could only fight it. Z comes from each cell's floor height, which is what makes
 * stairs and ramps work without any extra code.
 *
 * The body is a ball: a 1 m sphere that fits inside one grid cell and rolls as it moves.
 */
UCLASS()
class LETSTAKETHESUBWAY_API AGridPawn : public APawn
{
	GENERATED_BODY()

public:
	AGridPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Plan a route now, or after the current cell step finishes if one is in progress. */
	void RequestMoveToCell(FIntPoint Goal);

	FIntPoint GetCurrentCell() const { return CurrentCell; }
	FIntPoint GetGoalCell() const { return GoalCell; }
	bool IsMoving() const { return !Path.IsEmpty(); }
	int32 GetRemainingSteps() const { return Path.Num(); }
	AGridActor* GetGrid() const { return Grid; }

	UPROPERTY(EditAnywhere, Category = "Grid Pawn", meta = (ClampMin = 1.0))
	float MoveSpeed = 400.0f;

	/** Distance from the cell floor to the actor origin. Matches the ball radius, so the ball rests on the floor. */
	UPROPERTY(EditAnywhere, Category = "Grid Pawn")
	float HeightAboveFloor = 50.0f;

	/** Ball radius in cm. The body mesh is a 1 m engine sphere scaled to this. */
	UPROPERTY(EditAnywhere, Category = "Grid Pawn", meta = (ClampMin = 1.0))
	float BallRadius = 50.0f;

	/** Spin the ball mesh by the distance travelled, as a real ball would. Purely visual. */
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

	/** Cells still to walk through. Front is the immediate next step. */
	TArray<FIntPoint> Path;

	/** A click received mid-step; re-planned once the pawn is cell-aligned again. */
	TOptional<FIntPoint> PendingGoal;
};
