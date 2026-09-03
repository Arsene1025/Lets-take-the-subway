// Copyright Epic Games, Inc. All Rights Reserved.

#include "Authoring/GridCellMarker.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	FColor MarkerColorForType(EGridCellType Type)
	{
		switch (Type)
		{
		case EGridCellType::Blocked:		return FColor(220, 50, 50);
		case EGridCellType::StageClear:		return FColor(40, 200, 220);
		case EGridCellType::Conditional:	return FColor(235, 200, 40);
		case EGridCellType::Walkable:		return FColor(60, 200, 90);
		default:							return FColor::White;
		}
	}
}

AGridCellMarkerBase::AGridCellMarkerBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetGenerateOverlapEvents(false);
	Box->SetBoxExtent(FVector(50.0, 50.0, 10.0));
	Box->ShapeColor = MarkerColorForType(CellType);
	Box->bHiddenInGame = true;
}

void AGridCellMarkerBase::BeginPlay()
{
	Super::BeginPlay();

	// bIsEditorOnlyActor keeps these out of a cooked build, but PIE duplicates the editor
	// level as-is. Destroying them here makes "markers never affect runtime" absolute
	// rather than a convention.
	if (UWorld* World = GetWorld())
	{
		if (World->IsGameWorld())
		{
			Destroy();
		}
	}
}

#if WITH_EDITOR

void AGridCellMarkerBase::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (!bFinished)
	{
		return;		// snapping mid-drag fights the gizmo
	}

	if (const AGridActor* Grid = AGridActor::FindGrid(GetWorld()))
	{
		SnapToGrid(*Grid);
	}

	NotifyGrid();
}

void AGridCellMarkerBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateVisual();
	NotifyGrid();
}

void AGridCellMarkerBase::PostEditUndo()
{
	Super::PostEditUndo();

	UpdateVisual();
	NotifyGrid();
}

void AGridCellMarkerBase::Destroyed()
{
	NotifyGrid();
	Super::Destroyed();
}

void AGridCellMarkerBase::SnapToGrid(const AGridActor& Grid)
{
	const FIntPoint Cell = Grid.WorldToCell(GetActorLocation());
	if (!Grid.IsValidCell(Cell))
	{
		return;		// dragged outside the grid; leave it where it is so it can be dragged back
	}

	const FVector Target = Grid.CellToWorld(Cell) + FVector(0.0, 0.0, 10.0);

	Modify();
	SetActorLocation(Target);
}

void AGridCellMarkerBase::NotifyGrid()
{
	UWorld* World = GetWorld();

	// Destroyed() also fires while a level is being torn down or garbage collected. Baking
	// then would iterate half-destroyed actors, so only react in a live editor world.
	if (!World || World->WorldType != EWorldType::Editor || World->bIsTearingDown || IsGarbageCollecting())
	{
		return;
	}

	for (TActorIterator<AGridActor> It(World); It; ++It)
	{
		It->OnMarkerChanged();
	}
}

void AGridCellMarkerBase::UpdateVisual()
{
	if (Box)
	{
		Box->ShapeColor = MarkerColorForType(CellType);
		Box->MarkRenderStateDirty();
	}
}

#endif	// WITH_EDITOR

// ---------------------------------------------------------------------------- Single cell

AGridCellMarker::AGridCellMarker()
{
	if (Box)
	{
		Box->SetBoxExtent(FVector(50.0, 50.0, 10.0));
	}
}

void AGridCellMarker::GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const
{
	const FIntPoint Cell = Grid.WorldToCell(GetActorLocation());
	if (Grid.IsValidCell(Cell))
	{
		OutCells.Add(Cell);
	}
}

// ---------------------------------------------------------------------------- Box of cells

AGridBoxMarker::AGridBoxMarker()
{
	if (Box)
	{
		Box->SetBoxExtent(FVector(SizeInCells.X * 50.0, SizeInCells.Y * 50.0, 10.0));
	}
}

FIntPoint AGridBoxMarker::GetMinCell(const AGridActor& Grid) const
{
	// The actor sits at the centre of the covered rectangle, so step back half of it to
	// find the min corner. The +0.25 cell nudge keeps an exactly-on-the-boundary centre
	// from floor()ing one cell too low.
	const FVector HalfSpan(
		SizeInCells.X * Grid.CellSize * 0.5,
		SizeInCells.Y * Grid.CellSize * 0.5,
		0.0);

	const FVector MinCorner = GetActorLocation() - HalfSpan + FVector(Grid.CellSize * 0.25, Grid.CellSize * 0.25, 0.0);
	return Grid.WorldToCell(MinCorner);
}

void AGridBoxMarker::GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const
{
	const FIntPoint MinCell = GetMinCell(Grid);

	for (int32 OffsetY = 0; OffsetY < SizeInCells.Y; ++OffsetY)
	{
		for (int32 OffsetX = 0; OffsetX < SizeInCells.X; ++OffsetX)
		{
			const FIntPoint Cell(MinCell.X + OffsetX, MinCell.Y + OffsetY);
			if (Grid.IsValidCell(Cell))
			{
				OutCells.Add(Cell);
			}
		}
	}
}

#if WITH_EDITOR

void AGridBoxMarker::SnapToGrid(const AGridActor& Grid)
{
	// Snap so the rectangle's edges land on cell borders: with an even cell count the
	// centre falls on a border, with an odd count it falls on a cell centre.
	const FIntPoint MinCell = GetMinCell(Grid);
	if (!Grid.IsValidCell(MinCell))
	{
		return;
	}

	const FVector Origin = Grid.GetGridOrigin();
	const FVector Target(
		Origin.X + (MinCell.X + SizeInCells.X * 0.5) * Grid.CellSize,
		Origin.Y + (MinCell.Y + SizeInCells.Y * 0.5) * Grid.CellSize,
		Grid.CellToWorld(MinCell).Z + 10.0);

	Modify();
	SetActorLocation(Target);
}

void AGridBoxMarker::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Box)
	{
		Box->SetBoxExtent(FVector(SizeInCells.X * 50.0, SizeInCells.Y * 50.0, 10.0));
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif	// WITH_EDITOR
