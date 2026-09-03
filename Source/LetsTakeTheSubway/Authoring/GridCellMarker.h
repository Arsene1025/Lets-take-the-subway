// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridTypes.h"
#include "GridCellMarker.generated.h"

class AGridActor;
class UBoxComponent;
class UGridCellRule;

/**
 * Editor-only override placed on top of the auto-generated grid.
 *
 * Drop one in the level, drag it where you want it (it snaps to the nearest cell) and pick
 * a cell type. The grid re-bakes its overrides as soon as the marker moves or changes, so
 * the viewport preview stays truthful.
 *
 * Markers are stripped on cook. What survives is the baked FGridCellOverride array on the
 * grid actor, plus a copy of any rule object -- see AGridActor::BakeOverridesFromMarkers.
 */
UCLASS(Abstract, HideCategories = (Rendering, Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, WorldPartition, Replication))
class LETSTAKETHESUBWAY_API AGridCellMarkerBase : public AActor
{
	GENERATED_BODY()

public:
	AGridCellMarkerBase();

	/** What the covered cells become. NoFloor is a generated state and cannot be authored. */
	UPROPERTY(EditAnywhere, Category = "Grid Override", meta = (InvalidEnumValues = "NoFloor"))
	EGridCellType CellType = EGridCellType::Blocked;

	/** Runtime entry test. Only meaningful for Conditional cells. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Grid Override",
		meta = (EditCondition = "CellType == EGridCellType::Conditional", EditConditionHides))
	TObjectPtr<UGridCellRule> Rule;

	/** When markers overlap, the higher priority wins. */
	UPROPERTY(EditAnywhere, Category = "Grid Override")
	int32 Priority = 0;

	/** Cells this marker covers, in grid coordinates. */
	virtual void GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const PURE_VIRTUAL(AGridCellMarkerBase::GatherCells, );

	/** Single-cell markers win ties against box markers -- a small fix on a broad region. */
	virtual bool IsSingleCellMarker() const { return true; }

	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void Destroyed() override;

	/** Move onto the grid so the covered cells are unambiguous. */
	virtual void SnapToGrid(const AGridActor& Grid);

	/** Tell every grid in the level to re-bake. */
	void NotifyGrid();

	/** Recolour the box to match CellType. */
	void UpdateVisual();
#endif

protected:
	UPROPERTY(VisibleAnywhere, Category = "Grid Override")
	TObjectPtr<UBoxComponent> Box;
};

/** Overrides exactly the cell under the actor. */
UCLASS(meta = (DisplayName = "Grid Cell Marker"))
class LETSTAKETHESUBWAY_API AGridCellMarker : public AGridCellMarkerBase
{
	GENERATED_BODY()

public:
	AGridCellMarker();

	virtual void GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const override;
};

/**
 * Overrides a rectangle of cells.
 *
 * Size is in cells rather than world scale on purpose: a scaled box has float edges, which
 * makes "is this cell half covered?" a judgement call and the bake non-deterministic.
 */
UCLASS(meta = (DisplayName = "Grid Box Marker"))
class LETSTAKETHESUBWAY_API AGridBoxMarker : public AGridCellMarkerBase
{
	GENERATED_BODY()

public:
	AGridBoxMarker();

	UPROPERTY(EditAnywhere, Category = "Grid Override", meta = (ClampMin = 1, ClampMax = 512))
	FIntPoint SizeInCells = FIntPoint(3, 3);

	virtual void GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const override;
	virtual bool IsSingleCellMarker() const override { return false; }

#if WITH_EDITOR
	virtual void SnapToGrid(const AGridActor& Grid) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Min-corner cell of the covered rectangle, derived from the actor location. */
	FIntPoint GetMinCell(const AGridActor& Grid) const;
};
