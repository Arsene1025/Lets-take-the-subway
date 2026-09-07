// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Puzzle/PuzzleBlock.h"
#include "PuzzleRotatingObstacle.generated.h"

class APuzzleLever;

/**
 * A large rectangular structure with a channel cut through it, turned a quarter at a time by
 * a lever.
 *
 * The channel is the point of the piece. Its cells are left unclaimed, so ordinary blocks can
 * be pushed inside and the pawn can walk in; anything resting against one of the channel's
 * inner faces is carried around with the structure when it turns. That is the design's
 * hatched face: contact with the wall is what decides whether a block travels, not merely
 * being somewhere in the opening.
 *
 * Nothing about it slides. It is derived from APuzzleBlock for the footprint placement,
 * occupancy bookkeeping and rotation animation, with MoveAxis pinned to Immovable.
 *
 * The footprint's two sides must share a parity (6x4 or 7x3, never 6x3). A rectangle whose
 * sides differ in parity has its centre half a cell off the grid, and a block turned about
 * that centre would land half a cell out of alignment -- from which point it could no longer
 * be pushed, claim cells, or be walked around.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleRotatingObstacle : public APuzzleBlock
{
	GENERATED_BODY()

public:
	APuzzleRotatingObstacle();

	// ---------------------------------------------------------------- Authoring

	/** Low corner of the channel within the footprint, in the structure's own frame. */
	UPROPERTY(EditAnywhere, Category = "Rotating Obstacle", meta = (ClampMin = 0))
	FIntPoint ChannelOffset = FIntPoint(1, 1);

	/** Channel size in cells. Reaching an edge of the footprint leaves that end open. */
	UPROPERTY(EditAnywhere, Category = "Rotating Obstacle", meta = (ClampMin = 1))
	FIntPoint ChannelSize = FIntPoint(5, 2);

	/** Seconds for one quarter turn. Slower than a plain block, because the thing is huge. */
	UPROPERTY(EditAnywhere, Category = "Rotating Obstacle", meta = (ClampMin = 0.05))
	float RotateDuration = 0.6f;

	/** Height of the hatched faces inside the channel, as a fraction of the body height. */
	UPROPERTY(EditAnywhere, Category = "Rotating Obstacle", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float InnerFaceHeightRatio = 0.7f;

	// ---------------------------------------------------------------- Queries

	/** The channel as it lies on the grid right now. */
	FGridRect GetWorldChannelRect() const;

	/**
	 * The square the structure turns inside.
	 *
	 * Side N is the footprint's longer edge, centred on the footprint. The parity rule is
	 * what makes this square land on cell boundaries whichever way the piece is facing.
	 */
	FGridRect GetRegion() const;

	/** World position of the turning axis, at floor height. */
	FVector GetPivotWorld() const;

	bool IsChannelCell(FIntPoint Cell) const { return GetWorldChannelRect().Contains(Cell); }

	/** Part of the solid body: inside the footprint but not inside the channel. */
	bool IsSolidCell(FIntPoint Cell) const;

	/**
	 * True when the block sits in the channel with at least one cell against an inner face.
	 *
	 * A block floating in the middle of a wide channel is not attached, so it does not
	 * travel -- and because it is then standing in the way, it stops the turn instead.
	 */
	bool IsAttachedToInnerFace(const APuzzleBlock& Block) const;

	/** Every block that would be carried round by a turn. Elevators are never included. */
	void GatherRiders(TArray<APuzzleBlock*>& OutRiders) const;

	// ---------------------------------------------------------------- Rotation

	/**
	 * Turn a quarter, or explain why it cannot turn.
	 *
	 * TurnSign +1 is a yaw of +90 degrees, which reads as clockwise from above. Refuses when
	 * anything that is not coming along stands in the area the structure sweeps, when the
	 * destination is not floor, or when the pawn is in the way and has nowhere to be pushed.
	 */
	bool TryRotate(int32 TurnSign, FText* OutReason = nullptr);

	// ---------------------------------------------------------------- Lifecycle

	virtual void GatherOccupiedCells(TArray<FIntPoint>& OutCells) const override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

protected:
	virtual void RefreshVisual() override;
	virtual void RegisterWithSubsystem(UPuzzleSubsystem& Subsystem) override;
	virtual void UnregisterFromSubsystem(UPuzzleSubsystem& Subsystem) override;

private:
	/** Force the parity rule and keep the channel inside the footprint. */
	void NormaliseAuthoring();

	/**
	 * Where a cell of the authored footprint lies within the footprint as it sits on the
	 * grid, accounting for the quarter turns taken so far.
	 */
	FIntPoint LocalToWorldOffset(FIntPoint Local) const;

	/**
	 * Cells the moving parts pass through on the way round.
	 *
	 * Each moving cell is reduced to the ring of radii and the wedge of angles it covers
	 * about the pivot; sweeping is then just widening that wedge by ninety degrees. Testing
	 * a candidate cell against those two ranges is exact enough to keep the clearance a
	 * designer has to leave down to what the corners genuinely need, which a plain
	 * bounding circle would not.
	 */
	void GatherSweptCells(const TArray<FIntPoint>& MovingCells, int32 TurnSign, TSet<FIntPoint>& OutCells) const;

	/** Nearest cell the pawn can be pushed to that the turn will not touch. */
	bool FindPawnRefuge(FIntPoint From, const TSet<FIntPoint>& Swept, const TSet<FIntPoint>& Destinations, FIntPoint& OutCell) const;

	/** The solid body, drawn as up to four slabs around the channel. */
	UPROPERTY(VisibleAnywhere, Category = "Rotating Obstacle")
	TArray<TObjectPtr<UStaticMeshComponent>> SlabMeshes;

	/** Thin panels on the channel's inner walls: the hatched faces from the design. */
	UPROPERTY(VisibleAnywhere, Category = "Rotating Obstacle")
	TArray<TObjectPtr<UStaticMeshComponent>> InnerFaceMeshes;
};
