// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Grid/GridTypes.h"
#include "PuzzleTypes.generated.h"

/**
 * 퍼즐 블록이 슬라이드할 수 있는 방향.
 *
 * Rush Hour의 난이도는 조각이 자기 길이 방향으로만 움직인다는 데서 나오므로, 풋프린트에서
 * 유도하지 않고 블록마다 직접 지정한다: 남북으로만 움직이는 1x1 블록도 충분히 좋은 퍼즐
 * 조각이다.
 */
UENUM(BlueprintType)
enum class EPuzzleMoveAxis : uint8
{
	/** 전혀 밀 수 없다. 영구 지형이라면 Blocked 마커를 쓰는 편이 낫다. */
	None	UMETA(DisplayName = "Immovable"),

	/** 네 방향 모두 자유롭다. */
	Both	UMETA(DisplayName = "Both axes"),

	/** 동서 방향만. */
	AxisX	UMETA(DisplayName = "X axis (east/west)"),

	/** 남북 방향만. */
	AxisY	UMETA(DisplayName = "Y axis (north/south)")
};

namespace LTTSPuzzle
{
	/** 문이나 방향이 놓인 축: 북쪽을 향한 문은 남북으로 슬라이드한다. */
	inline EPuzzleMoveAxis AxisForDirection(EGridDirection Dir)
	{
		return (Dir == EGridDirection::North || Dir == EGridDirection::South)
			? EPuzzleMoveAxis::AxisY
			: EPuzzleMoveAxis::AxisX;
	}

	/** 90도 회전하면 단일 축 값 둘이 서로 바뀌고 나머지는 그대로다. */
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

	/** 축이 허용하는 두 방향. 음의 방향, 양의 방향 순서다. */
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
