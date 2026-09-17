// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/CutScene/CutSceneDatabase.h"
#include "CutsceneCellTrigger.generated.h"

class AGridActor;
class UBoxComponent;
class UWorld;

/**
 * 플레이어 폰이 이 박스가 덮는 셀에 들어서면 그림 컷씬을 띄우고, 끝나면 NextLevel을 연다(2026-09-17).
 *
 * 기획의 "엔딩 볼륨"이다. Subway_Stage2의 끝에 놓고 Kind = Ending, NextLevel = Subway_Title로 두면
 * 엔딩 컷씬 뒤 타이틀로 돌아간다(Docs/Plans/GameLoop.md 3.5절).
 *
 * AGuideCellTrigger와 같은 이유로 볼륨 오버랩 대신 그리드의 셀 진입 방송(AGridActor::OnPawnEnteredCell)을
 * 듣는다. 폰에게 콜리전이 없고, 폰의 ZoneProbe는 구역 볼륨 채널만 본다. 박스는 에디터에서 영역을 보여 줄
 * 뿐 충돌하지 않고, BeginPlay에서 박스의 XY 범위를 셀 목록으로 바꿔 둔다. 셀 한 칸(100 cm) 이상 덮어야 한다.
 *
 * 컷씬은 UUIManagerSubsystem::PlayCutscene이 띄운다. 그 동안 입력이 막혀 폰은 그 자리에 선다. 폰이나
 * 그리드를 파괴하거나 언포제스하지 않는다. 컷씬이 끝나면 매니저가 레벨을 열고 이 액터는 월드와 함께 사라진다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, Replication))
class LETSTAKETHESUBWAY_API ACutsceneCellTrigger : public AActor
{
	GENERATED_BODY()

public:
	ACutsceneCellTrigger();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 띄울 컷씬. 데이터 애셋은 UUISettings에서 찾는다. */
	UPROPERTY(EditAnywhere, Category = "Cutscene Trigger")
	ECutsceneKind Kind = ECutsceneKind::Ending;

	/** 컷씬 뒤 열 레벨. 비워 두면 레벨을 바꾸지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Cutscene Trigger")
	TSoftObjectPtr<UWorld> NextLevel;

	/** 셀에 들어선 뒤 컷씬까지 기다리는 시간(초). 0이면 곧바로. */
	UPROPERTY(EditAnywhere, Category = "Cutscene Trigger", meta = (ClampMin = 0.0, Units = "s"))
	float Delay = 0.0f;

	/** 한 번만 발동할지. 레벨이 다시 열리면 다시 발동한다. */
	UPROPERTY(EditAnywhere, Category = "Cutscene Trigger")
	bool bOnce = true;

	/** 덮는 셀 목록. BeginPlay 뒤에만 채워진다. */
	const TSet<FIntPoint>& GetCells() const { return Cells; }

private:
	UFUNCTION()
	void HandlePawnEnteredCell(APawn* Pawn, FIntPoint Cell);

	void Fire();
	void Unbind();

	UPROPERTY(VisibleAnywhere, Category = "Cutscene Trigger")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	TSet<FIntPoint> Cells;

	bool bFired = false;

	FTimerHandle DelayTimer;
};
