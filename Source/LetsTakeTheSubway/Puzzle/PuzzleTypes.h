// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Grid/GridTypes.h"
#include "PuzzleTypes.generated.h"

/**
 * Which way a puzzle block is allowed to slide.
 *
 * Rush Hour's whole difficulty comes from pieces that only travel along their own length,
 * so this is authored per block rather than derived from the footprint: a 1x1 block that
 * may only move north-south is a perfectly good puzzle piece.
 */
UENUM(BlueprintType)
enum class EPuzzleMoveAxis : uint8
{
	/** Cannot be pushed at all. Prefer a Blocked marker for permanent scenery. */
	None	UMETA(DisplayName = "Immovable"),

	/** Free in all four directions. */
	Both	UMETA(DisplayName = "Both axes"),

	/** East and west only. */
	AxisX	UMETA(DisplayName = "X axis (east/west)"),

	/** North and south only. */
	AxisY	UMETA(DisplayName = "Y axis (north/south)")
};

namespace LTTSPuzzle
{
	/** The axis a door or facing lies on: a north-facing door slides north and south. */
	inline EPuzzleMoveAxis AxisForDirection(EGridDirection Dir)
	{
		return (Dir == EGridDirection::North || Dir == EGridDirection::South)
			? EPuzzleMoveAxis::AxisY
			: EPuzzleMoveAxis::AxisX;
	}

	/** Turning a quarter turn swaps the two single-axis values and leaves the others alone. */
	inline EPuzzleMoveAxis RotateAxis(EPuzzleMoveAxis Axis, int32 QuarterTurns)
	{
		if (QuarterTurns % 2 == 0)
		{
			return Axis;
		}

		switch (Axis)
		{
		case EPuzzleMoveAxis::AxisX:	return EPuzzleMoveAxis::AxisY;
		case EPuzzleMoveAxis::AxisY:	return EPuzzleMoveAxis::AxisX;
		default:						return Axis;
		}
	}

	inline bool AxisAllowsDirection(EPuzzleMoveAxis Axis, EGridDirection Dir)
	{
		switch (Axis)
		{
		case EPuzzleMoveAxis::Both:		return true;
		case EPuzzleMoveAxis::AxisX:	return Dir == EGridDirection::East || Dir == EGridDirection::West;
		case EPuzzleMoveAxis::AxisY:	return Dir == EGridDirection::North || Dir == EGridDirection::South;
		default:						return false;
		}
	}

	/** The two directions an axis permits, in the order negative then positive. */
	inline bool GetAxisDirections(EPuzzleMoveAxis Axis, EGridDirection& OutNegative, EGridDirection& OutPositive)
	{
		if (Axis == EPuzzleMoveAxis::AxisX)
		{
			OutNegative = EGridDirection::West;
			OutPositive = EGridDirection::East;
			return true;
		}
		if (Axis == EPuzzleMoveAxis::AxisY)
		{
			OutNegative = EGridDirection::South;
			OutPositive = EGridDirection::North;
			return true;
		}
		return false;
	}
}
