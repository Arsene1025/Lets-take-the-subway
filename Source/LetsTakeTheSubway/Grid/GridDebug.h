// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

class AGridActor;
class UWorld;

/** ltts.GridDebug -- 0 off, 1 HUD text only, 2 HUD text plus in-world path and hover. */
extern TAutoConsoleVariable<int32> CVarGridDebug;

namespace LTTSGridDebug
{
	inline int32 GetLevel() { return CVarGridDebug.GetValueOnGameThread(); }
	inline bool ShouldDrawHUD() { return GetLevel() >= 1; }
	inline bool ShouldDrawWorld() { return GetLevel() >= 2; }
}

/**
 * Runtime in-world debug drawing.
 *
 * Uses the persistent line batcher with explicit batch ids instead of per-frame DrawDebug
 * calls: the path and hover only change on click or on arrival, so redrawing them every
 * frame would be waste, and DrawDebug's one-second default lifetime would stack copies.
 */
struct FGridRuntimeDebugDrawer
{
	static void DrawPath(UWorld* World, const AGridActor& Grid, FIntPoint CurrentCell, const TArray<FIntPoint>& Path);
	static void ClearPath(UWorld* World);

	static void DrawHover(UWorld* World, const AGridActor& Grid, FIntPoint Cell, bool bEnterable);
	static void ClearHover(UWorld* World);

	static void ClearAll(UWorld* World);
};
