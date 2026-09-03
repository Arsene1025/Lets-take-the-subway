// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "Grid/GridTypes.h"
#include "GridActor.generated.h"

class UGridCellRule;
class UGridDebugDrawComponent;
class AGridCellMarkerBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGridStageClear, APawn*, Pawn, FIntPoint, Cell);

/**
 * Owns the walkable grid: generates it by tracing the level, merges the designer's marker
 * overrides on top, and answers movement queries at runtime.
 *
 * The actor's location is the grid origin (cell (0,0) starts there and the grid extends
 * along +X/+Y). Rotation and scale are deliberately ignored -- a rotated grid would make
 * "1m cell" ambiguous and every world/cell conversion more expensive for no gameplay gain.
 *
 * Generated data is serialized into the map, so a packaged build performs no traces unless
 * bRegenerateOnPlay is set.
 */
UCLASS(HideCategories = (Rendering, Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, WorldPartition, Replication))
class LETSTAKETHESUBWAY_API AGridActor : public AActor
{
	GENERATED_BODY()

public:
	AGridActor();

	// ---------------------------------------------------------------- Generation settings

	/** Grid extent in cells along +X and +Y from the actor location. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = 1, ClampMax = 512))
	FIntPoint SizeInCells = FIntPoint(80, 80);

	/** One cell is 1 m. Fixed by design; shown here so the value is discoverable. */
	UPROPERTY(VisibleAnywhere, Category = "Grid")
	float CellSize = 100.0f;

	/** Floor traces run from ActorZ + RegionHeight down to ActorZ. Keep it tight: anything above the region is hit first. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = 1.0))
	float RegionHeight = 1000.0f;

	/** Two neighbouring cells connect only when their floors differ by no more than this. Sets stair riser tolerance. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = 0.0))
	float MaxStepHeight = 50.0f;

	/** Floors steeper than this are Blocked/Slope. Ramps below it stay walkable. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = 0.0, ClampMax = 89.0))
	float MaxSlopeAngle = 35.0f;

	/** Sweep a box above each floor to reject cells buried in walls, pillars or under low ceilings. */
	UPROPERTY(EditAnywhere, Category = "Grid")
	bool bClearanceTest = true;

	UPROPERTY(EditAnywhere, Category = "Grid", meta = (EditCondition = "bClearanceTest", ClampMin = 0.0))
	float ClearanceHeight = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Grid", meta = (EditCondition = "bClearanceTest", ClampMin = 1.0))
	float ClearanceHalfWidth = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Grid")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Re-trace on play instead of trusting the serialized data. Stored overrides are re-applied either way. */
	UPROPERTY(EditAnywhere, Category = "Grid")
	bool bRegenerateOnPlay = false;

	/** Regenerate automatically when a generation setting or the actor location changes. */
	UPROPERTY(EditAnywhere, Category = "Grid|Editor")
	bool bAutoRegenerateOnEdit = true;

	/** Re-bake overrides as soon as a marker is moved, edited or deleted. */
	UPROPERTY(EditAnywhere, Category = "Grid|Editor")
	bool bLiveApplyMarkerOverrides = true;

	// ---------------------------------------------------------------- Debug settings

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	bool bDrawGridInEditor = true;

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	bool bDrawCellCoords = false;

	/** Coordinate labels beyond this distance are clipped. */
	UPROPERTY(EditAnywhere, Category = "Grid|Debug", meta = (ClampMin = 0.0))
	float CoordLabelMaxDistance = 2500.0f;

	/** Hard cap on label count -- each label is a canvas draw. */
	UPROPERTY(EditAnywhere, Category = "Grid|Debug", meta = (ClampMin = 0))
	int32 CoordLabelMaxCount = 1500;

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	bool bDrawNoFloorCells = false;

	/** Mark cell borders where the step-height rule broke the connection. Useful for tuning stairs. */
	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	bool bDrawStepBreaks = true;

	// ---------------------------------------------------------------- Statistics (read only)

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumWalkable = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumBlocked = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumNoFloor = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumStageClear = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumConditional = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumBlockedBySlope = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumBlockedByClearance = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumOverridesApplied = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumStepBreaks = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	FString LastGenerated;

	// ---------------------------------------------------------------- Data

	UPROPERTY()
	TArray<FGridCellData> Cells;

	/** Baked from the level's markers. Survives cook; the markers themselves do not. */
	UPROPERTY()
	TArray<FGridCellOverride> Overrides;

	/** Copies of the marker rules, owned by this actor so they survive cook. */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Grid|Stats")
	TArray<TObjectPtr<UGridCellRule>> ConditionalRules;

	// ---------------------------------------------------------------- Events

	UPROPERTY(BlueprintAssignable, Category = "Grid")
	FOnGridStageClear OnStageClear;

	// ---------------------------------------------------------------- Editor actions

	/** Trace the level, bake marker overrides and apply them. */
	UFUNCTION(CallInEditor, Category = "Grid", meta = (DisplayName = "Generate Grid"))
	void GenerateGrid();

	/** Re-bake and re-apply marker overrides without re-tracing. Cheap. */
	UFUNCTION(CallInEditor, Category = "Grid", meta = (DisplayName = "Apply Marker Overrides"))
	void ApplyOverrides();

	UFUNCTION(CallInEditor, Category = "Grid", meta = (DisplayName = "Clear Grid"))
	void ClearGrid();

	/** Log one cell's generated state and a test path, for tuning stairs and slopes. */
	UFUNCTION(CallInEditor, Category = "Grid|Debug", meta = (DisplayName = "Log Debug Report"))
	void LogDebugReport();

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	FIntPoint DebugInspectCell = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	FIntPoint DebugPathStart = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	FIntPoint DebugPathGoal = FIntPoint(79, 79);

	// ---------------------------------------------------------------- Queries

	UFUNCTION(BlueprintPure, Category = "Grid")
	bool IsValidCell(FIntPoint Cell) const
	{
		return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < SizeInCells.X && Cell.Y < SizeInCells.Y;
	}

	int32 CellToIndex(FIntPoint Cell) const { return Cell.Y * SizeInCells.X + Cell.X; }
	FIntPoint IndexToCell(int32 Index) const { return FIntPoint(Index % SizeInCells.X, Index / SizeInCells.X); }

	const FGridCellData* GetCell(FIntPoint Cell) const
	{
		const int32 Index = CellToIndex(Cell);
		return (IsValidCell(Cell) && Cells.IsValidIndex(Index)) ? &Cells[Index] : nullptr;
	}

	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GetGridOrigin() const { return GetActorLocation(); }

	/** Cell containing a world position. May be out of range -- test with IsValidCell. */
	UFUNCTION(BlueprintPure, Category = "Grid")
	FIntPoint WorldToCell(const FVector& World) const;

	/** Centre of a cell at floor height. Falls back to the grid origin Z when the cell has no floor. */
	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector CellToWorld(FIntPoint Cell) const;

	/** Walkable ignoring runtime rules: Walkable, StageClear or Conditional. */
	UFUNCTION(BlueprintPure, Category = "Grid")
	bool IsCellWalkableStatic(FIntPoint Cell) const;

	/** Static walkability plus the Conditional rule, if any. */
	bool CanPawnEnter(FIntPoint Cell, const APawn* Pawn, FText* OutDeniedMessage = nullptr) const;

	/** Human-readable reason a cell cannot be entered. */
	FText DescribeCell(FIntPoint Cell, const APawn* Pawn) const;

	/** Expanding ring search for a walkable cell. Used to place the pawn at spawn. */
	bool FindNearestWalkableCell(FIntPoint From, int32 MaxRadius, const APawn* Pawn, FIntPoint& OutCell) const;

	/** A* over the grid. OutPath excludes Start and ends with Goal. */
	bool FindPath(FIntPoint Start, FIntPoint Goal, const APawn* Pawn, TArray<FIntPoint>& OutPath) const;

	/** Called by the pawn on arrival. Fires OnStageClear for StageClear cells. */
	void NotifyPawnEnteredCell(APawn* Pawn, FIntPoint Cell);

	/** First grid actor in the world, or null. */
	static AGridActor* FindGrid(const UWorld* World);

	// ---------------------------------------------------------------- Lifecycle

	virtual void PostInitializeComponents() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;

	/** Called by markers when they move, change or are deleted. */
	void OnMarkerChanged();
#endif

private:
	/** Fills Cells purely from traces. Knows nothing about markers. */
	void GenerateFromTraces();

	/** Second pass: connect 4-neighbours whose floors are within MaxStepHeight. */
	void BuildAdjacency();

	/** Stamps the serialized Overrides onto Cells, in array order (later wins). */
	void ApplyStoredOverrides();

	void RecomputeStats();
	void RefreshDebugDraw();

#if WITH_EDITOR
	/** Collects markers into Overrides + ConditionalRules. Deterministic ordering. */
	void BakeOverridesFromMarkers();

	/** Guards against markers re-entering while we are already re-baking. */
	bool bIsApplyingOverrides = false;
#endif

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UGridDebugDrawComponent> DebugDrawComponent;
};
