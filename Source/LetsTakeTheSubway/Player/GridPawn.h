// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GridPawn.generated.h"

class AGridActor;
class UCameraComponent;
class UCapsuleComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/**
 * Walks the grid one cell at a time.
 *
 * No CharacterMovement and no collision: position is entirely determined by the grid, so
 * physics could only fight it. Z comes from each cell's floor height, which is what makes
 * stairs and ramps work without any extra code.
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

	/** Distance from the cell floor to the actor origin. Matches the capsule half height. */
	UPROPERTY(EditAnywhere, Category = "Grid Pawn")
	float HeightAboveFloor = 88.0f;

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

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	UPROPERTY(VisibleAnywhere, Category = "Grid Pawn")
	TObjectPtr<UCapsuleComponent> Capsule;

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
