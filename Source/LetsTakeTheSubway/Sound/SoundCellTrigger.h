// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundKeys.h"
#include "SoundCellTrigger.generated.h"

class AGridActor;
class UBoxComponent;

/**
 * 폰이나 행인이 이 박스가 덮는 셀에 들어서면 소리를 낸다. 개찰구 "삑"이 첫 용도다.
 *
 * 볼륨 오버랩을 쓰지 않는 이유는 이 프로젝트의 폰과 행인에게 콜리전이 없기 때문이다(위치는 셀 사이의
 * 보간으로만 정해진다). 대신 그리드가 방송하는 셀 진입(AGridActor::OnPawnEnteredCell)을 듣는다.
 * 셀 규칙(UGridCellRule)에 매달지 않는 이유는 규칙이 경로 탐색 중 여러 번 불리고 부수 효과가 금지돼
 * 있기 때문이다.
 *
 * 박스는 에디터에서 영역을 보이게 할 뿐 충돌하지 않는다. BeginPlay에서 박스의 XY 범위를 셀 목록으로
 * 바꿔 두고, 그 뒤로는 박스를 움직여도 따라가지 않는다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, Replication))
class LETSTAKETHESUBWAY_API ASoundCellTrigger : public AActor
{
	GENERATED_BODY()

public:
	ASoundCellTrigger();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 낼 소리. DA_SoundLibrary의 키. */
	UPROPERTY(EditAnywhere, Category = "Sound Trigger")
	FName SoundKey = LTTSSoundKeys::WorldTurnstile;

	/** 행인(AGridNPC)이 들어설 때 낸다. */
	UPROPERTY(EditAnywhere, Category = "Sound Trigger")
	bool bTriggerOnNPC = true;

	/** 플레이어 폰(AGridPawn)이 들어설 때 낸다. */
	UPROPERTY(EditAnywhere, Category = "Sound Trigger")
	bool bTriggerOnPlayer = true;

	/** 켜면 들어선 셀 위치에서 3D로, 끄면 위치 없이 낸다. */
	UPROPERTY(EditAnywhere, Category = "Sound Trigger")
	bool bSpatial = true;

	/**
	 * 이 트리거가 다시 울리기까지의 최소 간격(초).
	 *
	 * 라이브러리 항목의 MinRetriggerSeconds와 별개다. 그쪽은 키 전체에, 이쪽은 이 트리거 하나에 걸린다.
	 * 개찰구 한 줄에 행인 무리가 동시에 지나갈 때 삑 소리가 한 덩어리로 뭉개지지 않게 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Sound Trigger", meta = (ClampMin = 0.0))
	float MinRetriggerSeconds = 0.15f;

	/** 덮는 셀 목록. BeginPlay 뒤에만 채워진다. */
	const TSet<FIntPoint>& GetCells() const { return Cells; }

private:
	UFUNCTION()
	void HandlePawnEnteredCell(APawn* Pawn, FIntPoint Cell);

	UPROPERTY(VisibleAnywhere, Category = "Sound Trigger")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	TSet<FIntPoint> Cells;

	double LastTriggerTime = -1.0;
};
