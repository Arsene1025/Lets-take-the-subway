// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleRotatingObstacle.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleElevatorBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double LTTSPi = 3.14159265358979323846;
	constexpr double LTTSTwoPi = 2.0 * LTTSPi;

	/** Slack in cells, so shapes that merely touch are not treated as overlapping. */
	constexpr double LTTSTouchEpsilon = 0.02;

	double WrapToPi(double Angle)
	{
		while (Angle > LTTSPi)
		{
			Angle -= LTTSTwoPi;
		}
		while (Angle < -LTTSPi)
		{
			Angle += LTTSTwoPi;
		}
		return Angle;
	}

	double WrapToTwoPi(double Angle)
	{
		while (Angle < 0.0)
		{
			Angle += LTTSTwoPi;
		}
		while (Angle >= LTTSTwoPi)
		{
			Angle -= LTTSTwoPi;
		}
		return Angle;
	}

	/**
	 * A shape reduced to the band of radii and the wedge of angles it occupies about the
	 * origin. Rotating such a description is adding to StartAngle, and sweeping it through a
	 * turn is lengthening AngleLength -- which is what makes the swept-area test cheap.
	 */
	struct FPolarSpan
	{
		double MinRadius = 0.0;
		double MaxRadius = 0.0;
		double StartAngle = 0.0;
		double AngleLength = 0.0;

		bool IsFullCircle() const { return AngleLength >= LTTSTwoPi; }
	};

	/** Polar extent of the unit cell whose low corner is at (X, Y), measured from the origin. */
	FPolarSpan MakeCellSpan(double X, double Y)
	{
		FPolarSpan Span;

		const double X0 = X;
		const double X1 = X + 1.0;
		const double Y0 = Y;
		const double Y1 = Y + 1.0;

		const double Corners[4][2] = { { X0, Y0 }, { X1, Y0 }, { X1, Y1 }, { X0, Y1 } };

		double MaxRadiusSq = 0.0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const double CX = Corners[Index][0];
			const double CY = Corners[Index][1];
			MaxRadiusSq = FMath::Max(MaxRadiusSq, CX * CX + CY * CY);
		}
		Span.MaxRadius = FMath::Sqrt(MaxRadiusSq);

		// The origin on or inside the cell means the cell covers every direction, and the
		// corner-angle arithmetic below would have no contiguous wedge to report.
		if (X0 <= 0.0 && 0.0 <= X1 && Y0 <= 0.0 && 0.0 <= Y1)
		{
			Span.MinRadius = 0.0;
			Span.StartAngle = 0.0;
			Span.AngleLength = LTTSTwoPi;
			return Span;
		}

		const double NearX = FMath::Clamp(0.0, X0, X1);
		const double NearY = FMath::Clamp(0.0, Y0, Y1);
		Span.MinRadius = FMath::Sqrt(NearX * NearX + NearY * NearY);

		// Measured as offsets from the first corner, so a wedge straddling the -X axis stays
		// one contiguous interval instead of splitting at the atan2 discontinuity. A cell
		// that excludes the origin always spans less than half a turn, so the offsets are
		// unambiguous.
		const double BaseAngle = FMath::Atan2(Corners[0][1], Corners[0][0]);

		double MinDelta = 0.0;
		double MaxDelta = 0.0;
		for (int32 Index = 1; Index < 4; ++Index)
		{
			const double Delta = WrapToPi(FMath::Atan2(Corners[Index][1], Corners[Index][0]) - BaseAngle);
			MinDelta = FMath::Min(MinDelta, Delta);
			MaxDelta = FMath::Max(MaxDelta, Delta);
		}

		Span.StartAngle = BaseAngle + MinDelta;
		Span.AngleLength = MaxDelta - MinDelta;
		return Span;
	}

	bool ArcsOverlap(const FPolarSpan& A, const FPolarSpan& B)
	{
		if (A.IsFullCircle() || B.IsFullCircle())
		{
			return true;
		}

		return WrapToTwoPi(B.StartAngle - A.StartAngle) <= A.AngleLength
			|| WrapToTwoPi(A.StartAngle - B.StartAngle) <= B.AngleLength;
	}

	bool RadiiOverlap(const FPolarSpan& A, const FPolarSpan& B)
	{
		return A.MinRadius <= B.MaxRadius - LTTSTouchEpsilon
			&& B.MinRadius <= A.MaxRadius - LTTSTouchEpsilon;
	}
}

APuzzleRotatingObstacle::APuzzleRotatingObstacle()
{
	FootprintSize = FIntPoint(6, 4);
	MoveAxis = EPuzzleMoveAxis::None;
	Height = 300.0f;

	// The inherited single box would sit across the channel. The body is drawn instead as
	// the slabs left over once the channel is cut out.
	if (BodyMesh)
	{
		BodyMesh->SetVisibility(false);
		BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FaceMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	static const TCHAR* SlabNames[4] = { TEXT("SlabWest"), TEXT("SlabEast"), TEXT("SlabSouth"), TEXT("SlabNorth") };
	static const TCHAR* FaceNames[4] = { TEXT("FaceWest"), TEXT("FaceEast"), TEXT("FaceSouth"), TEXT("FaceNorth") };

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Slab = CreateDefaultSubobject<UStaticMeshComponent>(SlabNames[Index]);
		Slab->SetupAttachment(SceneRoot);

		// Same rule as the base block: visible to the click trace and to nothing else.
		Slab->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Slab->SetCollisionResponseToAllChannels(ECR_Ignore);
		Slab->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Slab->SetGenerateOverlapEvents(false);

		if (CubeFinder.Succeeded())
		{
			Slab->SetStaticMesh(CubeFinder.Object);
		}
		if (BodyMaterialFinder.Succeeded())
		{
			Slab->SetMaterial(0, BodyMaterialFinder.Object);
		}
		SlabMeshes.Add(Slab);

		UStaticMeshComponent* Face = CreateDefaultSubobject<UStaticMeshComponent>(FaceNames[Index]);
		Face->SetupAttachment(SceneRoot);
		Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Face->SetCollisionResponseToAllChannels(ECR_Ignore);
		Face->SetGenerateOverlapEvents(false);

		if (CubeFinder.Succeeded())
		{
			Face->SetStaticMesh(CubeFinder.Object);
		}
		if (FaceMaterialFinder.Succeeded())
		{
			Face->SetMaterial(0, FaceMaterialFinder.Object);
		}
		InnerFaceMeshes.Add(Face);
	}
}

// ---------------------------------------------------------------------------- Authoring

void APuzzleRotatingObstacle::NormaliseAuthoring()
{
	FootprintSize.X = FMath::Max(FootprintSize.X, 2);
	FootprintSize.Y = FMath::Max(FootprintSize.Y, 2);

	// A rectangle whose sides differ in parity has its centre half a cell off the grid, and
	// everything it carries round would land there too.
	if ((FootprintSize.X % 2) != (FootprintSize.Y % 2))
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: footprint %dx%d cannot turn about its centre and stay on the grid; ")
			TEXT("widened to %dx%d. Both sides must be odd or both even."),
			*GetName(), FootprintSize.X, FootprintSize.Y, FootprintSize.X, FootprintSize.Y + 1);

		FootprintSize.Y += 1;
	}

	ChannelSize.X = FMath::Clamp(ChannelSize.X, 1, FootprintSize.X);
	ChannelSize.Y = FMath::Clamp(ChannelSize.Y, 1, FootprintSize.Y);
	ChannelOffset.X = FMath::Clamp(ChannelOffset.X, 0, FootprintSize.X - ChannelSize.X);
	ChannelOffset.Y = FMath::Clamp(ChannelOffset.Y, 0, FootprintSize.Y - ChannelSize.Y);

	if (ChannelSize.X == FootprintSize.X && ChannelSize.Y == FootprintSize.Y)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: the channel covers the whole footprint, leaving no solid body."), *GetName());
	}

	// Not a piece anyone pushes. Pinned here as well as in the constructor so an old level
	// that stored something else is corrected on load.
	MoveAxis = EPuzzleMoveAxis::None;
}

// ---------------------------------------------------------------------------- Geometry

FIntPoint APuzzleRotatingObstacle::LocalToWorldOffset(FIntPoint Local) const
{
	FIntPoint Point = Local;
	FIntPoint Size = FootprintSize;

	// One turn of a W x H rectangle sends local (x, y) to (H-1-y, x) and swaps the sides.
	for (int32 Turn = 0; Turn < GetQuarterTurns(); ++Turn)
	{
		Point = FIntPoint(Size.Y - 1 - Point.Y, Point.X);
		Size = FIntPoint(Size.Y, Size.X);
	}

	return Point;
}

FGridRect APuzzleRotatingObstacle::GetWorldChannelRect() const
{
	const FIntPoint LowLocal = LocalToWorldOffset(ChannelOffset);
	const FIntPoint HighLocal = LocalToWorldOffset(ChannelOffset + ChannelSize - FIntPoint(1, 1));

	// Both extreme corners are mapped and recombined: they swap roles on odd turns, so the
	// component-wise minimum is right for every facing without a special case.
	const FIntPoint MinLocal(FMath::Min(LowLocal.X, HighLocal.X), FMath::Min(LowLocal.Y, HighLocal.Y));
	const FIntPoint Size = (GetQuarterTurns() % 2 == 0)
		? ChannelSize
		: FIntPoint(ChannelSize.Y, ChannelSize.X);

	return FGridRect(GetRect().Min + MinLocal, Size);
}

FGridRect APuzzleRotatingObstacle::GetRegion() const
{
	const int32 Side = FMath::Max(FootprintSize.X, FootprintSize.Y);
	const FIntPoint WorldFootprint = GetWorldFootprint();

	// Both differences are even because of the parity rule, so the square lands on cell
	// boundaries and shares its centre with the footprint.
	const FIntPoint Inset((WorldFootprint.X - Side) / 2, (WorldFootprint.Y - Side) / 2);
	return FGridRect(GetRect().Min + Inset, FIntPoint(Side, Side));
}

FVector APuzzleRotatingObstacle::GetPivotWorld() const
{
	if (!Grid)
	{
		return GetActorLocation();
	}

	const FGridRect Region = GetRegion();
	const FVector Origin = Grid->GetGridOrigin();

	return FVector(
		Origin.X + (Region.Min.X + Region.Size.X * 0.5) * Grid->CellSize,
		Origin.Y + (Region.Min.Y + Region.Size.Y * 0.5) * Grid->CellSize,
		GetFloorZ());
}

bool APuzzleRotatingObstacle::IsSolidCell(FIntPoint Cell) const
{
	return GetRect().Contains(Cell) && !GetWorldChannelRect().Contains(Cell);
}

void APuzzleRotatingObstacle::GatherOccupiedCells(TArray<FIntPoint>& OutCells) const
{
	const FGridRect Footprint = GetRect();
	const FGridRect Channel = GetWorldChannelRect();

	TArray<FIntPoint> All;
	Footprint.GatherCells(All);

	OutCells.Reserve(OutCells.Num() + All.Num());
	for (const FIntPoint& Cell : All)
	{
		// The channel is left unclaimed on purpose: blocks are pushed into it and the pawn
		// walks through it, which is the whole point of the piece.
		if (!Channel.Contains(Cell))
		{
			OutCells.Add(Cell);
		}
	}
}

// ---------------------------------------------------------------------------- Riders

bool APuzzleRotatingObstacle::IsAttachedToInnerFace(const APuzzleBlock& Block) const
{
	const FGridRect Channel = GetWorldChannelRect();

	TArray<FIntPoint> Cells;
	Block.GatherOccupiedCells(Cells);

	static const EGridDirection Directions[4] = {
		EGridDirection::North, EGridDirection::East, EGridDirection::South, EGridDirection::West };

	for (const FIntPoint& Cell : Cells)
	{
		// Only the part of the block that is actually inside the opening can touch an inner
		// face. Cells hanging out past the open end are carried along regardless.
		if (!Channel.Contains(Cell))
		{
			continue;
		}

		for (const EGridDirection Dir : Directions)
		{
			if (IsSolidCell(Cell + LTTSGrid::DirOffset(Dir)))
			{
				return true;
			}
		}
	}

	return false;
}

void APuzzleRotatingObstacle::GatherRiders(TArray<APuzzleBlock*>& OutRiders) const
{
	const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Subsystem->GetBlocks())
	{
		APuzzleBlock* Block = Entry.Get();
		if (!Block || Block == this)
		{
			continue;
		}

		// The design excludes the elevator by name. It is a goal piece whose door facing is
		// the puzzle's state, so having a passing structure re-aim it would be a second,
		// hidden way to solve the level.
		if (Block->IsA<APuzzleElevatorBlock>())
		{
			continue;
		}

		if (IsAttachedToInnerFace(*Block))
		{
			OutRiders.Add(Block);
		}
	}
}

// ---------------------------------------------------------------------------- Sweep

void APuzzleRotatingObstacle::GatherSweptCells(
	const TArray<FIntPoint>& MovingCells, int32 TurnSign, TSet<FIntPoint>& OutCells) const
{
	if (!Grid || MovingCells.IsEmpty())
	{
		return;
	}

	const FGridRect Region = GetRegion();
	const double PivotX = Region.Min.X + Region.Size.X * 0.5;
	const double PivotY = Region.Min.Y + Region.Size.Y * 0.5;

	const double Turn = (TurnSign >= 0) ? (LTTSPi * 0.5) : (-LTTSPi * 0.5);

	TArray<FPolarSpan> Swept;
	Swept.Reserve(MovingCells.Num());

	double MaxRadius = 0.0;

	for (const FIntPoint& Cell : MovingCells)
	{
		FPolarSpan Span = MakeCellSpan(Cell.X - PivotX, Cell.Y - PivotY);

		if (!Span.IsFullCircle())
		{
			// Widening the wedge by the turn is exactly the region the cell passes through:
			// a rotation only adds to an angle and leaves the radius alone.
			if (Turn < 0.0)
			{
				Span.StartAngle += Turn;
			}
			Span.AngleLength = FMath::Min(Span.AngleLength + FMath::Abs(Turn), LTTSTwoPi);
		}

		MaxRadius = FMath::Max(MaxRadius, Span.MaxRadius);
		Swept.Add(Span);
	}

	const int32 MinX = FMath::FloorToInt32(PivotX - MaxRadius);
	const int32 MaxX = FMath::CeilToInt32(PivotX + MaxRadius);
	const int32 MinY = FMath::FloorToInt32(PivotY - MaxRadius);
	const int32 MaxY = FMath::CeilToInt32(PivotY + MaxRadius);

	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const FPolarSpan Candidate = MakeCellSpan(X - PivotX, Y - PivotY);

			for (const FPolarSpan& Span : Swept)
			{
				if (RadiiOverlap(Candidate, Span) && ArcsOverlap(Span, Candidate))
				{
					OutCells.Add(FIntPoint(X, Y));
					break;
				}
			}
		}
	}
}

bool APuzzleRotatingObstacle::FindPawnRefuge(
	FIntPoint From, const TSet<FIntPoint>& Swept, const TSet<FIntPoint>& Destinations, FIntPoint& OutCell) const
{
	if (!Grid)
	{
		return false;
	}

	// Walls are walked around rather than through, so the pawn is never pushed somewhere it
	// could not have reached on foot.
	TSet<FIntPoint> Visited;
	TArray<FIntPoint> Queue;
	Visited.Add(From);
	Queue.Add(From);

	static const EGridDirection Directions[4] = {
		EGridDirection::North, EGridDirection::East, EGridDirection::South, EGridDirection::West };

	constexpr int32 MaxVisited = 512;

	for (int32 Head = 0; Head < Queue.Num() && Visited.Num() < MaxVisited; ++Head)
	{
		const FIntPoint Current = Queue[Head];

		for (const EGridDirection Dir : Directions)
		{
			const FIntPoint Next = Current + LTTSGrid::DirOffset(Dir);

			if (Visited.Contains(Next))
			{
				continue;
			}
			Visited.Add(Next);

			if (!Grid->IsValidCell(Next) || !Grid->IsCellWalkableStatic(Next))
			{
				continue;
			}

			// Anything holding a cell here is scenery or a block that is staying put; the
			// pieces that are about to move released their cells before this ran.
			if (Grid->IsCellOccupied(Next))
			{
				continue;
			}

			if (!Swept.Contains(Next) && !Destinations.Contains(Next))
			{
				OutCell = Next;
				return true;
			}

			Queue.Add(Next);
		}
	}

	return false;
}

// ---------------------------------------------------------------------------- Rotation

bool APuzzleRotatingObstacle::TryRotate(int32 TurnSign, FText* OutReason)
{
	if (!Grid || IsAnimating())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleBusy", "The structure is still turning.");
		}
		return false;
	}

	UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
	if (!Subsystem)
	{
		return false;
	}

	const int32 Sign = (TurnSign >= 0) ? 1 : -1;
	const FGridRect Region = GetRegion();

	TArray<APuzzleBlock*> Riders;
	GatherRiders(Riders);

	for (const APuzzleBlock* Rider : Riders)
	{
		if (Rider->IsAnimating())
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleRiderMoving", "Wait for the pieces to settle.");
			}
			return false;
		}
	}

	// ---- Pass one: work out where everything lands, and refuse before touching anything.

	TArray<FIntPoint> MovingCells;
	GatherOccupiedCells(MovingCells);

	TArray<FGridRect> RiderDestinations;
	RiderDestinations.Reserve(Riders.Num());

	TSet<const AActor*> Movers;
	Movers.Add(this);

	for (const APuzzleBlock* Rider : Riders)
	{
		Rider->GatherOccupiedCells(MovingCells);
		RiderDestinations.Add(GridFootprint::RotateRect(Rider->GetRect(), Region, Sign));
		Movers.Add(Rider);
	}

	const FGridRect Destination = GridFootprint::RotateRect(GetRect(), Region, Sign);

	// Every cell the structure or a passenger comes to rest on has to be floor, including
	// the channel: an opening hanging over a drop is not somewhere anything can be pushed.
	TSet<FIntPoint> DestinationCells;
	{
		TArray<FIntPoint> Cells;
		Destination.GatherCells(Cells);
		for (const FGridRect& Rect : RiderDestinations)
		{
			Rect.GatherCells(Cells);
		}

		for (const FIntPoint& Cell : Cells)
		{
			if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
			{
				if (OutReason)
				{
					*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleNoFloor", "Cannot turn: there is no floor to turn into.");
				}
				return false;
			}
			DestinationCells.Add(Cell);
		}
	}

	TSet<FIntPoint> Swept;
	GatherSweptCells(MovingCells, Sign, Swept);

	const TSet<FIntPoint> MovingSet(MovingCells);

	for (const FIntPoint& Cell : Swept)
	{
		if (MovingSet.Contains(Cell))
		{
			continue;
		}

		// Outside the grid and cells with no floor are open air the structure swings over.
		if (!Grid->IsValidCell(Cell))
		{
			continue;
		}

		if (const FGridCellData* Data = Grid->GetCell(Cell))
		{
			if (Data->Type == EGridCellType::Blocked)
			{
				if (OutReason)
				{
					*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleHitsWall", "Cannot turn: it would swing into a wall.");
				}
				return false;
			}
		}

		const AActor* Occupant = Grid->GetOccupant(Cell);
		if (Occupant && !Movers.Contains(Occupant))
		{
			if (OutReason)
			{
				*OutReason = FText::Format(
					NSLOCTEXT("LTTSPuzzle", "ObstacleHitsObject", "Cannot turn: {0} is in the way."),
					FText::FromString(Occupant->GetName()));
			}
			return false;
		}
	}

	// The pawn is part of the space. It cannot ride along, so it is pushed clear -- and if
	// there is nowhere clear to push it to, nothing turns.
	AGridPawn* Pawn = Subsystem->GetGridPawn();
	TOptional<FIntPoint> PawnRefuge;

	if (Pawn)
	{
		if (Pawn->IsMoving())
		{
			Pawn->StopAndSnapToCurrentCell(TEXT("the structure is turning"));
		}

		const FIntPoint PawnCell = Pawn->GetCurrentCell();
		if (Swept.Contains(PawnCell) || DestinationCells.Contains(PawnCell))
		{
			FIntPoint Refuge;
			if (!FindPawnRefuge(PawnCell, Swept, DestinationCells, Refuge))
			{
				if (OutReason)
				{
					*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleNoRefuge", "Cannot turn: you have nowhere to be pushed to.");
				}
				return false;
			}
			PawnRefuge = Refuge;
		}
	}

	// ---- Pass two: commit.

	// Every mover lets go before any of them claims, so a cell changing hands between two of
	// them is not refused because the previous holder has not moved yet.
	Grid->ClearAllOccupantsOf(this);
	for (APuzzleBlock* Rider : Riders)
	{
		Grid->ClearAllOccupantsOf(Rider);
	}

	const FVector Pivot = GetPivotWorld();

	BeginRotation(Pivot, Sign, RotateDuration, Destination);
	for (int32 Index = 0; Index < Riders.Num(); ++Index)
	{
		Riders[Index]->BeginRotation(Pivot, Sign, RotateDuration, RiderDestinations[Index]);
	}

	// Moved at the start rather than the end, unlike the rotation tile: there the pawn rides
	// the space round, whereas here a wall is arriving in the cell it is standing in.
	if (Pawn && PawnRefuge.IsSet())
	{
		Pawn->TeleportToCell(PawnRefuge.GetValue());
		Subsystem->ShowFeedback(TEXT("You are shoved out of the way."), FLinearColor(1.0f, 0.65f, 0.05f));
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: turned %s with %d rider(s); footprint now at cell (%d,%d)."),
		*GetName(), (Sign > 0) ? TEXT("clockwise") : TEXT("counter-clockwise"),
		Riders.Num(), Destination.Min.X, Destination.Min.Y);

	return true;
}

// ---------------------------------------------------------------------------- Visual

void APuzzleRotatingObstacle::RefreshVisual()
{
	// Deliberately not calling Super: the inherited single box spans the channel.
	const double CellSize = Grid ? Grid->CellSize : 100.0;

	const double HalfX = FootprintSize.X * 0.5;
	const double HalfY = FootprintSize.Y * 0.5;

	const int32 ChannelMinX = ChannelOffset.X;
	const int32 ChannelMaxX = ChannelOffset.X + ChannelSize.X;
	const int32 ChannelMinY = ChannelOffset.Y;
	const int32 ChannelMaxY = ChannelOffset.Y + ChannelSize.Y;

	// The footprint minus the channel, cut into four disjoint rectangles: the two long walls
	// beside the channel and the two end caps. Any of them may be empty, which is what leaves
	// an end of the channel open.
	const int32 Slabs[4][4] = {
		{ 0,			ChannelMinX,	0,				FootprintSize.Y },
		{ ChannelMaxX,	FootprintSize.X, 0,				FootprintSize.Y },
		{ ChannelMinX,	ChannelMaxX,	0,				ChannelMinY },
		{ ChannelMinX,	ChannelMaxX,	ChannelMaxY,	FootprintSize.Y }
	};

	for (int32 Index = 0; Index < SlabMeshes.Num() && Index < 4; ++Index)
	{
		UStaticMeshComponent* Slab = SlabMeshes[Index];
		if (!Slab)
		{
			continue;
		}

		const int32 X0 = Slabs[Index][0];
		const int32 X1 = Slabs[Index][1];
		const int32 Y0 = Slabs[Index][2];
		const int32 Y1 = Slabs[Index][3];

		const bool bUsed = (X1 > X0) && (Y1 > Y0);
		Slab->SetVisibility(bUsed);
		Slab->SetCollisionEnabled(bUsed ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

		if (!bUsed)
		{
			continue;
		}

		Slab->SetRelativeLocation(FVector(
			((X0 + X1) * 0.5 - HalfX) * CellSize,
			((Y0 + Y1) * 0.5 - HalfY) * CellSize,
			Height * 0.5));

		Slab->SetRelativeScale3D(FVector(
			(X1 - X0) * CellSize / 100.0,
			(Y1 - Y0) * CellSize / 100.0,
			Height / 100.0));
	}

	// The hatched faces from the design: a thin panel on each wall that borders the channel,
	// in the elevator door's material so "this is the face things stick to" reads at a glance.
	constexpr double FaceThickness = 12.0;
	const double FaceHeight = FMath::Max(Height * InnerFaceHeightRatio, 1.0);

	const bool bFaceUsed[4] = {
		ChannelMinX > 0,
		ChannelMaxX < FootprintSize.X,
		ChannelMinY > 0,
		ChannelMaxY < FootprintSize.Y
	};

	const double ChannelCentreX = ((ChannelMinX + ChannelMaxX) * 0.5 - HalfX) * CellSize;
	const double ChannelCentreY = ((ChannelMinY + ChannelMaxY) * 0.5 - HalfY) * CellSize;
	const double ChannelSpanX = ChannelSize.X * CellSize;
	const double ChannelSpanY = ChannelSize.Y * CellSize;

	for (int32 Index = 0; Index < InnerFaceMeshes.Num() && Index < 4; ++Index)
	{
		UStaticMeshComponent* Face = InnerFaceMeshes[Index];
		if (!Face)
		{
			continue;
		}

		Face->SetVisibility(bFaceUsed[Index]);
		if (!bFaceUsed[Index])
		{
			continue;
		}

		FVector Location(0.0, 0.0, FaceHeight * 0.5);
		FVector Scale(1.0, 1.0, FaceHeight / 100.0);

		// Pushed half a thickness into the opening so the panel stands proud of the wall
		// instead of being half sunk into it, where it would read as a seam.
		switch (Index)
		{
		case 0:		// west wall, facing east into the channel
			Location.X = (ChannelMinX - HalfX) * CellSize + FaceThickness * 0.5;
			Location.Y = ChannelCentreY;
			Scale.X = FaceThickness / 100.0;
			Scale.Y = ChannelSpanY / 100.0;
			break;

		case 1:		// east wall
			Location.X = (ChannelMaxX - HalfX) * CellSize - FaceThickness * 0.5;
			Location.Y = ChannelCentreY;
			Scale.X = FaceThickness / 100.0;
			Scale.Y = ChannelSpanY / 100.0;
			break;

		case 2:		// south wall
			Location.X = ChannelCentreX;
			Location.Y = (ChannelMinY - HalfY) * CellSize + FaceThickness * 0.5;
			Scale.X = ChannelSpanX / 100.0;
			Scale.Y = FaceThickness / 100.0;
			break;

		default:	// north wall
			Location.X = ChannelCentreX;
			Location.Y = (ChannelMaxY - HalfY) * CellSize - FaceThickness * 0.5;
			Scale.X = ChannelSpanX / 100.0;
			Scale.Y = FaceThickness / 100.0;
			break;
		}

		Face->SetRelativeLocation(Location);
		Face->SetRelativeScale3D(Scale);
	}
}

// ---------------------------------------------------------------------------- Lifecycle

void APuzzleRotatingObstacle::RegisterWithSubsystem(UPuzzleSubsystem& Subsystem)
{
	// Not in the pushable-block list: the drag code would try to shove it, and a rotation
	// tile would see a piece far too big to be turned by it.
	Subsystem.RegisterObstacle(this);
}

void APuzzleRotatingObstacle::UnregisterFromSubsystem(UPuzzleSubsystem& Subsystem)
{
	Subsystem.UnregisterObstacle(this);
}

void APuzzleRotatingObstacle::OnConstruction(const FTransform& Transform)
{
	NormaliseAuthoring();
	Super::OnConstruction(Transform);
}

void APuzzleRotatingObstacle::BeginPlay()
{
	Super::BeginPlay();

	if (!Grid)
	{
		return;
	}

	// Belt and braces: OnConstruction already enforces this, but every destination the piece
	// computes is measured from the turning square, and a square half a cell off the grid
	// would put blocks and the pawn somewhere no other system can express.
	ensureMsgf((FootprintSize.X % 2) == (FootprintSize.Y % 2),
		TEXT("%s: footprint %dx%d breaks the parity rule; the turning square is off the grid."),
		*GetName(), FootprintSize.X, FootprintSize.Y);

	const FGridRect Channel = GetWorldChannelRect();
	const FGridRect Region = GetRegion();

	TArray<FIntPoint> Solid;
	GatherOccupiedCells(Solid);

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %d solid cell(s), channel %dx%d at cell (%d,%d), turning region %dx%d at (%d,%d)."),
		*GetName(), Solid.Num(),
		Channel.Size.X, Channel.Size.Y, Channel.Min.X, Channel.Min.Y,
		Region.Size.X, Region.Size.Y, Region.Min.X, Region.Min.Y);

	// The channel is only useful if it is floor, and a turn that lands the body off the
	// platform is refused at runtime -- worth saying now rather than when the player tries.
	TArray<FIntPoint> ChannelCells;
	Channel.GatherCells(ChannelCells);
	for (const FIntPoint& Cell : ChannelCells)
	{
		if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: channel cell (%d,%d) is not walkable floor; nothing can stand in the opening there."),
				*GetName(), Cell.X, Cell.Y);
		}
	}
}

#if WITH_EDITOR

bool APuzzleRotatingObstacle::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (!InProperty)
	{
		return true;
	}

	// Neither means anything here: the structure is turned by its lever, never pushed.
	const FName Name = InProperty->GetFName();
	return Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, MoveAxis)
		&& Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, bOneStepPerDrag);
}

#endif

// ---------------------------------------------------------------------------- Console

namespace
{
	/**
	 * Turn an obstacle without its lever.
	 *
	 * The lever can only be used from a cell beside it, so the one case the design calls for
	 * -- the pawn standing inside the channel when the structure turns -- cannot be reached
	 * by playing normally. This exists so it can still be tested.
	 */
	void RotateObstacleCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.RotateObstacle: run this in play mode."));
			return;
		}

		const FString Filter = Args.IsValidIndex(0) ? Args[0] : FString();
		const int32 Sign = (Args.IsValidIndex(1) && Args[1].StartsWith(TEXT("-"))) ? -1 : 1;

		int32 Matched = 0;
		for (TActorIterator<APuzzleRotatingObstacle> It(World); It; ++It)
		{
			APuzzleRotatingObstacle* Obstacle = *It;
			if (!Obstacle || (!Filter.IsEmpty() && !Obstacle->GetName().Contains(Filter)))
			{
				continue;
			}

			++Matched;

			FText Reason;
			if (!Obstacle->TryRotate(Sign, &Reason))
			{
				UE_LOG(LogLTTSGrid, Warning, TEXT("%s: %s"), *Obstacle->GetName(), *Reason.ToString());
			}
		}

		if (Matched == 0)
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("ltts.RotateObstacle: no rotating obstacle matching '%s'."), *Filter);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRotateObstacleCommand(
	TEXT("ltts.RotateObstacle"),
	TEXT("Turn a rotating obstacle without its lever: ltts.RotateObstacle <name substring> [+1|-1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RotateObstacleCommand));
