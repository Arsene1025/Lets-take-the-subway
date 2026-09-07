// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PuzzleLever.generated.h"

class AGridActor;
class AGridPawn;
class APuzzleRotatingObstacle;
class UStaticMeshComponent;

/**
 * The wheel that turns one rotating obstacle.
 *
 * Drawn as the design sketched it: a post with a handwheel on top, turned by taking hold of
 * it and dragging round. One drag is one quarter turn however far the cursor is swung, so a
 * solution is a countable number of moves rather than a matter of how hard the player spins.
 *
 * The pawn has to be standing next to it. Without that the structure could be turned from
 * anywhere on the platform, and the walk to the lever -- which is most of what makes the
 * puzzle a puzzle once the obstacle is in the way -- would not matter.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleLever : public AActor
{
	GENERATED_BODY()

public:
	APuzzleLever();

	/** The structure this wheel turns. Set per placed lever, not on the class. */
	UPROPERTY(EditInstanceOnly, Category = "Puzzle Lever")
	TObjectPtr<APuzzleRotatingObstacle> Target;

	/** Post height in cm. The wheel sits on top of it. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Lever", meta = (ClampMin = 10.0))
	float PostHeight = 90.0f;

	/** Wheel diameter in cm. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Lever", meta = (ClampMin = 10.0))
	float WheelDiameter = 80.0f;

	/**
	 * How far the cursor must be swung round the wheel before the quarter turn fires.
	 *
	 * Low enough that a deliberate twist registers, high enough that a small wobble while
	 * clicking does not turn a structure the size of a room.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Lever", meta = (ClampMin = 5.0, ClampMax = 180.0))
	float TurnThresholdDegrees = 60.0f;

	// ---------------------------------------------------------------- Queries

	FIntPoint GetCell() const { return Cell; }

	/** World position of the wheel, which is what a drag is measured around. */
	FVector GetWheelWorldLocation() const;

	/** The four cells the lever can be worked from. */
	void GetOperatingCells(TArray<FIntPoint>& OutCells) const;

	bool IsPawnAdjacent(const AGridPawn* Pawn) const;

	// ---------------------------------------------------------------- Use

	/** Turn the target a quarter, or explain why not. TurnSign +1 reads as clockwise. */
	bool TryTurn(int32 TurnSign, const AGridPawn* Pawn, FText* OutReason = nullptr);

	/** Spin the wheel mesh while it is being dragged. Purely visual; zero puts it back. */
	void SetWheelPreviewAngle(float Degrees);

	// ---------------------------------------------------------------- Lifecycle

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

	/** Turned by the drag. Everything that spins hangs off this so one rotation moves it all. */
	UPROPERTY(VisibleAnywhere, Category = "Puzzle Lever")
	TObjectPtr<USceneComponent> WheelPivot;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Lever")
	TObjectPtr<UStaticMeshComponent> WheelMesh;

	/** A stub on the rim, so the wheel's rotation is visible on an otherwise round shape. */
	UPROPERTY(VisibleAnywhere, Category = "Puzzle Lever")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	FIntPoint Cell = FIntPoint::ZeroValue;
};
