// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Grid/GridActor.h"

/**
 * 셀의 사각 영역: 마커, 퍼즐 블록, 회전 타일이 덮는 모양이다.
 *
 * Min은 낮은 쪽 모서리이고 Size는 개수이므로, 사각 영역은 두 축 모두
 * [Min, Min + Size) 범위를 차지하며 빈 사각 영역을 실수로 표현하는 일은
 * 불가능하다.
 */
struct FGridRect
{
	FIntPoint Min = FIntPoint::ZeroValue;
	FIntPoint Size = FIntPoint(1, 1);

	FGridRect() = default;
	FGridRect(FIntPoint InMin, FIntPoint InSize) : Min(InMin), Size(InSize) {}

	/** 높은 쪽 모서리 바로 다음. 반열린 범위와 짝을 이룬다. */
	FIntPoint MaxExclusive() const { return Min + Size; }

	/** 높은 쪽 모서리 그 자체 -- 실제로 덮이는 마지막 셀. */
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
 * 셀 사각 영역의 배치·회전 계산. 박스 마커와 퍼즐 액터들이 공유해, 풋프린트 중심에 놓인
 * 액터가 실제로 어디에 있는지에 대해 모두 같은 답을 내게 한다.
 */
namespace GridFootprint
{
	/**
	 * 중심이 Centre에 있는 사각 영역의 Min 모서리 셀.
	 *
	 * 4분의 1 셀만큼 밀어 주는 것은, 정확히 셀 경계에 떨어진 중심이 한 셀 낮게 내림되는
	 * 것을 막기 위해서다. 이 함수가 대체하는 AGridBoxMarker::GetMinCell과 같은
	 * 규칙이다.
	 */
	inline FIntPoint MinCellFromCentre(const AGridActor& Grid, const FVector& Centre, FIntPoint Size)
	{
		const FVector HalfSpan(Size.X * Grid.CellSize * 0.5, Size.Y * Grid.CellSize * 0.5, 0.0);
		const FVector Nudge(Grid.CellSize * 0.25, Grid.CellSize * 0.25, 0.0);
		return Grid.WorldToCell(Centre - HalfSpan + Nudge);
	}

	/** 주어진 높이에서의 사각 영역 월드 중심. MinCellFromCentre의 역함수다. */
	inline FVector CentreFromMinCell(const AGridActor& Grid, FIntPoint Min, FIntPoint Size, double Z)
	{
		const FVector Origin = Grid.GetGridOrigin();
		return FVector(
			Origin.X + (Min.X + Size.X * 0.5) * Grid.CellSize,
			Origin.Y + (Min.Y + Size.Y * 0.5) * Grid.CellSize,
			Z);
	}

	/**
	 * N x N 영역 안의 셀을 영역 로컬 좌표로 회전한다.
	 *
	 * 양의 한 번 회전은 요 +90도다. 셀 중심 기준으로 계산하면 로컬 셀 l은
	 * N/2 + Rot90(l + 0.5 - N/2) - 0.5로 가고, 이는 (N-1-l.y, l.x)로 정리된다. N = 4면
	 * (3,3)이 (0,3)으로 간다: 최대 X, 최대 Y에 있던 모서리가 최대 X, 최소 Y에서 끝난다.
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
	 * 정사각 영역 안에서 사각 영역을 회전한다.
	 *
	 * Min 모서리를 해석적으로 유도하는 대신 양 끝 모서리를 각각 변환한 뒤 다시 합친다:
	 * 홀수 번 회전에서는 두 모서리의 역할이 뒤바뀌는데, 성분별 최솟값을 취하면 특수 처리
	 * 없이 어떤 회전 횟수에서도 올바르다.
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
