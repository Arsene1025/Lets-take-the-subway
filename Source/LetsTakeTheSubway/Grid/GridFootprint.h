// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Grid/GridActor.h"

/**
 * A rectangle of cells: the shape a marker, a puzzle block or a rotation tile covers.
 *
 * Min is the low corner and Size is a count, so the rectangle spans
 * [Min, Min + Size) on both axes and an empty rectangle is impossible to express by
 * accident.
 */
struct FGridRect
{
	FIntPoint Min = FIntPoint::ZeroValue;
	FIntPoint Size = FIntPoint(1, 1);

	FGridRect() = default;
	FGridRect(FIntPoint InMin, FIntPoint InSize) : Min(InMin), Size(InSize) {}

	/** One past the high corner, matching the half-open span. */
	FIntPoint MaxExclusive() const { return Min + Size; }

	/** The high corner itself -- the last cell actually covered. */
	FIntPoint MaxInclusive() const { return Min + Size - FIntPoint(1, 1); }

	bool Contains(FIntPoint Cell) const
	{
		return Cell.X >= Min.X && Cell.Y >= Min.Y
			&& Cell.X < Min.X + Size.X && Cell.Y < Min.Y + Size.Y;
	}

	bool ContainsRect(const FGridRect& Other) const
	{
		return Other.Min.X >= Min.X && Other.Min.Y >= Min.Y
			&& Other.MaxExclusive().X <= MaxExclusive().X
			&& Other.MaxExclusive().Y <= MaxExclusive().Y;
	}

	bool Overlaps(const FGridRect& Other) const
	{
		return Min.X < Other.MaxExclusive().X && Other.Min.X < MaxExclusive().X
			&& Min.Y < Other.MaxExclusive().Y && Other.Min.Y < MaxExclusive().Y;
	}

	void GatherCells(TArray<FIntPoint>& OutCells) const
	{
		OutCells.Reserve(OutCells.Num() + Size.X * Size.Y);
		for (int32 OffsetY = 0; OffsetY < Size.Y; ++OffsetY)
		{
			for (int32 OffsetX = 0; OffsetX < Size.X; ++OffsetX)
			{
				OutCells.Emplace(Min.X + OffsetX, Min.Y + OffsetY);
			}
		}
	}

	bool operator==(const FGridRect& Other) const { return Min == Other.Min && Size == Other.Size; }
	bool operator!=(const FGridRect& Other) const { return !(*this == Other); }
};

/**
 * Cell-rectangle placement and rotation maths, shared by the box marker and the puzzle
 * actors so they all agree on where an actor sitting at a footprint's centre actually is.
 */
namespace GridFootprint
{
	/**
	 * Min corner cell of a rectangle whose centre is at Centre.
	 *
	 * The quarter-cell nudge keeps a centre that lands exactly on a cell boundary from
	 * flooring one cell too low. Same rule as AGridBoxMarker::GetMinCell, which this
	 * replaces.
	 */
	inline FIntPoint MinCellFromCentre(const AGridActor& Grid, const FVector& Centre, FIntPoint Size)
	{
		const FVector HalfSpan(Size.X * Grid.CellSize * 0.5, Size.Y * Grid.CellSize * 0.5, 0.0);
		const FVector Nudge(Grid.CellSize * 0.25, Grid.CellSize * 0.25, 0.0);
		return Grid.WorldToCell(Centre - HalfSpan + Nudge);
	}

	/** World centre of a rectangle, at the given height. Inverse of MinCellFromCentre. */
	inline FVector CentreFromMinCell(const AGridActor& Grid, FIntPoint Min, FIntPoint Size, double Z)
	{
		const FVector Origin = Grid.GetGridOrigin();
		return FVector(
			Origin.X + (Min.X + Size.X * 0.5) * Grid.CellSize,
			Origin.Y + (Min.Y + Size.Y * 0.5) * Grid.CellSize,
			Z);
	}

	/**
	 * Rotate a cell within an N x N region, in region-local coordinates.
	 *
	 * One positive turn is +90 degrees of yaw. Working from cell centres, local cell l maps
	 * to N/2 + Rot90(l + 0.5 - N/2) - 0.5, which reduces to (N-1-l.y, l.x). For N = 4 that
	 * sends (3,3) to (0,3): the corner that was at max X, max Y ends at max X, min Y.
	 */
	inline FIntPoint RotateLocalCell(FIntPoint Local, int32 RegionSize, int32 QuarterTurns)
	{
		const int32 Turns = ((QuarterTurns % 4) + 4) % 4;
		FIntPoint Result = Local;
		for (int32 Turn = 0; Turn < Turns; ++Turn)
		{
			Result = FIntPoint(RegionSize - 1 - Result.Y, Result.X);
		}
		return Result;
	}

	/**
	 * Rotate a rectangle inside a square region.
	 *
	 * Both extreme corners are mapped and recombined rather than deriving the min corner
	 * analytically: the two corners swap roles on odd turns, and taking the component-wise
	 * minimum is correct for every turn count without a special case.
	 */
	inline FGridRect RotateRect(const FGridRect& Rect, const FGridRect& Region, int32 QuarterTurns)
	{
		const int32 Turns = ((QuarterTurns % 4) + 4) % 4;
		const int32 RegionSize = Region.Size.X;

		const FIntPoint LowLocal = RotateLocalCell(Rect.Min - Region.Min, RegionSize, Turns);
		const FIntPoint HighLocal = RotateLocalCell(Rect.MaxInclusive() - Region.Min, RegionSize, Turns);

		const FIntPoint NewMinLocal(FMath::Min(LowLocal.X, HighLocal.X), FMath::Min(LowLocal.Y, HighLocal.Y));
		const FIntPoint NewSize = (Turns % 2 == 0) ? Rect.Size : FIntPoint(Rect.Size.Y, Rect.Size.X);

		return FGridRect(Region.Min + NewMinLocal, NewSize);
	}
}
