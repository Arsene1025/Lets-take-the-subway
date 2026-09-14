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

/**
 * 위에서 내려다본 회전 방향.
 *
 * 레버와 회전 타일이 같은 타입을 공유하되, Free가 무엇을 뜻하는지는 서로 다르다:
 * 레버는 플레이어가 휠을 돌린 쪽으로 돌고, 회전 타일은 발동할 때마다 번갈아 돌린다. 둘 다
 * "어느 한 방향으로 고정되지 않았다"는 점에서는 같으므로 하나의 설정 값으로 묶는다.
 *
 * 각도가 자유롭다는 뜻이 아니다. 회전은 여전히 90도 단위이며, 그것은 정수 셀 점유 모델이
 * 요구하는 조건이다.
 */
UENUM(BlueprintType)
enum class EPuzzleRotationDirection : uint8
{
	/** 항상 시계 방향(TurnSign +1). */
	Clockwise			UMETA(DisplayName = "Clockwise"),

	/** 항상 반시계 방향(TurnSign -1). */
	CounterClockwise	UMETA(DisplayName = "Counter-clockwise"),

	/** 한 방향으로 고정하지 않는다. 구체적인 의미는 사용하는 클래스가 정한다. */
	Free				UMETA(DisplayName = "Free (either way)")
};

namespace LTTSPuzzle
{
	/** 부호 관례를 한군데 모아 둔다: +1이 시계 방향이다. */
	inline const TCHAR* DescribeTurnSign(int32 TurnSign)
	{
		return (TurnSign >= 0) ? TEXT("clockwise") : TEXT("counter-clockwise");
	}

	/** 이 설정이 주어진 TurnSign을 허용하는지. Free는 무엇이든 받는다. */
	inline bool DirectionAllowsTurn(EPuzzleRotationDirection Direction, int32 TurnSign)
	{
		switch (Direction)
		{
		case EPuzzleRotationDirection::Clockwise:			return TurnSign >= 0;
		case EPuzzleRotationDirection::CounterClockwise:		return TurnSign < 0;
		default:											return true;
		}
	}

	/**
	 * 고정된 방향의 TurnSign. Free에는 정답이 없으므로 FallbackSign을 그대로 돌려준다.
	 *
	 * 호출하는 쪽이 Free를 어떻게 해석할지(플레이어 입력, 번갈아 돌리기) 정하게 한다.
	 */
	inline int32 ResolveTurnSign(EPuzzleRotationDirection Direction, int32 FallbackSign)
	{
		switch (Direction)
		{
		case EPuzzleRotationDirection::Clockwise:			return 1;
		case EPuzzleRotationDirection::CounterClockwise:		return -1;
		default:											return (FallbackSign >= 0) ? 1 : -1;
		}
	}

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
