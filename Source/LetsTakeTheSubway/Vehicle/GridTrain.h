// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridTrain.generated.h"

class AGridActor;
class AGridNPC;
class AGridNPCSpawner;
class AGridPawn;
class UStaticMeshComponent;

/**
 * 열차가 서는 곳 하나.
 *
 * 위치를 월드 좌표가 아니라 셀로 적는 이유는 이 프로젝트의 다른 모든 저작과 같다: 셀 번호는
 * 그리드 디버그 오버레이에서 그대로 읽을 수 있지만 월드 좌표는 계산해야 한다. 높이는 열차를
 * 배치한 Z를 그대로 쓰므로 적지 않는다.
 */
USTRUCT(BlueprintType)
struct FGridTrainStop
{
	GENERATED_BODY()

	/** 로그와 피드백에 쓰는 이름. */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	FName StopName;

	/** 열차 중심이 서는 셀. XY만 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	FIntPoint StopCell = FIntPoint::ZeroValue;

	/**
	 * 문 앞 승강장 셀. 폰이 여기 서 있을 때만 열차를 클릭해 탈 수 있다.
	 *
	 * 이 목록은 탑승 조건이면서 동시에 커서 규칙이기도 하다. 폰이 여기 없으면 열차는 커서에
	 * 잡히지 않고 뒤쪽 바닥이 클릭된다. 그러지 않으면 4 m짜리 차체가 승강장 셀을 가려
	 * 영영 클릭할 수 없게 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	TArray<FIntPoint> BoardingCells;

	/**
	 * 이 역에서 내릴 때 폰이 걸어 나갈 셀. 유효하지 않으면 문 앞 셀 중 하나를 쓴다.
	 *
	 * 정확한 셀을 못 찾더라도 그리드가 근처에서 걸을 수 있는 셀을 찾아 준다
	 * (AGridActor::FindEntryCell).
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	FIntPoint ExitCell = FIntPoint(-1, -1);

	/**
	 * 내린 뒤 이어서 걸어갈 셀. 유효하지 않으면 내린 자리에 선다.
	 *
	 * 기획의 "일정 위치까지 이동한 뒤 정지"다. 문 앞에서 바로 조작이 돌아오면 플레이어가
	 * 자기가 어디에 내렸는지 알아차리기 전에 열차가 떠난다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	FIntPoint PostExitCell = FIntPoint(-1, -1);

	/**
	 * 이 역에서 문이 열릴 때 행인을 내보낼 스포너.
	 *
	 * 스포너를 열차가 아니라 승강장에 두는 이유는 경유 셀이 역마다 다르기 때문이다. 열차에
	 * 붙이면 정차역마다 목록을 갈아 끼워야 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	TObjectPtr<AGridNPCSpawner> DisembarkSpawner;

	UPROPERTY(EditAnywhere, Category = "Train Stop", meta = (ClampMin = 0, ClampMax = 32))
	int32 DisembarkCount = 3;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrainDoorsOpened, AGridTrain*, Train, FName, StopName);

/**
 * 승강장 사이를 오가는 열차.
 *
 * 퍼즐 조각이 아니라서 Puzzle이 아닌 Vehicle에 산다. 그리드 셀을 점유하지도 않는다 --
 * 선로는 애초에 걸을 수 없는 바닥이라 막을 것이 없다.
 *
 * 기획의 두 역할을 함께 한다. 하나는 연출이다: 플레이어가 퍼즐을 푸는 동안에도 열차가
 * 주기적으로 들어와 문을 열고 행인을 쏟아낸 뒤 떠난다. 다른 하나는 이동 수단이다: 문이
 * 열려 있을 때 문 앞에 선 플레이어가 클릭하면 타고, 다음 역에서 자동으로 내린다.
 *
 * 선로는 스플라인이 아니라 정차역 셀들의 직선 구간이다. 지하철 승강장 구간은 곧고, 셀
 * 번호는 디버그 오버레이에서 그대로 읽을 수 있다. 곡선이 필요해지면 그때 경로 액터를 둔다.
 */
UCLASS(HideCategories = (Physics, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API AGridTrain : public AActor
{
	GENERATED_BODY()

public:
	AGridTrain();

	// ---------------------------------------------------------------- 저작

	/** 순서대로 도는 정차역. 최소 둘은 있어야 열차가 움직인다. */
	UPROPERTY(EditAnywhere, Category = "Train")
	TArray<FGridTrainStop> Stops;

	/** 마지막 역 다음에 첫 역으로 돌아간다. 끄면 마지막 역에서 멈춘 채로 있는다. */
	UPROPERTY(EditAnywhere, Category = "Train")
	bool bLoop = true;

	UPROPERTY(EditAnywhere, Category = "Train", meta = (ClampMin = 1.0))
	float Speed = 1200.0f;

	/** 문이 열려 있는 시간(초). 기획의 5초가 기본값이다. */
	UPROPERTY(EditAnywhere, Category = "Train", meta = (ClampMin = 0.5))
	float DoorOpenSeconds = 5.0f;

	/** 문이 닫히는 연출에 쓰는 시간(초). 이 동안에는 탈 수 없다. */
	UPROPERTY(EditAnywhere, Category = "Train", meta = (ClampMin = 0.0))
	float DoorCloseSeconds = 1.0f;

	/** 플레이어가 타면 남은 대기 시간을 버리고 곧바로 문을 닫는다. */
	UPROPERTY(EditAnywhere, Category = "Train")
	bool bDepartAfterBoarding = true;

	/** 첫 역에 도착하기까지의 대기(초). 여러 열차의 시각을 어긋나게 할 때 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Train", meta = (ClampMin = 0.0))
	float StartDelaySeconds = 2.0f;

	/** 차체 길이(cm), 진행 축 방향. */
	UPROPERTY(EditAnywhere, Category = "Train|Body", meta = (ClampMin = 100.0))
	float BodyLength = 1600.0f;

	/** 차체 폭(cm). */
	UPROPERTY(EditAnywhere, Category = "Train|Body", meta = (ClampMin = 100.0))
	float BodyWidth = 400.0f;

	/** 차체 높이(cm). 액터 원점이 객차 바닥이고 몸체는 그 위로 올라간다. */
	UPROPERTY(EditAnywhere, Category = "Train|Body", meta = (ClampMin = 100.0))
	float BodyHeight = 400.0f;

	/** 문이 열렸을 때 발생한다. 하차 연출을 여기에 매달 수 있다. */
	UPROPERTY(BlueprintAssignable, Category = "Train")
	FOnTrainDoorsOpened OnDoorsOpened;

	// ---------------------------------------------------------------- 조회

	bool AreDoorsOpen() const { return Phase == ETrainPhase::DoorsOpen; }

	/** 지금 서 있는(또는 향해 가는) 정차역의 인덱스. */
	int32 GetCurrentStopIndex() const { return CurrentStop; }

	/** 폰이 지금 이 열차에 탈 수 있으면 true. 아니면 이유를 알려준다. */
	bool CanBoard(const AGridPawn* Pawn, FText* OutReason = nullptr) const;

	// ---------------------------------------------------------------- 사용

	/** 폰을 태우거나, 왜 못 타는지 설명한다. */
	bool TryBoard(AGridPawn* Pawn, FText* OutReason = nullptr);

	/** 지금 즉시 다음 역에 도착시킨다(콘솔 시험용). */
	void ForceArriveAtNextStop();

	/** 문을 강제로 열거나 닫는다(콘솔 시험용). */
	void ForceDoors(bool bOpen);

	// ---------------------------------------------------------------- 생명주기

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	/** 열차가 무엇을 하고 있는지. */
	enum class ETrainPhase : uint8
	{
		/** 시작 대기. 첫 역에 아직 들어오지 않았다. */
		Idle,

		/** 다음 역으로 이동 중. */
		Moving,

		/** 정차해 문이 열려 있다. */
		DoorsOpen,

		/** 문이 닫히는 중. 탈 수 없다. */
		DoorsClosing
	};

	void EnterMoving();
	void EnterDoorsOpen();
	void EnterDoorsClosing();

	/** 정차역의 월드 위치. XY는 셀에서, Z는 배치된 높이에서 온다. */
	FVector GetStopLocation(int32 StopIndex) const;

	int32 GetNextStopIndex() const;

	void RefreshVisual();

	/** 문 슬랩 색으로 열림·닫힘을 보여 준다. */
	void RefreshDoorLook();

	UPROPERTY(VisibleAnywhere, Category = "Train")
	TObjectPtr<USceneComponent> SceneRoot;

	/**
	 * 차체. Visibility를 막아 커서에 잡힌다.
	 *
	 * 폰과는 부딪히지 않는다. 폰에는 콜리전이 전혀 없고 위치는 전적으로 그리드가 정한다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Train")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 승강장 쪽 면에 붙는 문 표시. 열림·닫힘에 따라 머티리얼이 바뀐다. */
	UPROPERTY(VisibleAnywhere, Category = "Train")
	TArray<TObjectPtr<UStaticMeshComponent>> DoorMeshes;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DoorClosedMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DoorOpenMaterial;

	TWeakObjectPtr<AGridPawn> Rider;

	/**
	 * 방금 내린 폰. 그리드에 다시 설 때까지만 들고 있는다.
	 *
	 * 하차는 두 걸음이다: 차체에서 승강장으로 걸어 나오고, 그다음 지정된 자리까지 걸어간다.
	 * 두 번째 걸음은 폰이 셀 위에 다시 서기 전에는 시킬 수 없다.
	 */
	TWeakObjectPtr<AGridPawn> UnloadedPawn;

	ETrainPhase Phase = ETrainPhase::Idle;

	/** 향해 가는(또는 서 있는) 정차역. */
	int32 CurrentStop = 0;

	/** 열차가 서는 높이. BeginPlay에서 배치된 Z를 기록한다. */
	double TrackZ = 0.0;

	float PhaseTimer = 0.0f;

	bool bDoorsLookOpen = false;
};
