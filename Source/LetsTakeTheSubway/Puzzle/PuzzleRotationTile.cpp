// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleRotationTile.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APuzzleRotationTile::APuzzleRotationTile()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(SceneRoot);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PadMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PadMesh->SetGenerateOverlapEvents(false);

	CornerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CornerMesh"));
	CornerMesh->SetupAttachment(SceneRoot);
	CornerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CornerMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CornerMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		PadMesh->SetStaticMesh(CubeFinder.Object);
		CornerMesh->SetStaticMesh(CubeFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	if (MaterialFinder.Succeeded())
	{
		PadMesh->SetMaterial(0, MaterialFinder.Object);
		CornerMesh->SetMaterial(0, MaterialFinder.Object);
	}

	// The pad is decoration lying on the floor; the grid must not trace it as geometry.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

	// Authoring data the cooker reads; editor builds only.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void APuzzleRotationTile::RefreshVisual()
{
	const double CellSize = Grid ? Grid->CellSize : 100.0;
	const double Span = SizeInCells * CellSize / 100.0;

	// The editor grid overlay draws its cell quads 2 cm above the floor, so a pad lying flat
	// on the floor is coplanar with them and tears apart in the viewport. Sit clear of both.
	if (PadMesh)
	{
		PadMesh->SetRelativeLocation(FVector(0.0, 0.0, 5.0));
		PadMesh->SetRelativeScale3D(FVector(Span, Span, 0.04));
	}

	if (CornerMesh)
	{
		const double Inset = (SizeInCells - 1) * CellSize * 0.5;
		CornerMesh->SetRelativeLocation(FVector(Inset, Inset, 14.0));
		CornerMesh->SetRelativeScale3D(FVector(CellSize / 200.0, CellSize / 200.0, 0.12));
	}
}

void APuzzleRotationTile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
}

void APuzzleRotationTile::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; the tile is disabled."), *GetName());
		bDisabled = true;
		return;
	}

	const FIntPoint Size(SizeInCells, SizeInCells);
	Region = FGridRect(GridFootprint::MinCellFromCentre(*Grid, GetActorLocation(), Size), Size);

	TArray<FIntPoint> Cells;
	Region.GatherCells(Cells);

	for (const FIntPoint& Cell : Cells)
	{
		if (!Grid->IsValidCell(Cell))
		{
			UE_LOG(LogLTTSGrid, Error,
				TEXT("%s: cell (%d,%d) is outside the grid; the tile is disabled."), *GetName(), Cell.X, Cell.Y);
			bDisabled = true;
			return;
		}

		if (!Grid->IsCellWalkableStatic(Cell))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: cell (%d,%d) is not walkable floor; rotations landing there will be refused."),
				*GetName(), Cell.X, Cell.Y);
		}
	}

	// Even side length, so the centre of the region falls on a cell corner -- which is
	// exactly the pivot every rotated footprint has to be measured from.
	const FVector Origin = Grid->GetGridOrigin();
	PivotWorld = FVector(
		Origin.X + (Region.Min.X + SizeInCells * 0.5) * Grid->CellSize,
		Origin.Y + (Region.Min.Y + SizeInCells * 0.5) * Grid->CellSize,
		Grid->CellToWorld(Region.Min).Z);

	SetActorLocation(FVector(PivotWorld.X, PivotWorld.Y, PivotWorld.Z));
	RefreshVisual();

	// Overlapping tiles would both claim a block that came to rest between them, and the
	// order they were registered in would decide which one won.
	for (TActorIterator<APuzzleRotationTile> It(GetWorld()); It; ++It)
	{
		const APuzzleRotationTile* Other = *It;
		if (Other && Other != this && !Other->bDisabled && Other->Region.Overlaps(Region))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: overlaps rotation tile %s. Tiles must be disjoint."), *GetName(), *Other->GetName());
		}
	}

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->RegisterTile(this);
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %dx%d rotation tile at cell (%d,%d), %s."),
		*GetName(), SizeInCells, SizeInCells, Region.Min.X, Region.Min.Y,
		bClockwise ? TEXT("clockwise") : TEXT("counter-clockwise"));
}

void APuzzleRotationTile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->UnregisterTile(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- Containment

bool APuzzleRotationTile::FullyContains(const APuzzleBlock& Block) const
{
	return !bDisabled && Region.ContainsRect(Block.GetRect());
}

bool APuzzleRotationTile::Straddles(const APuzzleBlock& Block) const
{
	if (bDisabled)
	{
		return false;
	}

	const FGridRect BlockRect = Block.GetRect();
	return Region.Overlaps(BlockRect) && !Region.ContainsRect(BlockRect);
}

// ---------------------------------------------------------------------------- Rotation

bool APuzzleRotationTile::TryRotate(FText* OutReason)
{
	if (bDisabled || bRotating || !Grid)
	{
		return false;
	}

	UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
	if (!Subsystem)
	{
		return false;
	}

	const int32 TurnSign = bClockwise ? 1 : -1;

	// Pass one: nothing is committed until every piece is known to fit. A rotation that
	// failed halfway would leave the puzzle in a state the player could not have reached.
	TArray<APuzzleBlock*> Inside;
	TArray<FGridRect> Destinations;

	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Subsystem->GetBlocks())
	{
		APuzzleBlock* Block = Entry.Get();
		if (!Block)
		{
			continue;
		}

		if (Straddles(*Block))
		{
			if (OutReason)
			{
				*OutReason = FText::Format(
					NSLOCTEXT("LTTSPuzzle", "TileStraddled", "Cannot rotate: {0} is half on the tile."),
					FText::FromString(Block->GetName()));
			}
			return false;
		}

		if (!FullyContains(*Block))
		{
			continue;
		}

		const FGridRect Destination = GridFootprint::RotateRect(Block->GetRect(), Region, TurnSign);

		TArray<FIntPoint> Cells;
		Destination.GatherCells(Cells);
		for (const FIntPoint& Cell : Cells)
		{
			if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
			{
				if (OutReason)
				{
					*OutReason = NSLOCTEXT("LTTSPuzzle", "TileNoFloor", "Cannot rotate: there is no floor to turn into.");
				}
				return false;
			}
		}

		Inside.Add(Block);
		Destinations.Add(Destination);
	}

	if (Inside.IsEmpty())
	{
		return false;
	}

	// The pawn is part of the space too. It is stopped first, because a pawn caught between
	// two cells has no single cell to carry around the pivot.
	AGridPawn* Pawn = Subsystem->GetGridPawn();
	PendingPawnCell.Reset();

	if (Pawn && Region.Contains(Pawn->GetCurrentCell()))
	{
		const FIntPoint Local = Pawn->GetCurrentCell() - Region.Min;
		const FIntPoint Rotated = Region.Min + GridFootprint::RotateLocalCell(Local, SizeInCells, TurnSign);

		if (!Grid->IsCellWalkableStatic(Rotated))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "TilePawnNoFloor", "Cannot rotate: you would end up off the floor.");
			}
			return false;
		}

		PendingPawnCell = Rotated;
	}

	if (Pawn && Pawn->IsMoving())
	{
		Pawn->StopAndSnapToCurrentCell(TEXT("the space is rotating"));
	}

	// Pass two: commit. Each block claims its destination cells immediately, so the swap is
	// atomic from every other system's point of view even though the visuals take 0.4 s.
	for (int32 Index = 0; Index < Inside.Num(); ++Index)
	{
		Inside[Index]->BeginRotation(PivotWorld, TurnSign, RotateDuration, Destinations[Index]);
	}

	bRotating = true;
	RotationElapsed = 0.0f;

	Subsystem->ShowFeedback(
		FString::Printf(TEXT("The space turns %s."), bClockwise ? TEXT("clockwise") : TEXT("counter-clockwise")),
		FLinearColor(0.45f, 0.85f, 1.0f));

	return true;
}

void APuzzleRotationTile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bRotating)
	{
		return;
	}

	RotationElapsed += DeltaSeconds;
	if (RotationElapsed < RotateDuration)
	{
		return;
	}

	bRotating = false;

	// Moved at the end rather than swung around with the blocks: the pawn is a ball rolling
	// between cell centres, and interpolating it along an arc would desync the cell it is
	// recorded as standing on from where it is drawn.
	if (PendingPawnCell.IsSet())
	{
		if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
		{
			if (AGridPawn* Pawn = Subsystem->GetGridPawn())
			{
				Pawn->TeleportToCell(PendingPawnCell.GetValue());
			}
		}
		PendingPawnCell.Reset();
	}
}

// ---------------------------------------------------------------------------- Editor

#if WITH_EDITOR

void APuzzleRotationTile::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (!bFinished)
	{
		return;
	}

	const AGridActor* FoundGrid = AGridActor::FindGrid(GetWorld());
	if (!FoundGrid)
	{
		return;
	}

	const FIntPoint Size(SizeInCells, SizeInCells);
	const FIntPoint SnappedMin = GridFootprint::MinCellFromCentre(*FoundGrid, GetActorLocation(), Size);
	if (!FoundGrid->IsValidCell(SnappedMin))
	{
		return;
	}

	Modify();
	SetActorRotation(FRotator::ZeroRotator);
	SetActorLocation(GridFootprint::CentreFromMinCell(
		*FoundGrid, SnappedMin, Size, FoundGrid->CellToWorld(SnappedMin).Z));
}

void APuzzleRotationTile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshVisual();
	PostEditMove(true);
}

#endif
