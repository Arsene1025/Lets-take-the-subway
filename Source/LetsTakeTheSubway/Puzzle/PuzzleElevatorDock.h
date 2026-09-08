// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Grid/GridTypes.h"
#include "Puzzle/PuzzleFloorTile.h"
#include "PuzzleElevatorDock.generated.h"

class AGridPawn;
class APuzzleElevatorBlock;
class UMaterialInterface;

/**
 * 엘리베이터를 올려놓으면 층을 오갈 수 있게 되는 바닥 구조물.
 *
 * 기획의 "엘리베이터 구조물"이다. 엘리베이터 자체는 어디서나 밀 수 있지만 Z축 이동은 오직
 * 이 위에서만 일어난다. 그래서 퍼즐의 목표가 분명해진다: 차체를 여기까지 밀어 오는 것.
 *
 * **얼마나 올라가는가**는 숫자가 아니라 목적 층의 셀에서 나온다. TargetFloorCell을 지정하면
 * 그 셀의 바닥 높이가 곧 목표 Z이고, 동시에 폰이 내릴 자리이기도 하다. 고정 높이를 쓰면
 * 층고가 바뀔 때마다 손으로 맞춰야 하고 "내려가는 층 바닥에 닿으면 정지"와도 어긋난다.
 * 셀을 지정하지 않았을 때만 TravelHeight로 물러선다.
 *
 * 단일 그리드로 충분한 이유: 샤프트 셀은 아래층 바닥으로, 출구 셀은 위층 바닥으로 구워지고
 * 둘은 단차가 커서 서로 이어지지 않는다. 즉 걸어서는 층을 오갈 수 없고 차체만이 통로가 된다.
 * 전제는 위층 바닥이 아래층 걸을 수 있는 바닥과 XY로 겹치지 않는 것이다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleElevatorDock : public APuzzleFloorTile
{
	GENERATED_BODY()

public:
	APuzzleElevatorDock();

	// ---------------------------------------------------------------- 저작

	/**
	 * 도착 층에서 폰이 내릴 셀. 이 셀의 바닥 높이가 곧 목표 Z다.
	 *
	 * 유효하지 않은 셀(기본값 -1,-1)이면 TravelHeight로 물러선다. Detect Target Floor
	 * 버튼이 트레이스로 채워 준다.
	 */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock")
	FIntPoint TargetFloorCell = FIntPoint(-1, -1);

	/** 도착 층에서 폰이 나갈 문의 방향. 출구 셀을 찾는 기준이기도 하다. */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock")
	EGridDirection ExitDirection = EGridDirection::East;

	/** TargetFloorCell이 없을 때 쓰는 이동 거리(cm). 기획의 8 m가 기본값이다. */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock", meta = (ClampMin = 0.0))
	float TravelHeight = 800.0f;

	/** TravelHeight로 물러섰을 때 올라갈지 내려갈지. */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock")
	bool bTravelUp = true;

	UPROPERTY(EditAnywhere, Category = "Elevator Dock", meta = (ClampMin = 1.0))
	float TravelSpeed = 200.0f;

	/** 도착한 뒤 폰이 내리기 시작하기까지의 시간(초). 문이 열리는 사이다. */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock", meta = (ClampMin = 0.0))
	float DoorDwellSeconds = 0.5f;

	/**
	 * 목적 층 셀을 트레이스로 찾아 TargetFloorCell에 채운다.
	 *
	 * 구조물 중심에서 ExitDirection으로 한 칸 바깥 지점을 위아래로 훑어, 지금 바닥에서
	 * 가장 가까운 다른 층 바닥을 고른다. 손으로 셀 번호를 세는 것보다 정확하다.
	 */
	UFUNCTION(CallInEditor, Category = "Elevator Dock", meta = (DisplayName = "Detect Target Floor"))
	void DetectTargetFloor();

	// ---------------------------------------------------------------- 조회

	/** 지금 이 구조물 위에 완전히 올라와 있는 엘리베이터. 없으면 null. */
	APuzzleElevatorBlock* GetDockedElevator() const { return DockedElevator.Get(); }

	/**
	 * 목표 Z. 도킹된 차체가 지금 어느 층에 있느냐로 정해진다.
	 *
	 * 구조물 층에 있으면 반대편 층으로, 반대편 층에 있으면 구조물 층으로 간다. 구조물 하나가
	 * 왕복을 담당해야 플레이어가 층을 잘못 골라도 갇히지 않는다.
	 */
	double ComputeTargetZ(const APuzzleElevatorBlock& Elevator) const;

	// ---------------------------------------------------------------- 사용

	/**
	 * 도킹된 차체에 폰을 태워 층을 옮긴다. 못 하면 이유를 알려준다.
	 *
	 * 컨트롤러가 엘리베이터 클릭(TryBoard)에 성공한 뒤 이어서 부른다. 구조물 위가 아닌
	 * 엘리베이터를 클릭하면 여기까지 오지 않고 "구조물 위로 먼저"라는 안내만 뜬다.
	 */
	bool TryLaunch(AGridPawn* Pawn, FText* OutReason = nullptr);

	virtual bool IsBusy() const override;
	virtual void OnBlockCameToRest(APuzzleBlock& Block) override;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual FString DescribeTile() const override;
	virtual void OnTileReady() override;

private:
	/** 구조물 반대편 층의 바닥 높이. TargetFloorCell이 있으면 그 셀에서, 없으면 TravelHeight로. */
	double GetFarFloorZ() const;

	/** 목표 높이에서 폰이 내릴 월드 위치. 올라갈 때와 내려올 때가 다른 셀이다. */
	bool FindExitWorld(const APuzzleElevatorBlock& Elevator, double TargetZ, FVector& OutWorld) const;

	/** 지금 완전히 올라와 있는 엘리베이터를 다시 찾는다. */
	APuzzleElevatorBlock* FindElevatorOnTop() const;

	/** 도킹 여부에 따라 패드 머티리얼을 바꾼다. */
	void RefreshDockedLook();

	TWeakObjectPtr<APuzzleElevatorBlock> DockedElevator;

	/** 비어 있을 때의 패드 머티리얼. 생성자에서 잡아 둔다. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> IdleMaterial;

	/** 차체가 올라와 있을 때의 패드 머티리얼. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ReadyMaterial;

	bool bLookIsReady = false;
};
