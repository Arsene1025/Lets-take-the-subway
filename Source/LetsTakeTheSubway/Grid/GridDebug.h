// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

class AGridActor;
class UWorld;

/** ltts.GridDebug -- 0 끔, 1 HUD 텍스트만, 2 HUD 텍스트에 월드 내 경로와 호버까지. */
extern TAutoConsoleVariable<int32> CVarGridDebug;

namespace LTTSGridDebug
{
	inline int32 GetLevel() { return CVarGridDebug.GetValueOnGameThread(); }
	inline bool ShouldDrawHUD() { return GetLevel() >= 1; }
	inline bool ShouldDrawWorld() { return GetLevel() >= 2; }
}

/**
 * 런타임 월드 내 디버그 표시.
 *
 * 매 프레임 DrawDebug를 호출하는 대신 명시적 batch id를 붙인 영구 라인 배처를 쓴다:
 * 경로와 호버는 클릭하거나 도착할 때만 바뀌므로 매 프레임 다시 그리면 낭비이고,
 * DrawDebug의 기본 수명 1초 때문에 복사본이 겹겹이 쌓인다.
 */
struct FGridRuntimeDebugDrawer
{
	static void DrawPath(UWorld* World, const AGridActor& Grid, FIntPoint CurrentCell, const TArray<FIntPoint>& Path);
	static void ClearPath(UWorld* World);

	static void DrawHover(UWorld* World, const AGridActor& Grid, FIntPoint Cell, bool bEnterable);

	/** 여러 셀의 외곽선을 한 번에 그린다. 풋프린트를 덮는 대상을 호버할 때 쓴다. */
	static void DrawHoverCells(UWorld* World, const AGridActor& Grid, const TArray<FIntPoint>& Cells, bool bEnterable);

	static void ClearHover(UWorld* World);

	static void ClearAll(UWorld* World);
};
