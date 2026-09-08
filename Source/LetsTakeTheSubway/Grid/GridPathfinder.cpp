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

	/** F가 낮은 순. 동점이면 목표에 가까운 쪽을 택해 탐색이 옆으로 퍼지지 않게 한다. */
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

	// 시작 셀은 폰이 이미 서 있는 곳이므로 다시 검증하지 않는다: 발밑에서 false로 바뀐
	// 규칙 때문에 폰이 빠져나가지 못하는 일은 없어야 한다.
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

	// 방향 비트 -> 인덱스 델타. 같은 비용의 경로가 재현되도록 순서를 고정한다.
	const uint8 DirBits[4] = { EGridDir::North, EGridDir::East, EGridDir::South, EGridDir::West };
	const int32 IndexDeltas[4] = { Width, 1, -Width, -1 };

	bool bReachedGoal = false;

	while (Open.Num() > 0)
	{
		FOpenNode Current;
		Open.HeapPop(Current, FOpenNodeLess(), EAllowShrinking::No);

		// 지연 삭제: 셀 하나가 여러 번 푸시될 수 있으며, 첫 번째 팝만 유효하다.
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
			// NeighborMask가 이미 이웃이 범위 안에 있고 단차 높이 이내임을 보장한다.
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
