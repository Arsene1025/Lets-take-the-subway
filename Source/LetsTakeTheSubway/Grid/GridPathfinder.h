// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AGridActor;
class APawn;

/**
 * A* over the 4-neighbour grid.
 *
 * Neighbours come from FGridCellData::NeighborMask, which already encodes both "in range"
 * and "step height is passable", so the search only has to ask the grid whether the pawn
 * may enter the cell. Cost per step is 1 and the heuristic is Manhattan distance, which is
 * exactly admissible on a 4-connected uniform grid.
 */
struct FGridPathfinder
{
	/**
	 * @param Pawn      Evaluated against Conditional cells. May be null (rules then see null).
	 * @param OutPath   Cells to walk through: excludes Start, ends with Goal.
	 * @return          False when no route exists. Start == Goal returns true with an empty path.
	 */
	static bool FindPath(const AGridActor& Grid, FIntPoint Start, FIntPoint Goal, const APawn* Pawn, TArray<FIntPoint>& OutPath);
};
