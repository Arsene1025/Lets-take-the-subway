// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AGridActor;
class APawn;

/**
 * 4방향 이웃 그리드 위의 A*.
 *
 * 이웃은 FGridCellData::NeighborMask에서 가져오는데, 여기에 "범위 안"과 "단차 높이 통과
 * 가능"이 이미 담겨 있으므로 탐색은 폰이 셀에 들어갈 수 있는지만 그리드에 물으면 된다.
 * 걸음당 비용은 1이고 휴리스틱은 맨해튼 거리로, 4연결 균일 그리드에서 정확히
 * admissible하다.
 */
struct FGridPathfinder
{
	/**
	 * @param Pawn      Conditional 셀 판정에 쓴다. null이어도 된다(그러면 규칙은 null을 본다).
	 * @param OutPath   지나갈 셀 목록: Start는 제외하고 Goal로 끝난다.
	 * @return          경로가 없으면 false. Start == Goal이면 빈 경로와 함께 true를 돌려준다.
	 */
	static bool FindPath(const AGridActor& Grid, FIntPoint Start, FIntPoint Goal, const APawn* Pawn, TArray<FIntPoint>& OutPath);
};
