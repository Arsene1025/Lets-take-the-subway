// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "Grid/GridTypes.h"
#include "GridActor.generated.h"

class UGridCellRule;
class UGridDebugDrawComponent;
class AGridCellMarkerBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGridStageClear, APawn*, Pawn, FIntPoint, Cell);

/**
 * 걸을 수 있는 그리드를 소유한다. 레벨을 트레이스해 그리드를 생성하고, 그 위에 디자이너의
 * 마커 오버라이드를 합친 뒤, 런타임에 이동 질의에 답한다.
 *
 * 액터 위치가 그리드 원점이다(셀 (0,0)이 거기서 시작하고 그리드는 +X/+Y 방향으로 뻗는다).
 * 회전과 스케일은 의도적으로 무시한다 -- 회전된 그리드는 "1m 셀"의 의미를 모호하게 만들고
 * 월드/셀 변환 비용만 늘릴 뿐 게임플레이상 이득이 없다.
 *
 * 생성된 데이터는 맵에 직렬화되므로, bRegenerateOnPlay가 켜져 있지 않으면 패키징 빌드는
 * 트레이스를 전혀 수행하지 않는다.
 */
UCLASS(HideCategories = (Rendering, Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, WorldPartition, Replication))
class LETSTAKETHESUBWAY_API AGridActor : public AActor
{
	GENERATED_BODY()

public:
	AGridActor();

	// ---------------------------------------------------------------- 생성 설정

	/** 액터 위치에서 +X, +Y 방향으로 뻗는 그리드 크기(셀 단위). */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = 1, ClampMax = 512))
	FIntPoint SizeInCells = FIntPoint(80, 80);

	/** 셀 하나는 1 m다. 설계상 고정값이며, 값을 찾아볼 수 있도록 여기에 표시한다. */
	UPROPERTY(VisibleAnywhere, Category = "Grid")
	float CellSize = 100.0f;

	/** 바닥 트레이스는 ActorZ + RegionHeight에서 ActorZ까지 내려온다. 여유를 크게 두지 말 것: 영역 위에 있는 것이 먼저 맞는다. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = 1.0))
	float RegionHeight = 1000.0f;

	/** 이웃한 두 셀은 바닥 높이 차이가 이 값 이하일 때만 연결된다. 계단 한 단의 허용 높이를 정한다. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = 0.0))
	float MaxStepHeight = 50.0f;

	/** 이보다 가파른 바닥은 Blocked/Slope가 된다. 이보다 완만한 경사로는 걸을 수 있는 상태로 남는다. */
	UPROPERTY(EditAnywhere, Category = "Grid", meta = (ClampMin = 0.0, ClampMax = 89.0))
	float MaxSlopeAngle = 35.0f;

	/** 각 바닥 위로 박스를 스윕해 벽이나 기둥에 파묻혔거나 낮은 천장 아래에 있는 셀을 걸러낸다. */
	UPROPERTY(EditAnywhere, Category = "Grid")
	bool bClearanceTest = true;

	UPROPERTY(EditAnywhere, Category = "Grid", meta = (EditCondition = "bClearanceTest", ClampMin = 0.0))
	float ClearanceHeight = 180.0f;

	UPROPERTY(EditAnywhere, Category = "Grid", meta = (EditCondition = "bClearanceTest", ClampMin = 1.0))
	float ClearanceHalfWidth = 40.0f;

	UPROPERTY(EditAnywhere, Category = "Grid")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** 직렬화된 데이터를 믿지 않고 플레이 시 다시 트레이스한다. 저장된 오버라이드는 어느 쪽이든 다시 적용된다. */
	UPROPERTY(EditAnywhere, Category = "Grid")
	bool bRegenerateOnPlay = false;

	/** 생성 설정이나 액터 위치가 바뀌면 자동으로 다시 생성한다. */
	UPROPERTY(EditAnywhere, Category = "Grid|Editor")
	bool bAutoRegenerateOnEdit = true;

	/** 마커가 이동·편집·삭제되는 즉시 오버라이드를 다시 굽는다. */
	UPROPERTY(EditAnywhere, Category = "Grid|Editor")
	bool bLiveApplyMarkerOverrides = true;

	// ---------------------------------------------------------------- 디버그 설정

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	bool bDrawGridInEditor = true;

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	bool bDrawCellCoords = false;

	/** 이 거리보다 먼 좌표 라벨은 건너뛴다. 셀 쿼드는 절대 잘리지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Grid|Debug", meta = (ClampMin = 0.0))
	float CoordLabelMaxDistance = 2500.0f;

	/** 라벨 개수 상한 -- 라벨 하나가 캔버스 드로우 하나다. */
	UPROPERTY(EditAnywhere, Category = "Grid|Debug", meta = (ClampMin = 0))
	int32 CoordLabelMaxCount = 1500;

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	bool bDrawNoFloorCells = false;

	/** 단차 높이 규칙 때문에 연결이 끊긴 셀 경계를 표시한다. 계단을 튜닝할 때 유용하다. */
	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	bool bDrawStepBreaks = true;

	// ---------------------------------------------------------------- 통계 (읽기 전용)

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumWalkable = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumBlocked = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumNoFloor = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumStageClear = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumConditional = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumBlockedBySlope = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumBlockedByClearance = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumOverridesApplied = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	int32 NumStepBreaks = 0;

	UPROPERTY(VisibleAnywhere, Category = "Grid|Stats")
	FString LastGenerated;

	// ---------------------------------------------------------------- 데이터

	UPROPERTY()
	TArray<FGridCellData> Cells;

	/** 레벨의 마커에서 구운 데이터. 쿡 후에도 남지만, 마커 자체는 남지 않는다. */
	UPROPERTY()
	TArray<FGridCellOverride> Overrides;

	/** 마커 규칙의 복사본. 쿡 후에도 남도록 이 액터가 소유한다. */
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Grid|Stats")
	TArray<TObjectPtr<UGridCellRule>> ConditionalRules;

	// ---------------------------------------------------------------- 이벤트

	UPROPERTY(BlueprintAssignable, Category = "Grid")
	FOnGridStageClear OnStageClear;

	// ---------------------------------------------------------------- 에디터 동작

	/** 레벨을 트레이스하고, 마커 오버라이드를 구워 적용한다. */
	UFUNCTION(CallInEditor, Category = "Grid", meta = (DisplayName = "Generate Grid"))
	void GenerateGrid();

	/** 다시 트레이스하지 않고 마커 오버라이드만 다시 구워 적용한다. 비용이 싸다. */
	UFUNCTION(CallInEditor, Category = "Grid", meta = (DisplayName = "Apply Marker Overrides"))
	void ApplyOverrides();

	UFUNCTION(CallInEditor, Category = "Grid", meta = (DisplayName = "Clear Grid"))
	void ClearGrid();

	/** 셀 하나의 생성 상태와 테스트 경로를 로그로 남긴다. 계단과 경사를 튜닝할 때 쓴다. */
	UFUNCTION(CallInEditor, Category = "Grid|Debug", meta = (DisplayName = "Log Debug Report"))
	void LogDebugReport();

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	FIntPoint DebugInspectCell = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	FIntPoint DebugPathStart = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, Category = "Grid|Debug")
	FIntPoint DebugPathGoal = FIntPoint(79, 79);

	// ---------------------------------------------------------------- 질의

	UFUNCTION(BlueprintPure, Category = "Grid")
	bool IsValidCell(FIntPoint Cell) const
	{
		return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < SizeInCells.X && Cell.Y < SizeInCells.Y;
	}

	int32 CellToIndex(FIntPoint Cell) const { return Cell.Y * SizeInCells.X + Cell.X; }
	FIntPoint IndexToCell(int32 Index) const { return FIntPoint(Index % SizeInCells.X, Index / SizeInCells.X); }

	const FGridCellData* GetCell(FIntPoint Cell) const
	{
		const int32 Index = CellToIndex(Cell);
		return (IsValidCell(Cell) && Cells.IsValidIndex(Index)) ? &Cells[Index] : nullptr;
	}

	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector GetGridOrigin() const { return GetActorLocation(); }

	/** 월드 위치가 속한 셀. 범위를 벗어날 수 있다 -- IsValidCell로 검사할 것. */
	UFUNCTION(BlueprintPure, Category = "Grid")
	FIntPoint WorldToCell(const FVector& World) const;

	/** 바닥 높이에서의 셀 중심. 셀에 바닥이 없으면 그리드 원점 Z를 쓴다. */
	UFUNCTION(BlueprintPure, Category = "Grid")
	FVector CellToWorld(FIntPoint Cell) const;

	/** 런타임 규칙을 무시한 정적 걸을 수 있음 여부: Walkable, StageClear, Conditional. */
	UFUNCTION(BlueprintPure, Category = "Grid")
	bool IsCellWalkableStatic(FIntPoint Cell) const;

	// ---------------------------------------------------------------- 런타임 점유
	//
	// 움직이는 액터가 지금 어느 셀 위에 서 있는지. Cells와 분리한 이유는, 그 배열은 에디터에서
	// 구워 맵에 직렬화되는 반면 이것은 퍼즐 블록이 슬라이드할 때마다 바뀌기 때문이다.
	// CanPawnEnter를 거치게 하면 길찾기, 폰의 매 걸음 재검사, 호버 오버레이가 모두 추가 작업
	// 없이 블록을 인식하게 된다.
	//
	// 의도적으로 AActor 타입으로 둔다: 그리드는 퍼즐 클래스를 모르는 채로 남아 의존성이
	// 한 방향으로만 흐르고, 앞으로 추가될 어떤 움직이는 액터도 같은 방식으로 등록할 수 있다.
	//
	// LTTSGrid::PassThroughOccupantsTag()가 붙은 폰에게는 이 레이어가 보이지 않는다. 행인
	// NPC는 셀을 잡지도, 남이 잡은 셀에 막히지도 않으면서 같은 길찾기를 쓴다.

	/** 셀을 점유한다. 다른 액터가 이미 잡고 있으면 실패한다. */
	bool SetOccupant(FIntPoint Cell, AActor* Occupant);

	/** 셀을 해제한다. 단, Expected가 아직 잡고 있을 때만. */
	void ClearOccupant(FIntPoint Cell, const AActor* Expected);

	/** 액터가 잡고 있는 모든 셀을 해제한다. 액터가 이동하거나 레벨을 떠날 때 쓴다. */
	void ClearAllOccupantsOf(const AActor* Occupant);

	AActor* GetOccupant(FIntPoint Cell) const;

	bool IsCellOccupied(FIntPoint Cell, const AActor* Ignore = nullptr) const;

	int32 GetNumOccupiedCells() const { return Occupants.Num(); }

	/**
	 * 정적 걸을 수 있음 여부에 점유와 Conditional 규칙(있다면)을 더한 판정.
	 *
	 * 폰에 PassThroughOccupantsTag가 붙어 있으면 점유는 건너뛴다.
	 */
	bool CanPawnEnter(FIntPoint Cell, const APawn* Pawn, FText* OutDeniedMessage = nullptr) const;

	/** 셀에 들어갈 수 없는 이유를 사람이 읽을 수 있는 문장으로 돌려준다. */
	FText DescribeCell(FIntPoint Cell, const APawn* Pawn) const;

	/** 링을 넓혀 가며 걸을 수 있는 셀을 찾는다. 스폰 시 폰을 배치할 때 쓴다. */
	bool FindNearestWalkableCell(FIntPoint From, int32 MaxRadius, const APawn* Pawn, FIntPoint& OutCell) const;

	/** 그리드 위 A*. OutPath는 Start를 제외하고 Goal로 끝난다. */
	bool FindPath(FIntPoint Start, FIntPoint Goal, const APawn* Pawn, TArray<FIntPoint>& OutPath) const;

	/** 폰이 도착하면 호출한다. StageClear 셀이면 OnStageClear를 발생시킨다. */
	void NotifyPawnEnteredCell(APawn* Pawn, FIntPoint Cell);

	/** 월드의 첫 번째 그리드 액터, 없으면 null. */
	static AGridActor* FindGrid(const UWorld* World);

	// ---------------------------------------------------------------- 라이프사이클

	virtual void PostInitializeComponents() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;

	/** 마커가 이동·변경·삭제될 때 마커 쪽에서 호출한다. */
	void OnMarkerChanged();
#endif

private:
	/** 트레이스만으로 Cells를 채운다. 마커에 대해서는 전혀 모른다. */
	void GenerateFromTraces();

	/** 두 번째 패스: 바닥 높이 차이가 MaxStepHeight 이내인 4방향 이웃을 연결한다. */
	void BuildAdjacency();

	/** 직렬화된 Overrides를 배열 순서대로 Cells에 찍는다(뒤쪽이 이긴다). */
	void ApplyStoredOverrides();

	void RecomputeStats();
	void RefreshDebugDraw();

#if WITH_EDITOR
	/** 마커를 모아 Overrides + ConditionalRules로 만든다. 순서는 결정적이다. */
	void BakeOverridesFromMarkers();

	/** 이미 다시 굽는 중에 마커가 재진입하는 것을 막는다. */
	bool bIsApplyingOverrides = false;
#endif

	/**
	 * 셀 인덱스 -> 그 위에 서 있는 액터. 런타임 전용이며 절대 직렬화하지 않는다: 레벨이
	 * 시작될 때마다 각 블록이 스스로 등록하면서 처음부터 다시 만들어진다.
	 */
	TMap<int32, TWeakObjectPtr<AActor>> Occupants;

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UGridDebugDrawComponent> DebugDrawComponent;
};
