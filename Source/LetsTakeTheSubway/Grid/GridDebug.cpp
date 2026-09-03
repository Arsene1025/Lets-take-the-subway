// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/GridDebug.h"

#include "Grid/GridActor.h"
#include "Components/LineBatchComponent.h"
#include "Engine/World.h"

TAutoConsoleVariable<int32> CVarGridDebug(
	TEXT("ltts.GridDebug"),
	1,
	TEXT("Grid movement debug display.\n")
	TEXT("  0: off\n")
	TEXT("  1: HUD text only (default)\n")
	TEXT("  2: HUD text plus in-world path and hovered cell"),
	ECVF_Default);

namespace
{
	// Arbitrary but stable ids so each overlay can be cleared without touching the other.
	constexpr uint32 PathBatchID = 0x4C545301;
	constexpr uint32 HoverBatchID = 0x4C545302;

	ULineBatchComponent* GetBatcher(UWorld* World)
	{
		return World ? World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent) : nullptr;
	}
}

void FGridRuntimeDebugDrawer::DrawPath(UWorld* World, const AGridActor& Grid, FIntPoint CurrentCell, const TArray<FIntPoint>& Path)
{
#if ENABLE_DRAW_DEBUG
	ULineBatchComponent* Batcher = GetBatcher(World);
	if (!Batcher)
	{
		return;
	}

	Batcher->ClearBatch(PathBatchID);
	if (Path.IsEmpty())
	{
		return;
	}

	const double HalfSize = Grid.CellSize * 0.5 - 6.0;
	const FVector BoxExtent(HalfSize, HalfSize, 2.0);

	FVector Previous = Grid.CellToWorld(CurrentCell) + FVector(0.0, 0.0, 6.0);

	for (int32 StepIndex = 0; StepIndex < Path.Num(); ++StepIndex)
	{
		const FVector Centre = Grid.CellToWorld(Path[StepIndex]) + FVector(0.0, 0.0, 6.0);
		const bool bIsGoal = (StepIndex == Path.Num() - 1);
		const FLinearColor Color = bIsGoal ? FLinearColor(0.1f, 1.0f, 0.3f) : FLinearColor(0.2f, 0.7f, 1.0f);

		Batcher->DrawBox(Centre, BoxExtent, Color, -1.0f, SDPG_World, bIsGoal ? 4.0f : 2.0f, PathBatchID);
		Batcher->DrawLine(Previous, Centre, Color, SDPG_World, 3.0f, -1.0f, PathBatchID);

		Previous = Centre;
	}
#endif
}

void FGridRuntimeDebugDrawer::ClearPath(UWorld* World)
{
#if ENABLE_DRAW_DEBUG
	if (ULineBatchComponent* Batcher = GetBatcher(World))
	{
		Batcher->ClearBatch(PathBatchID);
	}
#endif
}

void FGridRuntimeDebugDrawer::DrawHover(UWorld* World, const AGridActor& Grid, FIntPoint Cell, bool bEnterable)
{
#if ENABLE_DRAW_DEBUG
	ULineBatchComponent* Batcher = GetBatcher(World);
	if (!Batcher)
	{
		return;
	}

	Batcher->ClearBatch(HoverBatchID);

	if (!Grid.IsValidCell(Cell))
	{
		return;
	}

	const double HalfSize = Grid.CellSize * 0.5;
	const FVector Centre = Grid.CellToWorld(Cell) + FVector(0.0, 0.0, 8.0);
	const FLinearColor Color = bEnterable ? FLinearColor(0.2f, 1.0f, 0.2f) : FLinearColor(1.0f, 0.2f, 0.2f);

	Batcher->DrawBox(Centre, FVector(HalfSize, HalfSize, 3.0), Color, -1.0f, SDPG_World, 3.0f, HoverBatchID);
#endif
}

void FGridRuntimeDebugDrawer::ClearHover(UWorld* World)
{
#if ENABLE_DRAW_DEBUG
	if (ULineBatchComponent* Batcher = GetBatcher(World))
	{
		Batcher->ClearBatch(HoverBatchID);
	}
#endif
}

void FGridRuntimeDebugDrawer::ClearAll(UWorld* World)
{
	ClearPath(World);
	ClearHover(World);
}
