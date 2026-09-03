// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/GridPathfinder.h"

#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"

#include "Algo/Reverse.h"
#include "Containers/BitArray.h"

namespace
{
	struct FOpenNode
	{
		int32 Index = INDEX_NONE;
		float F = 0.0f;
		float H = 0.0f;
	};

	/** Lowest F first; ties broken towards the goal so the search does not fan out sideways. */
	struct FOpenNodeLess
	{
		bool operator()(const FOpenNode& A, const FOpenNode& B) const
		{
			return A.F < B.F || (A.F == B.F && A.H < B.H);
		}
	};

	FORCEINLINE float ManhattanDistance(FIntPoint A, FIntPoint B)
	{
		return static_cast<float>(FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y));
	}
}

bool FGridPathfinder::FindPath(const AGridActor& Grid, FIntPoint Start, FIntPoint Goal, const APawn* Pawn, TArray<FIntPoint>& OutPath)
{
	OutPath.Reset();

	if (!Grid.IsValidCell(Start) || !Grid.IsValidCell(Goal))
	{
		return false;
	}

	if (Start == Goal)
	{
		return true;
	}

	// The start cell is where the pawn already stands, so it is never re-validated: a rule
	// that turned false underfoot must not make the pawn unable to path away.
	if (!Grid.CanPawnEnter(Goal, Pawn))
	{
		return false;
	}

	const int32 Width = Grid.SizeInCells.X;
	const int32 NumCells = Grid.Cells.Num();
	if (NumCells != Width * Grid.SizeInCells.Y)
	{
		return false;
	}

	const int32 StartIndex = Grid.CellToIndex(Start);
	const int32 GoalIndex = Grid.CellToIndex(Goal);

	TArray<float> GScore;
	GScore.Init(TNumericLimits<float>::Max(), NumCells);

	TArray<int32> CameFrom;
	CameFrom.Init(INDEX_NONE, NumCells);

	TBitArray<> Closed(false, NumCells);

	TArray<FOpenNode> Open;
	Open.Reserve(256);

	GScore[StartIndex] = 0.0f;
	Open.HeapPush(FOpenNode{ StartIndex, ManhattanDistance(Start, Goal), ManhattanDistance(Start, Goal) }, FOpenNodeLess());

	// Direction bit -> index delta. Order fixed so equal-cost paths are reproducible.
	const uint8 DirBits[4] = { EGridDir::North, EGridDir::East, EGridDir::South, EGridDir::West };
	const int32 IndexDeltas[4] = { Width, 1, -Width, -1 };

	bool bReachedGoal = false;

	while (Open.Num() > 0)
	{
		FOpenNode Current;
		Open.HeapPop(Current, FOpenNodeLess(), EAllowShrinking::No);

		// Lazy deletion: a cell can be pushed several times, only the first pop counts.
		if (Closed[Current.Index])
		{
			continue;
		}
		Closed[Current.Index] = true;

		if (Current.Index == GoalIndex)
		{
			bReachedGoal = true;
			break;
		}

		const FGridCellData& CurrentCell = Grid.Cells[Current.Index];

		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			// NeighborMask already guarantees the neighbour is in range and within step height.
			if ((CurrentCell.NeighborMask & DirBits[Dir]) == 0)
			{
				continue;
			}

			const int32 NeighborIndex = Current.Index + IndexDeltas[Dir];
			if (!Grid.Cells.IsValidIndex(NeighborIndex) || Closed[NeighborIndex])
			{
				continue;
			}

			const FIntPoint NeighborCell = Grid.IndexToCell(NeighborIndex);
			if (!Grid.CanPawnEnter(NeighborCell, Pawn))
			{
				continue;
			}

			const float TentativeG = GScore[Current.Index] + 1.0f;
			if (TentativeG < GScore[NeighborIndex])
			{
				GScore[NeighborIndex] = TentativeG;
				CameFrom[NeighborIndex] = Current.Index;

				const float H = ManhattanDistance(NeighborCell, Goal);
				Open.HeapPush(FOpenNode{ NeighborIndex, TentativeG + H, H }, FOpenNodeLess());
			}
		}
	}

	if (!bReachedGoal)
	{
		return false;
	}

	for (int32 Trace = GoalIndex; Trace != StartIndex; Trace = CameFrom[Trace])
	{
		if (Trace == INDEX_NONE)
		{
			OutPath.Reset();
			return false;
		}
		OutPath.Add(Grid.IndexToCell(Trace));
	}

	Algo::Reverse(OutPath);
	return true;
}
