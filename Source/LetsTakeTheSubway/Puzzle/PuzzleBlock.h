// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridFootprint.h"
#include "Grid/GridTypes.h"
#include "Puzzle/PuzzleTypes.h"
#include "PuzzleBlock.generated.h"

class AGridActor;
class UStaticMeshComponent;

/**
 * A Rush Hour piece: a rectangle of cells the player pushes around the floor.
 *
 * The block owns no cells of its own. It claims them on the grid actor's runtime occupancy
 * map, which is what makes the pawn path around it and refuse to walk into it -- the baked
 * cell data is never touched, so the level's walkable floor stays exactly as the designer
 * generated it no matter where the blocks end up.
 *
 * The actor sits at the centre of its footprint, matching AGridBoxMarker, so a rotation is
 * just "swing the location about the tile centre and add 90 degrees of yaw".
 *
 * FootprintSize, MoveAxis and (on the elevator) the door are all authored in the block's own
 * local frame. The world-space versions are derived from QuarterTurns, which comes from the
 * actor's yaw. That way rotating a block in the editor gizmo and rotating it at runtime on a
 * tile go through exactly the same code.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleBlock : public AActor
{
	GENERATED_BODY()

public:
	APuzzleBlock();

	/** What the block is doing visually. Occupancy is always already at the final state. */
	enum class EAnimState : uint8
	{
		Idle,
		Sliding,
		Rotating
	};

	// ---------------------------------------------------------------- Authoring

	/** Footprint in cells, in the block's own frame. A 1x3 laid east-west is authored 1x3 and yawed 90. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 1, ClampMax = 16))
	FIntPoint FootprintSize = FIntPoint(1, 3);

	/**
	 * Which way the block may be pushed, in the block's own frame.
	 *
	 * Free on both axes by default, matching the design's "slides in all four directions"
	 * basic object. Pin it to one axis for a piece that should behave like a Rush Hour car.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block")
	EPuzzleMoveAxis MoveAxis = EPuzzleMoveAxis::Both;

	/** Visual height in cm. Does not affect the puzzle -- the grid is two-dimensional. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 10.0))
	float Height = 150.0f;

	/** Slide speed in cm/s. One 1 m cell at 600 takes about a sixth of a second. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 1.0))
	float SlideSpeed = 600.0f;

	// --- CUTAWAY DISABLED 2026-09-04 -------------------------------------------------
	// Flattening a block that hides the pawn is switched off for now. The code is kept
	// rather than deleted so it can be turned back on: search for this marker, restore
	// every guarded block, and put GetVisualHeight() back into the two RefreshVisual
	// overrides. See section 9 of Docs/Plans/RushHourPuzzle.md for why the occlusion
	// test must stay analytic if it is revived.
#if 0
	/** Height the block shrinks to while it is hiding the pawn. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block|Cutaway", meta = (ClampMin = 1.0))
	float CutawaySlabHeight = 20.0f;

	/** Seconds to press the block down, and to let it back up. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block|Cutaway", meta = (ClampMin = 0.01))
	float CutawayBlendTime = 0.15f;
#endif

	// ---------------------------------------------------------------- Queries

	AGridActor* GetGrid() const { return Grid; }

	/** Quarter turns of yaw away from the authored frame, 0 to 3. */
	int32 GetQuarterTurns() const { return QuarterTurns; }

	/** Footprint as it lies on the grid: the authored size, swapped on an odd quarter turn. */
	FIntPoint GetWorldFootprint() const;

	FGridRect GetRect() const { return FGridRect(MinCell, GetWorldFootprint()); }

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	/**
	 * The volume the block would fill at its authored height, whatever it is drawn at now.
	 *
	 * Deliberately independent of the cutaway: the occlusion test that decides whether to
	 * flatten a block reads this, and if it read the current height instead, a flattened
	 * block would stop occluding, stand up, occlude again, and oscillate every frame.
	 */
	FBox GetFullBounds() const;
#endif

	/** Which way this block may be pushed right now, after any rotation. */
	virtual EPuzzleMoveAxis GetWorldMoveAxis() const;

	bool IsAnimating() const { return AnimState != EAnimState::Idle; }

	bool IsHeld() const { return bHeld; }

	// ---------------------------------------------------------------- Cutaway
	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	/** Ask the block to flatten (or to stand back up). Blended, so repeated calls are cheap. */
	void SetCutaway(bool bInCutaway);

	bool IsCutaway() const { return bCutawayTarget; }

	/**
	 * Height the body is drawn at right now.
	 *
	 * The collision travels with the mesh, so a flattened block is genuinely a slab: its top
	 * face is still up and still grabbable, just lower down.
	 */
	float GetVisualHeight() const { return FMath::Lerp(Height, CutawaySlabHeight, CutawayAlpha); }
#endif

	/** Height of the floor the block stands on. Captured once, at BeginPlay. */
	double GetFloorZ() const { return FloorZ; }

	// ---------------------------------------------------------------- Movement

	/** True when the block could take one step that way right now. */
	bool CanSlide(EGridDirection Dir, FText* OutReason = nullptr) const;

	/** Begin one cell of travel. Cells are claimed immediately, so nothing else can take them mid-step. */
	bool StartSlide(EGridDirection Dir);

	/**
	 * Mark the block as grabbed by the cursor.
	 *
	 * Rotation is checked when a held block is let go and finishes its last step, not on
	 * every step arrival: otherwise dragging a block across a rotation tile would spin it
	 * out from under the cursor halfway through the drag.
	 */
	void SetHeld(bool bInHeld);

	/**
	 * Start the visual half of a rotation. The caller has already decided the outcome and
	 * passes in the destination rectangle.
	 *
	 * Occupancy, MinCell and QuarterTurns are committed here rather than when the animation
	 * ends, so any query during the spin already sees the final layout and nothing can move
	 * into a cell the block is on its way to.
	 */
	void BeginRotation(const FVector& Pivot, int32 TurnSign, float Duration, const FGridRect& NewRect);

	// ---------------------------------------------------------------- Lifecycle

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** Resize and recolour the body. Called whenever the footprint or height changes. */
	virtual void RefreshVisual();

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Block")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Block")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Min corner of the occupied rectangle, in grid cells. */
	FIntPoint MinCell = FIntPoint::ZeroValue;

	int32 QuarterTurns = 0;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

private:
	/** Claim the current rectangle on the grid, releasing whatever was held before. */
	void ClaimCells();

	/** Move the actor onto the exact centre of its current rectangle. */
	void SnapToRect();

	/** Tell the subsystem the block has settled, so a rotation tile can react. */
	void ReportAtRest();

	EAnimState AnimState = EAnimState::Idle;

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	bool bCutawayTarget = false;

	/** 0 at full height, 1 fully flattened. */
	float CutawayAlpha = 0.0f;
#endif

	double FloorZ = 0.0;

	bool bHeld = false;

	/** Steps taken since the block was grabbed. Zero at release means it was a click, not a drag. */
	int32 StepsWhileHeld = 0;

	FVector SlideTarget = FVector::ZeroVector;

	FVector RotationPivot = FVector::ZeroVector;
	FVector RotationStartLocation = FVector::ZeroVector;
	FVector RotationTargetLocation = FVector::ZeroVector;
	double RotationStartYaw = 0.0;
	double RotationTargetYaw = 0.0;
	int32 RotationTurnSign = 1;
	float RotationDuration = 0.4f;
	float RotationElapsed = 0.0f;
};
