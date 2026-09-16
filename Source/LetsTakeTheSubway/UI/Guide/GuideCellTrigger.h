// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/Guide/GuideType.h"
#include "GuideCellTrigger.generated.h"

class AGridActor;
class UBoxComponent;

/**
 * 플레이어 폰이 이 박스가 덮는 셀에 들어서면 가이드 팝업을 요청한다(2026-09-16).
 *
 * Stage2 개찰구 앞의 [회전 기둥과 레버](Stage2_1)가 첫 용도다. 튜토리얼의 [엘리베이터 이동](Tutorial1)처럼
 * "어디에 다가갔을 때" 뜨는 가이드는 이 액터를 놓고 Guide만 바꾸면 된다.
 *
 * ASoundCellTrigger와 같은 이유로 볼륨 오버랩 대신 그리드의 셀 진입 방송(AGridActor::OnPawnEnteredCell)을
 * 듣는다. 폰에게 콜리전이 없기 때문이다. 박스는 에디터에서 영역을 보여 줄 뿐 충돌하지 않고,
 * BeginPlay에서 박스의 XY 범위를 셀 목록으로 바꿔 둔다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, Replication))
class LETSTAKETHESUBWAY_API AGuideCellTrigger : public AActor
{
	GENERATED_BODY()

public:
	AGuideCellTrigger();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 요청할 가이드. None이면 아무것도 하지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Guide Trigger")
	EGuideType Guide = EGuideType::None;

	/** 덮는 셀 목록. BeginPlay 뒤에만 채워진다. */
	const TSet<FIntPoint>& GetCells() const { return Cells; }

private:
	UFUNCTION()
	void HandlePawnEnteredCell(APawn* Pawn, FIntPoint Cell);

	void Unbind();

	UPROPERTY(VisibleAnywhere, Category = "Guide Trigger")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	TSet<FIntPoint> Cells;
};
