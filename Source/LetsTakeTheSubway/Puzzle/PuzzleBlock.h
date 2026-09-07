// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridFootprint.h"
#include "Grid/GridTypes.h"
#include "Puzzle/PuzzleTypes.h"
#include "PuzzleBlock.generated.h"

class AGridActor;
class UPuzzleSubsystem;
class UStaticMeshComponent;

/**
 * Rush Hour 조각: 플레이어가 바닥 위에서 밀고 다니는 셀 사각 영역.
 *
 * 블록은 자기 셀을 따로 갖지 않는다. 그리드 액터의 런타임 점유 맵에 셀을 점유(등록)하며,
 * 폰이 블록을 피해 길을 찾고 블록 안으로 걸어 들어가지 않는 것도 이 덕분이다 -- 베이크된
 * 셀 데이터는 전혀 건드리지 않으므로, 블록이 어디에 있든 레벨의 걸을 수 있는 바닥은
 * 디자이너가 생성한 그대로 유지된다.
 *
 * 액터는 AGridBoxMarker와 마찬가지로 풋프린트 중심에 놓이므로, 회전은 그저 "타일 중심을
 * 기준으로 위치를 돌리고 yaw에 90도를 더한다"일 뿐이다.
 *
 * FootprintSize, MoveAxis, 그리고 (엘리베이터의) 문은 모두 블록 자신의 로컬 프레임에서
 * 지정한다. 월드 공간 버전은 QuarterTurns에서 유도하며, QuarterTurns는 액터의 yaw에서
 * 나온다. 그래서 에디터 기즈모로 블록을 돌리는 것과 런타임에 타일 위에서 돌리는 것이
 * 정확히 같은 코드를 거친다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleBlock : public AActor
{
	GENERATED_BODY()

public:
	APuzzleBlock();

	/** 블록이 비주얼상 무엇을 하고 있는지. 점유는 언제나 이미 최종 상태다. */
	enum class EAnimState : uint8
	{
		Idle,
		Sliding,
		Rotating
	};

	// ---------------------------------------------------------------- 편집 설정

	/** 블록 자신의 프레임 기준 셀 단위 풋프린트. 동서로 놓인 1x3은 1x3으로 지정하고 yaw를 90으로 둔다. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 1, ClampMax = 16))
	FIntPoint FootprintSize = FIntPoint(1, 3);

	/**
	 * 블록 자신의 프레임 기준으로 블록을 밀 수 있는 방향.
	 *
	 * 기본값은 양 축 모두 자유로운 상태로, 기획의 "네 방향 모두 슬라이드하는" 기본 오브젝트에
	 * 맞춘 것이다. Rush Hour 자동차처럼 움직여야 하는 조각은 한 축으로 고정한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block")
	EPuzzleMoveAxis MoveAxis = EPuzzleMoveAxis::Both;

	/** 비주얼 높이(cm). 퍼즐에는 영향이 없다 -- 그리드는 2차원이다. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 10.0))
	float Height = 150.0f;

	/** 슬라이드 속도(cm/s). 600이면 1 m 셀 하나에 약 1/6초가 걸린다. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 1.0))
	float SlideSpeed = 600.0f;

	/**
	 * 한 셀 이동 후 멈추고, 플레이어가 놓았다가 다시 잡을 때까지 기다린다.
	 *
	 * 기본값은 켜짐: 기획은 밀 수 있는 모든 조각에서 드래그 한 번이 한 걸음이기를 요구하므로,
	 * 커서를 플랫폼 위로 쓸어 넘기는 게 아니라 셀 수 있는 이동 횟수로 퍼즐을 풀게 된다.
	 * 끄면 예전의 연속 드래그로 돌아가는데, 드래그 코드의 모서리 꺾기 동작이 그것을 위해
	 * 작성됐고 테스트에도 여전히 유용하다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block")
	bool bOneStepPerDrag = true;

	// --- CUTAWAY DISABLED 2026-09-04 -------------------------------------------------
	// 폰을 가리는 블록을 납작하게 만드는 기능은 당분간 꺼 둔다. 다시 켤 수 있도록 코드는
	// 지우지 않고 남겨 둔다: 이 마커를 검색해서 가드된 블록을 모두 복원하고, 두
	// RefreshVisual 오버라이드에 GetVisualHeight()를 다시 넣으면 된다. 되살릴 때 가림
	// 판정이 왜 해석적으로 유지돼야 하는지는 Docs/Plans/RushHourPuzzle.md 9절을
	// 참고한다.
#if 0
	/** 블록이 폰을 가리는 동안 줄어드는 높이. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block|Cutaway", meta = (ClampMin = 1.0))
	float CutawaySlabHeight = 20.0f;

	/** 블록을 눌러 내리는 데, 그리고 다시 올리는 데 걸리는 초. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block|Cutaway", meta = (ClampMin = 0.01))
	float CutawayBlendTime = 0.15f;
#endif

	// ---------------------------------------------------------------- 조회

	AGridActor* GetGrid() const { return Grid; }

	/** 지정된 프레임에서 yaw가 90도 회전한 횟수, 0~3. */
	int32 GetQuarterTurns() const { return QuarterTurns; }

	/** 그리드에 놓인 상태의 풋프린트: 지정된 크기를 홀수 회 90도 회전 시 뒤바꾼 것. */
	FIntPoint GetWorldFootprint() const;

	FGridRect GetRect() const { return FGridRect(MinCell, GetWorldFootprint()); }

	/**
	 * 이 블록이 그리드에서 점유하는 셀.
	 *
	 * 일반 블록은 풋프린트 전체다. 가상 함수인 이유는 조각이 구멍 뚫린 사각형일 수
	 * 있어서다: 돌아가는 장애물은 홈(채널)을 비워 두어 블록과 폰이 그 안에 설 수 있게
	 * 한다.
	 */
	virtual void GatherOccupiedCells(TArray<FIntPoint>& OutCells) const;

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	/**
	 * 지금 어떤 높이로 그려지든, 지정된 높이대로라면 블록이 채울 부피.
	 *
	 * 일부러 컷어웨이와 독립적으로 둔다: 블록을 납작하게 할지 결정하는 가림 판정이 이 값을
	 * 읽는데, 대신 현재 높이를 읽으면 납작해진 블록이 가림을 멈추고, 일어서고, 다시 가리며
	 * 매 프레임 진동하게 된다.
	 */
	FBox GetFullBounds() const;
#endif

	/** 회전을 반영해 지금 이 블록을 밀 수 있는 방향. */
	virtual EPuzzleMoveAxis GetWorldMoveAxis() const;

	bool IsAnimating() const { return AnimState != EAnimState::Idle; }

	bool IsHeld() const { return bHeld; }

	// ---------------------------------------------------------------- 컷어웨이
	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	/** 블록에 납작해지라고(또는 다시 일어서라고) 요청한다. 블렌드되므로 반복 호출해도 부담이 없다. */
	void SetCutaway(bool bInCutaway);

	bool IsCutaway() const { return bCutawayTarget; }

	/**
	 * 지금 몸체가 그려지는 높이.
	 *
	 * 콜리전이 메시와 함께 움직이므로 납작해진 블록은 정말로 슬랩이 된다: 윗면은 여전히
	 * 위를 향하고 여전히 잡을 수 있으며, 그저 더 낮아졌을 뿐이다.
	 */
	float GetVisualHeight() const { return FMath::Lerp(Height, CutawaySlabHeight, CutawayAlpha); }
#endif

	/** 블록이 서 있는 바닥의 높이. BeginPlay에서 한 번만 기록한다. */
	double GetFloorZ() const { return FloorZ; }

	// ---------------------------------------------------------------- 이동

	/** 지금 당장 그 방향으로 한 걸음 갈 수 있으면 true. */
	bool CanSlide(EGridDirection Dir, FText* OutReason = nullptr) const;

	/** 한 셀 이동을 시작한다. 셀은 즉시 점유되므로 이동 중에 다른 것이 차지할 수 없다. */
	bool StartSlide(EGridDirection Dir);

	/**
	 * 블록이 커서에 잡혔다고 표시한다.
	 *
	 * 회전은 잡힌 블록을 놓고 마지막 걸음이 끝났을 때 검사하지, 걸음마다 도착할 때 검사하지
	 * 않는다: 그렇게 하면 회전 타일 위로 블록을 드래그하는 도중에 블록이 커서 아래에서
	 * 빙글 돌아 빠져나가 버린다.
	 */
	void SetHeld(bool bInHeld);

	/**
	 * 회전의 비주얼 쪽 절반을 시작한다. 호출자가 이미 결과를 결정했고 목적지 사각 영역을
	 * 넘겨 준다.
	 *
	 * 점유, MinCell, QuarterTurns는 애니메이션이 끝날 때가 아니라 여기서 확정한다. 그래서
	 * 회전 중의 어떤 조회도 이미 최종 배치를 보게 되고, 블록이 향해 가는 셀로 아무것도
	 * 들어갈 수 없다.
	 */
	void BeginRotation(const FVector& Pivot, int32 TurnSign, float Duration, const FGridRect& NewRect);

	// ---------------------------------------------------------------- 생명 주기

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** 몸체의 크기와 색을 다시 맞춘다. 풋프린트나 높이가 바뀔 때마다 호출된다. */
	virtual void RefreshVisual();

	/**
	 * 서브시스템의 등록부에 들어간다. 조각이 다른 목록에 들어갈 수 있도록 오버라이드한다:
	 * 돌아가는 장애물이 밀 수 있는 블록 사이에 나타나면 안 되는데, 그러면 드래그 코드가
	 * 그것을 밀려 들고 회전 타일이 그것을 낯선 조각으로 취급한다.
	 */
	virtual void RegisterWithSubsystem(UPuzzleSubsystem& Subsystem);
	virtual void UnregisterFromSubsystem(UPuzzleSubsystem& Subsystem);

	/** 이전에 점유했던 셀을 모두 풀고 현재 셀을 그리드에 점유(등록)한다. */
	void ClaimCells();

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Block")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Block")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 점유한 사각 영역의 최소 모서리, 그리드 셀 단위. */
	FIntPoint MinCell = FIntPoint::ZeroValue;

	int32 QuarterTurns = 0;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

private:
	/** 액터를 현재 사각 영역의 정확한 중심으로 옮긴다. */
	void SnapToRect();

	/** 회전 타일이 반응할 수 있도록 블록이 정지했음을 서브시스템에 알린다. */
	void ReportAtRest();

	EAnimState AnimState = EAnimState::Idle;

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	bool bCutawayTarget = false;

	/** 0이면 원래 높이, 1이면 완전히 납작해진 상태. */
	float CutawayAlpha = 0.0f;
#endif

	double FloorZ = 0.0;

	bool bHeld = false;

	/** 블록을 잡은 뒤 이동한 걸음 수. 놓을 때 0이면 드래그가 아니라 클릭이었다는 뜻이다. */
	int32 StepsWhileHeld = 0;

	FVector SlideTarget = FVector::ZeroVector;

	FVector RotationPivot = FVector::ZeroVector;
	FVector RotationStartLocation = FVector::ZeroVector;
	FVector RotationTargetLocation = FVector::ZeroVector;
	double RotationStartYaw = 0.0;
	double RotationTargetYaw = 0.0;
	int32 RotationTurnSign = 1;
	float RotationDuration = 0.4f;
	float RotationElapsed = 0.0f;
};
