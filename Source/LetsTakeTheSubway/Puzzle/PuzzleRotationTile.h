// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridFootprint.h"
#include "PuzzleRotationTile.generated.h"

class AGridActor;
class APuzzleBlock;
class UStaticMeshComponent;

/**
 * A square patch of floor that turns whatever is standing on it a quarter turn.
 *
 * The design calls this "rotating the space", and that is literally what happens: every
 * block sitting entirely within the region is carried around the region's centre together,
 * so their relative arrangement is preserved and no two of them can collide. The elevator's
 * value here is that its door comes out facing a new direction.
 *
 * A block that only half overlaps the region has no sensible destination, so the rotation is
 * refused outright and the player is told why rather than having a piece shoved aside.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleRotationTile : public AActor
{
	GENERATED_BODY()

public:
	APuzzleRotationTile();

	/** Side length in cells. Square, because a rectangle could not hold its own contents after a turn. */
	UPROPERTY(EditAnywhere, Category = "Rotation Tile", meta = (ClampMin = 2, ClampMax = 16))
	int32 SizeInCells = 4;

	/** Turn direction seen from above. Clockwise matches the design document's diagram. */
	UPROPERTY(EditAnywhere, Category = "Rotation Tile")
	bool bClockwise = true;

	UPROPERTY(EditAnywhere, Category = "Rotation Tile", meta = (ClampMin = 0.05))
	float RotateDuration = 0.4f;

	FGridRect GetRegion() const { return Region; }

	bool IsRotating() const { return bRotating; }

	/** True when every cell of the block lies inside the region. */
	bool FullyContains(const APuzzleBlock& Block) const;

	/** True when the block covers some of the region but hangs over its edge. */
	bool Straddles(const APuzzleBlock& Block) const;

	/**
	 * Turn the space, or explain why it cannot be turned.
	 *
	 * Refuses when a block straddles the border, when a rotated block would land on
	 * something that is not walkable floor, or when the pawn standing inside would be moved
	 * somewhere it cannot stand.
	 */
	bool TryRotate(FText* OutReason = nullptr);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void RefreshVisual();

	UPROPERTY(VisibleAnywhere, Category = "Rotation Tile")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Flat pad marking the region. Carries no collision so click and floor traces pass through it. */
	UPROPERTY(VisibleAnywhere, Category = "Rotation Tile")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	/** Sits on one corner so the turn direction is readable at a glance in the level. */
	UPROPERTY(VisibleAnywhere, Category = "Rotation Tile")
	TObjectPtr<UStaticMeshComponent> CornerMesh;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	FGridRect Region;

	FVector PivotWorld = FVector::ZeroVector;

	bool bRotating = false;
	bool bDisabled = false;
	float RotationElapsed = 0.0f;

	/** Where the pawn ends up, applied once the spin finishes so it travels with the space. */
	TOptional<FIntPoint> PendingPawnCell;
};
