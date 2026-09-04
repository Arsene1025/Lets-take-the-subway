// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleBlock.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APuzzleBlock::APuzzleBlock()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);

	// Blocks Visibility and nothing else: the click trace uses that channel to identify what
	// the cursor grabbed, while physics and the pawn (which has no collision at all) stay
	// out of it entirely.
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BodyMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	if (MaterialFinder.Succeeded())
	{
		BodyMesh->SetMaterial(0, MaterialFinder.Object);
	}

	// The grid must trace the floor underneath the block, not the block itself.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

	// The puzzle is one connected mechanism: half of it unloaded because the camera moved
	// would silently change what the player can solve. The flag is authoring data that the
	// cooker reads, so it only exists in editor builds.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

// ---------------------------------------------------------------------------- Queries

FIntPoint APuzzleBlock::GetWorldFootprint() const
{
	return (QuarterTurns % 2 == 0) ? FootprintSize : FIntPoint(FootprintSize.Y, FootprintSize.X);
}

EPuzzleMoveAxis APuzzleBlock::GetWorldMoveAxis() const
{
	return LTTSPuzzle::RotateAxis(MoveAxis, QuarterTurns);
}

// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
FBox APuzzleBlock::GetFullBounds() const
{
	if (!Grid)
	{
		return FBox(ForceInit);
	}

	const FGridRect Rect = GetRect();
	const FVector Origin = Grid->GetGridOrigin();

	const FVector Min(
		Origin.X + Rect.Min.X * Grid->CellSize,
		Origin.Y + Rect.Min.Y * Grid->CellSize,
		FloorZ);

	const FVector Max(
		Origin.X + Rect.MaxExclusive().X * Grid->CellSize,
		Origin.Y + Rect.MaxExclusive().Y * Grid->CellSize,
		FloorZ + Height);

	return FBox(Min, Max);
}
#endif

// ---------------------------------------------------------------------------- Placement

void APuzzleBlock::RefreshVisual()
{
	if (!BodyMesh)
	{
		return;
	}

	// The footprint is authored in the local frame and the actor's yaw carries the rotation,
	// so the mesh is always sized from the unrotated size.
	const double CellSize = Grid ? Grid->CellSize : 100.0;

	// --- CUTAWAY DISABLED 2026-09-04: was GetVisualHeight() ---
	const double VisualHeight = Height;

	BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, VisualHeight * 0.5));
	BodyMesh->SetRelativeScale3D(FVector(
		FootprintSize.X * CellSize / 100.0,
		FootprintSize.Y * CellSize / 100.0,
		VisualHeight / 100.0));
}

// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
void APuzzleBlock::SetCutaway(bool bInCutaway)
{
	if (bCutawayTarget == bInCutaway)
	{
		return;
	}

	bCutawayTarget = bInCutaway;

	UE_LOG(LogLTTSGrid, Verbose, TEXT("%s: cutaway %s."), *GetName(), bInCutaway ? TEXT("on") : TEXT("off"));
}
#endif

void APuzzleBlock::ClaimCells()
{
	if (!Grid)
	{
		return;
	}

	Grid->ClearAllOccupantsOf(this);

	TArray<FIntPoint> Cells;
	GetRect().GatherCells(Cells);
	for (const FIntPoint& Cell : Cells)
	{
		Grid->SetOccupant(Cell, this);
	}
}

void APuzzleBlock::SnapToRect()
{
	if (!Grid)
	{
		return;
	}

	SetActorLocation(GridFootprint::CentreFromMinCell(*Grid, MinCell, GetWorldFootprint(), FloorZ));
}

void APuzzleBlock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
}

void APuzzleBlock::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; the block cannot be placed."), *GetName());
		return;
	}

	QuarterTurns = ((FMath::RoundToInt32(GetActorRotation().Yaw / 90.0) % 4) + 4) % 4;

	const FIntPoint WorldFootprint = GetWorldFootprint();
	MinCell = GridFootprint::MinCellFromCentre(*Grid, GetActorLocation(), WorldFootprint);
	FloorZ = Grid->CellToWorld(MinCell).Z;

	// Report rather than correct: a block hanging off the platform or overlapping another is
	// a level bug, and quietly nudging it would hide which cells the designer meant.
	TArray<FIntPoint> Cells;
	GetRect().GatherCells(Cells);
	for (const FIntPoint& Cell : Cells)
	{
		if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: covers cell (%d,%d), which is not walkable floor."), *GetName(), Cell.X, Cell.Y);
		}
		else if (Grid->IsCellOccupied(Cell, this))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: cell (%d,%d) is already taken by %s."),
				*GetName(), Cell.X, Cell.Y, *GetNameSafe(Grid->GetOccupant(Cell)));
		}
	}

	ClaimCells();
	SnapToRect();
	RefreshVisual();

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->RegisterBlock(this);
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %dx%d block at cell (%d,%d), %d quarter turn(s)."),
		*GetName(), WorldFootprint.X, WorldFootprint.Y, MinCell.X, MinCell.Y, QuarterTurns);
}

void APuzzleBlock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Grid)
	{
		Grid->ClearAllOccupantsOf(this);
	}

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->UnregisterBlock(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- Movement

bool APuzzleBlock::CanSlide(EGridDirection Dir, FText* OutReason) const
{
	if (!Grid)
	{
		return false;
	}

	if (!LTTSPuzzle::AxisAllowsDirection(GetWorldMoveAxis(), Dir))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockWrongAxis", "This block does not move that way.");
		}
		return false;
	}

	const FGridRect Target(MinCell + LTTSGrid::DirOffset(Dir), GetWorldFootprint());

	TArray<FIntPoint> Cells;
	Target.GatherCells(Cells);

	// The pawn spends most of a step between two cells, so the cell it is committed to
	// counts as taken: a block that slid into it would end up standing on the pawn.
	TArray<FIntPoint> PawnCells;
	if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->GetPawnReservedCells(PawnCells);
	}

	for (const FIntPoint& Cell : Cells)
	{
		if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockNoFloor", "There is no floor that way.");
			}
			return false;
		}

		if (Grid->IsCellOccupied(Cell, this))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockBlocked", "Something is in the way.");
			}
			return false;
		}

		if (PawnCells.Contains(Cell))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockPawnInWay", "You are standing in the way.");
			}
			return false;
		}
	}

	return true;
}

bool APuzzleBlock::StartSlide(EGridDirection Dir)
{
	if (!Grid || IsAnimating() || !CanSlide(Dir))
	{
		return false;
	}

	MinCell += LTTSGrid::DirOffset(Dir);

	// Claimed before the actor has travelled anywhere: for the rest of the step both the
	// pawn and any other block must treat the destination as already taken.
	ClaimCells();

	SlideTarget = GridFootprint::CentreFromMinCell(*Grid, MinCell, GetWorldFootprint(), FloorZ);
	AnimState = EAnimState::Sliding;
	++StepsWhileHeld;

	return true;
}

void APuzzleBlock::SetHeld(bool bInHeld)
{
	if (bHeld == bInHeld)
	{
		return;
	}

	bHeld = bInHeld;

	if (bHeld)
	{
		StepsWhileHeld = 0;
		return;
	}

	// Released between steps: the block is already at rest, so nothing will arrive later to
	// trigger the rotation check.
	if (!IsAnimating())
	{
		ReportAtRest();
	}
}

void APuzzleBlock::ReportAtRest()
{
	if (StepsWhileHeld <= 0)
	{
		return;
	}

	StepsWhileHeld = 0;

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->NotifyBlockCameToRest(this);
	}
}

void APuzzleBlock::BeginRotation(const FVector& Pivot, int32 TurnSign, float Duration, const FGridRect& NewRect)
{
	if (!Grid)
	{
		return;
	}

	RotationPivot = Pivot;
	RotationTurnSign = (TurnSign >= 0) ? 1 : -1;
	RotationDuration = FMath::Max(Duration, 0.01f);
	RotationElapsed = 0.0f;
	RotationStartLocation = GetActorLocation();
	RotationStartYaw = GetActorRotation().Yaw;
	RotationTargetYaw = RotationStartYaw + 90.0 * RotationTurnSign;

	// The whole layout change is committed now. The animation that follows is decoration:
	// every query during it already reports where the block is going to be.
	QuarterTurns = ((QuarterTurns + RotationTurnSign) % 4 + 4) % 4;
	MinCell = NewRect.Min;
	ClaimCells();

	RotationTargetLocation = GridFootprint::CentreFromMinCell(*Grid, MinCell, GetWorldFootprint(), FloorZ);
	AnimState = EAnimState::Rotating;
}

void APuzzleBlock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	// Cutaway is independent of sliding and rotating: a block can be shoved aside while it
	// is flattened, and it should stay flattened for as long as it is in the way.
	const float TargetAlpha = bCutawayTarget ? 1.0f : 0.0f;
	if (!FMath::IsNearlyEqual(CutawayAlpha, TargetAlpha))
	{
		CutawayAlpha = FMath::FInterpConstantTo(CutawayAlpha, TargetAlpha, DeltaSeconds, 1.0f / FMath::Max(CutawayBlendTime, 0.01f));
		RefreshVisual();
	}
#endif

	switch (AnimState)
	{
	case EAnimState::Sliding:
	{
		const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), SlideTarget, DeltaSeconds, SlideSpeed);
		SetActorLocation(NewLocation);

		if (NewLocation.Equals(SlideTarget, 0.5))
		{
			SetActorLocation(SlideTarget);
			AnimState = EAnimState::Idle;

			if (!bHeld)
			{
				ReportAtRest();
			}
		}
		break;
	}

	case EAnimState::Rotating:
	{
		RotationElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(RotationElapsed / RotationDuration, 0.0f, 1.0f);
		const double Angle = 90.0 * RotationTurnSign * FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

		const FVector Offset = RotationStartLocation - RotationPivot;
		const FVector Swung = FRotator(0.0, Angle, 0.0).RotateVector(FVector(Offset.X, Offset.Y, 0.0));
		SetActorLocation(FVector(RotationPivot.X + Swung.X, RotationPivot.Y + Swung.Y, RotationStartLocation.Z));

		FRotator Rotation = GetActorRotation();
		Rotation.Yaw = RotationStartYaw + Angle;
		SetActorRotation(Rotation);

		if (Alpha >= 1.0f)
		{
			SetActorLocation(RotationTargetLocation);
			Rotation.Yaw = RotationTargetYaw;
			SetActorRotation(Rotation);
			AnimState = EAnimState::Idle;
		}
		break;
	}

	default:
		break;
	}
}

// ---------------------------------------------------------------------------- Editor

#if WITH_EDITOR

void APuzzleBlock::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (!bFinished)
	{
		return;		// mid-drag; snapping every frame would fight the gizmo
	}

	const AGridActor* FoundGrid = AGridActor::FindGrid(GetWorld());
	if (!FoundGrid)
	{
		return;
	}

	// Yaw is the authoring channel for orientation, so round it to a quarter turn before
	// deriving anything from it. Pitch and roll would tilt the footprint off the grid.
	FRotator Rotation = GetActorRotation();
	const int32 Turns = ((FMath::RoundToInt32(Rotation.Yaw / 90.0) % 4) + 4) % 4;
	Rotation = FRotator(0.0, Turns * 90.0, 0.0);

	const FIntPoint WorldFootprint = (Turns % 2 == 0) ? FootprintSize : FIntPoint(FootprintSize.Y, FootprintSize.X);
	const FIntPoint SnappedMin = GridFootprint::MinCellFromCentre(*FoundGrid, GetActorLocation(), WorldFootprint);
	if (!FoundGrid->IsValidCell(SnappedMin))
	{
		return;		// dragged off the grid; leave it so it can be dragged back
	}

	const double SnapFloorZ = FoundGrid->CellToWorld(SnappedMin).Z;

	Modify();
	SetActorRotation(Rotation);
	SetActorLocation(GridFootprint::CentreFromMinCell(*FoundGrid, SnappedMin, WorldFootprint, SnapFloorZ));
}

void APuzzleBlock::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshVisual();
	PostEditMove(true);
}

#endif
