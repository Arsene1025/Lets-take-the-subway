// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridEscalator.generated.h"

class AGridActor;
class AGridPawn;
class UBoxComponent;
class USceneComponent;
class USplineComponent;

/**
 * 한쪽 방향으로만 태워 나르는 에스컬레이터.
 *
 * 걷는 경사로가 아니라 **탈것**이다. 시작 쪽 셀에 서서 클릭하면 폰이 첫 발판 위로 걸어
 * 들어가고, 그 뒤로는 발판이 폰을 끝까지 실어 나른 다음 반대쪽에 내려놓는다. 조작도 코드
 * 경로도 엘리베이터와 같다: BoardVehicle로 태우고, 실려 가는 동안 폰이 스스로 입력을 잠그고,
 * WalkOntoGrid로 내린다.
 *
 * **그리드에 굽지 않는다.** 셀을 점유하지도, 이웃 연결을 끊지도 않는다. 대신 경사 구간의
 * 바닥을 아예 없애는 것이 층을 가르는 방법이다(그레이박스 계단에 GridTraceIgnore를 붙이면
 * 그 셀들은 NoFloor가 된다). 그러면 걸어서 층을 오갈 방법이 없어지고, 지나갈 유일한 수단이
 * 탑승이 된다 -- 규칙을 그리드 데이터에 새기지 않고도 일방통행이 성립한다.
 *
 * 태워 나르는 길은 셀이 아니라 **경로**로 적는다(RidePathLocal). 아트 발판은 상승 6.1 m,
 * 진행 6.5 m라 셀 격자에 딱 떨어지지 않고 층 간격과도 어긋나므로, 셀에서 좌석 높이를 얻으려
 * 하면 바닥이 구워져 있어야만 동작한다. 경로는 액터 로컬이라 회전·이동을 따라오고, 어느
 * 끝에서 태우는지도 순서로 정해진다: 첫 점이 타는 쪽, 마지막 점이 내리는 쪽이다.
 * BP_escalator_up_s와 down_s의 차이는 이 순서뿐이다.
 *
 * **컴포넌트를 하나도 만들지 않는다.** 이 클래스는 아트 블루프린트의 부모가 되라고 만든
 * 것인데, 네이티브 루트 컴포넌트가 있으면 리페어런트할 때 블루프린트의 DefaultSceneRoot와
 * 그 아래 메시가 전부 사라진다(2026-09-08에 실제로 겪었다). 그래서 필요한 컴포넌트는 전부
 * BeginPlay에서 만든다. 대신 C++ 클래스를 레벨에 그냥 놓을 수는 없다 -- 트랜스폼을 줄 루트가
 * 없기 때문이다.
 */
UCLASS(HideCategories = (Physics, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication),
	meta = (DisplayName = "Grid Escalator"))
class LETSTAKETHESUBWAY_API AGridEscalator : public AActor
{
	GENERATED_BODY()

public:
	AGridEscalator();

	// ---------------------------------------------------------------- 저작

	/**
	 * 발판 표면을 따라가는 탑승 경로. 액터 로컬 좌표이며 **[0]이 타는 쪽, 마지막이 내리는 쪽**이다.
	 *
	 * 방향을 따로 적지 않는 이유가 여기 있다: 올라가는 것과 내려가는 것의 차이는 이 배열의
	 * 순서뿐이고, 액터를 회전시키면 경로도 함께 돈다. 점은 두 개면 충분하지만(직선), 발판이
	 * 시작과 끝에서 평평해지는 구간까지 살리려면 네 개를 찍는다.
	 *
	 * 각 점의 Z는 **공이 놓일 발판 윗면**이다. 폰은 여기에 자기 HeightAboveFloor를 더한 높이에
	 * 실린다. 레벨에서 아래로 트레이스해 잰 값은 눈에 보이는 발판보다 낮게 나오므로(맞는 것은
	 * 난간 밑판이다) 실측값에 70 cm를 더해 넣었다.
	 */
	UPROPERTY(EditAnywhere, Category = "Escalator", meta = (MakeEditWidget = true))
	TArray<FVector> RidePathLocal;

	/** 경로를 따라 실어 나르는 속도 (cm/s). 경사를 따라 잰 거리 기준이다. */
	UPROPERTY(EditAnywhere, Category = "Escalator", meta = (ClampMin = 1.0))
	float RideSpeed = 75.0f;

	/**
	 * PlayRate 1일 때 발판이 초당 나아가는 거리 (cm/s). 실측값이다.
	 *
	 * 0이 아니면 BeginPlay에서 이 액터의 모든 스켈레탈 메시 PlayRate를 RideSpeed에 맞춰,
	 * 발판과 그 위에 실린 공이 서로 미끄러지지 않게 한다. 재생 방향(부호)은 그대로 둔다 --
	 * 내려가는 에스컬레이터는 아트에서 역재생으로 만들어져 있다.
	 *
	 * 0이면 애니메이션에 손대지 않는다. 아직 재 보지 않았다는 뜻이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Escalator", meta = (ClampMin = 0.0))
	float AnimStepSpeedAtRate1 = 0.0f;

	/**
	 * 경로 시작점에서 진행 반대 방향으로 이만큼 떨어진 자리가 타는 셀이다 (cm).
	 *
	 * 폰이 서는 곳은 발판 위가 아니라 발판 앞 바닥이므로, 경로 시작점 그대로를 셀로 바꾸면
	 * 타야 할 자리가 아니라 첫 발판이 있는 셀이 나온다.
	 */
	UPROPERTY(EditAnywhere, Category = "Escalator", meta = (ClampMin = 0.0))
	float ApproachDistance = 100.0f;

	/** 경로 끝점에서 진행 방향으로 이만큼 앞이 내려놓을 자리다 (cm). WalkOntoGrid가 여기서 셀을 찾는다. */
	UPROPERTY(EditAnywhere, Category = "Escalator", meta = (ClampMin = 0.0))
	float LandingDistance = 100.0f;

	/**
	 * 타는 셀을 진행 방향의 좌우로 몇 칸까지 인정할지.
	 *
	 * 아트 발판의 폭이 3.8 m라 정면 한 칸만 인정하면 옆에 서서 누른 플레이어가 이유 없이
	 * 거부당한다. 1이면 세 칸(가운데와 좌우 한 칸씩)이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Escalator", meta = (ClampMin = 0, ClampMax = 8))
	int32 BoardingHalfWidthCells = 1;

	/**
	 * 클릭을 받을 상자의 액터 로컬 중심과 반크기 (cm).
	 *
	 * 아트 발판(AC_001)에는 콜리전이 없고 난간에만 있어서, 발판 한가운데를 눌러도 아무것도
	 * 맞지 않고 클릭이 뒤쪽 바닥으로 빠진다. 그래서 발판을 덮는 보이지 않는 상자를 BeginPlay에
	 * 만들어 커서 트레이스만 받는다. 반크기가 0이면 상자를 만들지 않고 아트 콜리전에 맡긴다.
	 */
	UPROPERTY(EditAnywhere, Category = "Escalator")
	FVector ClickBoxCentreLocal = FVector(0.0, 0.0, 300.0);

	UPROPERTY(EditAnywhere, Category = "Escalator")
	FVector ClickBoxExtent = FVector(340.0, 190.0, 320.0);

	/** 경로·타는 셀·내리는 자리를 화면에 그린다. 배치를 맞출 때만 켠다. */
	UPROPERTY(EditAnywhere, Category = "Escalator|Debug")
	bool bDrawDebugPath = false;

	// ---------------------------------------------------------------- 조회

	/** 폰이 지금 탈 수 있는지. 타는 쪽 셀에 서 있어야 한다. */
	bool CanBoard(const AGridPawn* Pawn, FText* OutReason = nullptr) const;

	/** 폰을 첫 발판으로 걸어 들어오게 한다. 실려 가는 것은 그다음부터다. */
	bool TryBoard(AGridPawn* Pawn, FText* OutReason = nullptr);

	/** 지금 누군가를 태우고 있는지. */
	bool HasRider() const { return Rider.IsValid(); }

	/** 타는 셀과 내리는 자리를 로그로 남긴다. 뷰포트에 표시가 없으므로 이걸로 확인한다. */
	UFUNCTION(CallInEditor, Category = "Escalator", meta = (DisplayName = "Log Boarding Cells"))
	void LogBoardingCells();

	// ---------------------------------------------------------------- 생명주기

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 탑승 진행. 엘리베이터의 ETravelPhase와 같은 골격이다. */
	enum class ERidePhase : uint8
	{
		/** 아무도 타고 있지 않다. */
		Idle,

		/** 폰이 첫 발판까지 걸어 들어오는 중. 다 들어올 때까지 발판을 움직이지 않는다. */
		WaitingForRider,

		/** 좌석을 경로를 따라 옮기고 폰이 따라온다. */
		Riding,

		/** 폰이 그리드로 걸어 나가는 중. 다 나가면 조작이 풀린다. */
		Unloading
	};

	/** BeginPlay에서 스플라인·좌석·클릭 상자를 만든다. 하나라도 못 만들면 false. */
	bool BuildRuntimeComponents();

	/** 경로의 i번째 점을 월드로. */
	FVector PathPointWorld(int32 Index) const;

	/** 경로 시작·끝의 수평 진행 방향(월드, 정규화). */
	FVector StartDirectionWorld() const;
	FVector EndDirectionWorld() const;

	/** 폰이 서서 타야 하는 자리와, 내려놓을 자리. */
	FVector BoardingAnchorWorld() const;
	FVector LandingAnchorWorld() const;

	/** 경로 위 거리에서의 좌석 위치. 발판 표면 위로 폰의 HeightAboveFloor만큼 띄운다. */
	FVector SeatWorldAtDistance(double Distance, const AGridPawn& Pawn) const;

	/** 타는 셀 목록을 다시 계산한다. 에디터에서도 부를 수 있게 그리드를 인자로 받는다. */
	void ComputeBoardingCells(const AGridActor& InGrid, TArray<FIntPoint>& OutCells) const;

	/** 스켈레탈 메시 재생 속도를 RideSpeed에 맞춘다. */
	void ApplyAnimationPlayRate();

	/** 태우기를 끝내고 평소 상태로 돌아간다. */
	void FinishRide();

	void DrawDebugOverlay() const;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> RidePath;

	/** 폰이 따라다니는 점. 액터는 가만히 있고 이것만 발판을 따라 움직인다. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Seat;

	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> ClickVolume;

	TWeakObjectPtr<AGridPawn> Rider;

	/** 폰이 여기 서 있을 때만 탈 수 있다. */
	TArray<FIntPoint> BoardingCells;

	ERidePhase Phase = ERidePhase::Idle;

	/** 이번 탑승이 경로를 따라 지금까지 온 거리 (cm). */
	double RideDistance = 0.0;
};
