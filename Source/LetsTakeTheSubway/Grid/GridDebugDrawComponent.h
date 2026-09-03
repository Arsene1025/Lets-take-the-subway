// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "GridDebugDrawComponent.generated.h"

/**
 * Draws the cell coordinate labels with their own distance cut-off.
 *
 * The stock helper shares FDebugRenderSceneProxy::FarClippingDistance with the proxy, so
 * clipping labels that way also clips the cell quads and the whole grid vanishes as soon as
 * the camera backs away. Keeping the label distance here leaves the proxy unclipped.
 */
struct FGridLabelDrawHelper : public FDebugDrawDelegateHelper
{
	/** Labels farther than this from the camera are skipped. 0 disables the cut-off. */
	double LabelMaxDistance = 0.0;

protected:
	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController* PlayerController) override;
};

/**
 * Draws the owning AGridActor's cells in editor viewports.
 *
 * A scene proxy rather than per-tick DrawDebug calls: DrawDebug primitives live for a
 * second by default, so re-issuing thousands of them every frame stacks up copies and
 * costs real time. The proxy is rebuilt only when the grid actually changes
 * (MarkRenderStateDirty), so an idle editor pays nothing.
 *
 * The proxy is gated on the "Editor" show flag, so it never appears in PIE or a packaged
 * build -- and the component is editor-only, so it is stripped on cook anyway.
 */
UCLASS(NotBlueprintable, ClassGroup = Debug)
class LETSTAKETHESUBWAY_API UGridDebugDrawComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UGridDebugDrawComponent();

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FDebugDrawDelegateHelper& GetDebugDrawDelegateHelper() override { return LabelHelper; }

private:
	FGridLabelDrawHelper LabelHelper;
};
