// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Grid/GridTypes.h"
#include "Puzzle/PuzzleRotatingObstacle.h"
#include "PuzzleRotatingPillar.generated.h"

/**
 * 1x1 셀 기둥으로, 레버로 한 번에 90도씩 제자리에서 돈다.
 *
 * 홈 파인 큰 장애물과 회전 규칙(동승 판정 → 회전 경로 검사 → 목적지 바닥 검사 → 폰
 * 밀어내기 → 커밋)은 완전히 같고, 다른 것은 둘뿐이다.
 *
 * - 부착 면을 홈에서 유도하지 않고 디자이너가 동서남북 네 면 각각에 대해 직접 켜고 끈다.
 *   켜진 면에 변을 맞대고 있는 블록이 기둥과 함께 돈다. 면은 기둥의 로컬 프레임 기준이라
 *   기둥이 돌면 면도 따라 돈다.
 * - 본체는 원기둥이라 이웃 셀을 스치지 않는다. 회전 경로 검사는 동승 블록만 대상으로
 *   하므로, 부착 면이 아닌 쪽에 붙어 있는 블록은 회전을 막지 않고 그 자리에 남는다.
 *
 * 풋프린트는 **정사각형**이면 된다(기본 1x1). 두 변이 같으면 홀짝 규칙이 저절로 만족되고
 * 회전 정사각형이 풋프린트와 정확히 겹친다. 2 m짜리 아트 기둥은 2x2로 둔다.
 * 높이는 기본 4 m(셀 4칸)이며 프로퍼티로 조정한다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleRotatingPillar : public APuzzleRotatingObstacle
{
	GENERATED_BODY()

public:
	APuzzleRotatingPillar();

	// ---------------------------------------------------------------- 저작

	/** 북쪽(+Y, 로컬 프레임) 면에 붙은 블록이 함께 돈다. */
	UPROPERTY(EditAnywhere, Category = "Rotating Pillar")
	bool bAttachNorth = true;

	/** 동쪽(+X, 로컬 프레임) 면에 붙은 블록이 함께 돈다. */
	UPROPERTY(EditAnywhere, Category = "Rotating Pillar")
	bool bAttachEast = false;

	/** 남쪽(-Y, 로컬 프레임) 면에 붙은 블록이 함께 돈다. */
	UPROPERTY(EditAnywhere, Category = "Rotating Pillar")
	bool bAttachSouth = false;

	/** 서쪽(-X, 로컬 프레임) 면에 붙은 블록이 함께 돈다. */
	UPROPERTY(EditAnywhere, Category = "Rotating Pillar")
	bool bAttachWest = false;

	// ---------------------------------------------------------------- 조회

	/** 기둥이 서 있는 셀. */
	FIntPoint GetPillarCell() const { return GetRect().Min; }

	/** 저작한 로컬 방향의 면이 부착 면인지. */
	bool IsLocalAttachFace(EGridDirection LocalDir) const;

	/** 지금까지 돈 횟수를 반영해, 이 월드 방향의 면이 부착 면인지. */
	bool IsWorldAttachFace(EGridDirection WorldDir) const;

	/** 지금 부착 면인 월드 방향들. */
	void GetWorldAttachDirections(TArray<EGridDirection>& OutDirs) const;

	/** 부착 면에 변을 맞댄 셀이 하나라도 있으면 true. 홈이 없으므로 기둥 바깥 면 기준이다. */
	virtual bool IsAttachedToInnerFace(const APuzzleBlock& Block) const override;

	// ---------------------------------------------------------------- 생명주기

	virtual void GatherOccupiedCells(TArray<FIntPoint>& OutCells) const override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

protected:
	virtual void RefreshVisual() override;
	virtual void NormaliseAuthoring() override;

	/** 원기둥은 이웃 셀을 스치지 않는다. 동승 블록만 회전 경로에 넣는다. */
	virtual bool SweepsOwnCells() const override { return false; }
};
