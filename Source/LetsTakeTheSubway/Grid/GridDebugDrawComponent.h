// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "GridDebugDrawComponent.generated.h"

/**
 * 셀 좌표 라벨을 독자적인 거리 제한으로 그린다.
 *
 * 기본 헬퍼는 FDebugRenderSceneProxy::FarClippingDistance를 프록시와 공유하므로, 그 방식으로
 * 라벨을 자르면 셀 쿼드까지 잘려 카메라가 조금만 멀어져도 그리드 전체가 사라진다.
 * 라벨 거리를 여기에 두면 프록시는 잘리지 않는다.
 */
struct FGridLabelDrawHelper : public FDebugDrawDelegateHelper
{
	/** 카메라에서 이보다 먼 라벨은 건너뛴다. 0이면 거리 제한을 끈다. */
	double LabelMaxDistance = 0.0;

	/**
	 * 캐시된 라벨을 버린다. 엔진은 새 프록시가 만들어질 때만 라벨을 갱신하므로, 이것이 없으면
	 * 방금 표시를 끈 그리드가 마지막 라벨을 계속 보여 준다.
	 */
	void ClearLabels() { ResetTexts(); }

protected:
	virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController* PlayerController) override;
};

/**
 * 소유자 AGridActor의 셀을 에디터 뷰포트에 그린다.
 *
 * 매 틱 DrawDebug를 호출하는 대신 씬 프록시를 쓴다: DrawDebug 프리미티브는 기본 수명이
 * 1초라 매 프레임 수천 개를 다시 내면 복사본이 쌓이고 실제 시간을 잡아먹는다. 프록시는
 * 그리드가 실제로 바뀔 때(MarkRenderStateDirty)만 다시 만들어지므로, 가만히 있는
 * 에디터는 아무 비용도 치르지 않는다.
 *
 * 프록시는 "Editor" 쇼 플래그로 가려지므로 PIE나 패키징 빌드에는 절대 나타나지 않는다
 * -- 게다가 컴포넌트가 에디터 전용이라 어차피 쿡에서 제거된다.
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
