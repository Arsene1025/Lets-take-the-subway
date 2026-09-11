// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridFootprint.h"
#include "Grid/GridTypes.h"
#include "Puzzle/PuzzleTypes.h"
#include "PuzzleBlock.generated.h"

class AGridActor;
class APuzzleRegion;
class UChildActorComponent;
class UPuzzleSubsystem;
class UMaterialInterface;
class UStaticMesh;
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
		Rotating,

		/**
		 * 엘리베이터가 층 사이를 오가는 중.
		 *
		 * 슬라이드·회전과 같은 목록에 두는 이유는 IsAnimating() 하나로 입력 잠금이 결정되기
		 * 때문이다. 승강 중에 블록을 밀 수 있으면 퍼즐이 절반쯤 다른 층에 놓인다.
		 */
		Lifting
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

	/**
	 * 이 조각을 밀 수 있는지. 끄면 어느 방향으로도 움직이지 않는다.
	 *
	 * MoveAxis와 나누어 두는 이유는 두 값이 다른 질문에 답하기 때문이다. MoveAxis는 "어느
	 * 축으로 움직이는가"라는 퍼즐 설계이고, 이 값은 "애초에 움직이는 물건인가"라는 배치
	 * 결정이다. 같은 벤치 블루프린트를 놓고 인스턴스마다 이 체크만 끄면 붙박이 벤치가 된다.
	 *
	 * **끈 조각도 셀은 그대로 점유한다.** 고정 벤치는 폰과 다른 조각의 길을 막는 지형이다.
	 * 영구 지형이라 아예 클릭도 되지 않아야 한다면 블록이 아니라 Blocked 마커를 쓴다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block")
	bool bCanMove = true;

	/**
	 * 이 조각이 회전할 수 있는지. 회전판 위에서도, 회전 기둥에 붙어서도 돌지 않는다.
	 *
	 * 회전 장애물과 기둥에게는 "레버를 돌려도 이 구조물은 돌지 않는다"는 뜻이고, 일반
	 * 블록에게는 "회전판이 나를 돌릴 수 없다"는 뜻이다. 후자의 경우 회전 자체가 거부된다 --
	 * 돌지 않는 조각만 남겨 두고 나머지를 돌리면 조각들이 서로 겹친다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block")
	bool bCanRotate = true;

	/**
	 * 풋프린트 안에서 **점유하지 않고 비워 둘** 사각 영역의 낮은 쪽 모서리.
	 * 블록 자신의 프레임 기준이며, 블록이 돌면 함께 돈다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 0))
	FIntPoint HollowOffset = FIntPoint::ZeroValue;

	/**
	 * 비워 둘 영역의 크기(셀). 어느 한 변이라도 0이면 꽉 찬 사각형이다(기본값).
	 *
	 * L자 벤치처럼 사각형에서 한 귀퉁이가 빠진 조각을 위한 것이다. 빠진 자리에는 다른
	 * 조각이나 폰이 들어설 수 있다 -- 회전 기둥을 감싸고 도는 벤치가 바로 이 모양이다.
	 *
	 * **프록시 큐브는 여전히 풋프린트 전체를 덮는다.** 커서 판정이 그만큼 넉넉해지는데,
	 * 빈 자리에 들어앉는 조각이 대개 더 높아서 실루엣 싸움에서 이긴다. 정확한 클릭 판정이
	 * 필요해지면 몸체를 슬랩 여러 장으로 나누는 편이 낫다(회전 장애물이 그렇게 한다).
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 0))
	FIntPoint HollowSize = FIntPoint::ZeroValue;

	/** 비주얼 높이(cm). 퍼즐에는 영향이 없다 -- 그리드는 2차원이다. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 10.0))
	float Height = 150.0f;

	/** 슬라이드 속도(cm/s). 600이면 1 m 셀 하나에 약 1/6초가 걸린다. */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block", meta = (ClampMin = 1.0))
	float SlideSpeed = 600.0f;

	/**
	 * 한 셀 이동 후 멈추고, 플레이어가 놓았다가 다시 잡을 때까지 기다린다.
	 *
	 * 기본값은 꺼짐(2026-09-11): 드래그는 방향을 가리키는 조이스틱처럼 동작해서, 커서를 잡은
	 * 지점에서 반 칸 이상 밀어 둔 채로 있으면 막힐 때까지 **일정한 속도로 계속** 밀린다.
	 * 켜면 예전의 "드래그 한 번에 한 칸"으로 돌아간다 -- 조각 하나하나를 셈해서 푸는 퍼즐을
	 * 다시 만들고 싶을 때를 위해 남겨 둔다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block")
	bool bOneStepPerDrag = false;

	/**
	 * 그레이박스 큐브 대신 보여 줄 아트 액터. 비워 두면 지금까지처럼 큐브를 그린다.
	 *
	 * 아트 에셋을 이 클래스의 부모로 삼지 않고 자식 액터로 품는 이유는 소유권이다. 아트
	 * 블루프린트는 아트가 계속 고치는 물건이라, 리페어런팅으로 게임플레이 클래스에 묶으면
	 * 양쪽이 같은 에셋을 두고 부딪힌다. 자식으로 두면 아트는 자기 블루프린트만 고치면 되고
	 * 퍼즐 코드는 그것을 모른 채로 남는다.
	 *
	 * 아트는 **보여 주기만 한다.** 커서 판정과 그리드 트레이스는 그대로 그레이박스 프록시가
	 * 맡는다(SanitiseVisualActor에서 자식의 콜리전을 전부 끈다). 그래야 조각을 잡는 규칙이
	 * 아트 메시의 모양에 좌우되지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block|Art")
	TSubclassOf<AActor> VisualActorClass;

	/**
	 * 그레이박스 큐브 대신 보여 줄 스태틱 메시. 비워 두면 큐브를 그린다.
	 *
	 * VisualActorClass와 같은 일을 더 싸게 한다. 아트가 이미 블루프린트로 만들어 둔 물건
	 * (엘리베이터처럼 자기 문과 이벤트 그래프를 가진 것)은 자식 액터로 품어야 하지만,
	 * 벤치나 자판기처럼 **메시 하나뿐인 장식**을 위해 액터 블루프린트를 따로 만들 이유는
	 * 없다. 이 슬롯을 채우면 코드가 만든 컴포넌트에 그 메시가 들어간다.
	 *
	 * 둘 다 채우면 둘 다 그려진다. 보통은 하나만 쓴다.
	 *
	 * 규칙은 자식 액터 쪽과 똑같다: **아트는 보여 주기만 한다.** 콜리전은 꺼지고 그리드
	 * 생성도 이 컴포넌트를 지나친다. 커서가 잡는 것과 그리드가 보는 것은 언제나 풋프린트와
	 * 정확히 같은 프록시 큐브다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block|Art")
	TObjectPtr<UStaticMesh> ArtMesh;

	/**
	 * 아트 메시를 블록 원점(풋프린트 중심의 바닥)에 맞추는 보정.
	 *
	 * 아트 메시의 피벗이 바닥 중앙이 아니거나 축이 다른 쪽을 보고 있을 때 쓴다. 아트 에셋을
	 * 고치지 않고 게임플레이 쪽에서 맞추기 위한 자리다.
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block|Art")
	FTransform ArtMeshOffset = FTransform::Identity;

	/**
	 * 아트 메시의 슬롯이 비어 있을 때(엔진 기본 회색 격자) 대신 씌울 머티리얼.
	 *
	 * 임포트된 메시는 슬롯이 비면 WorldGridMaterial로 그려진다. 원본 스태틱 메시 액터는
	 * 컴포넌트 오버라이드로 프로젝트 툰 머티리얼을 덮어 쓰고 있었는데, 메시만 이 컴포넌트로
	 * 옮기면 그 오버라이드는 따라오지 않는다. 이미 칠해진 슬롯은 건드리지 않는다.
	 *
	 * C++ 기본값은 비어 있다. 어느 머티리얼을 쓸지는 아트의 결정이므로 블루프린트
	 * 기본값에서 지정한다(README: Core에서 Art로 하드 참조하지 않는다).
	 */
	UPROPERTY(EditAnywhere, Category = "Puzzle Block|Art")
	TObjectPtr<UMaterialInterface> ArtFallbackMaterial;

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

	/** 비워 둘 영역이 지정돼 있으면 true. */
	bool HasHollow() const { return HollowSize.X > 0 && HollowSize.Y > 0; }

	/** 지금 그리드에 놓인 상태의 빈 영역. 비워 둔 것이 없으면 크기 0 사각형이다. */
	FGridRect GetWorldHollowRect() const;

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

	/** 지금 잡힌 채로 지나온 걸음 수. 디버그 오버레이가 읽는다. */
	int32 GetStepsWhileHeld() const { return StepsWhileHeld; }

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

	/**
	 * 이 블록이 벗어날 수 없는 퍼즐 구간. 구간이 없는 레벨에서는 null이며 제한도 없다.
	 *
	 * 소속은 BeginPlay에서 한 번 정해지고 이후 바뀌지 않는다. 블록이 구간 사이를 옮겨
	 * 다닐 수 있다면 "이 퍼즐의 조각"이라는 말 자체가 성립하지 않는다.
	 */
	APuzzleRegion* GetHomeRegion() const { return HomeRegion.Get(); }

	// ---------------------------------------------------------------- 이동

	/** 지금 당장 그 방향으로 한 걸음 갈 수 있으면 true. */
	bool CanSlide(EGridDirection Dir, FText* OutReason = nullptr) const;

	/**
	 * 조각 자신의 사정으로 지금 움직일 수 없으면 false.
	 *
	 * 목적지와 무관한 이유를 위한 자리다. 엘리베이터는 누가 타고 있으면 거부한다.
	 */
	virtual bool CanStartMoving(FText* OutReason = nullptr) const { return true; }

	/**
	 * 목적지 사각형이 이 조각에게 맞는지.
	 *
	 * 기본 블록에게는 바닥이 있으면 그만이지만, 엘리베이터는 지금 서 있는 층과 같은 높이의
	 * 바닥으로만 나갈 수 있다. 위층으로 올라간 차체가 샤프트로 되돌아가면 허공에 뜬다.
	 */
	virtual bool CanOccupyRect(const FGridRect& Rect, FText* OutReason = nullptr) const { return true; }

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
	 * 쥔 채로 계속 밀 방향. 비어 있으면 지금 향하던 셀에 도착한 뒤 멈춘다.
	 *
	 * 컨트롤러가 매 틱 갱신하고, **이어 붙이는 일은 블록이 자기 Tick에서** 한다. 도착한
	 * 프레임에 컨트롤러가 다시 밀어 주기를 기다리면 칸마다 한 프레임씩 쉬게 되어, 요청된
	 * "일정한 속도"가 칸 경계마다 끊긴다. 남은 이동 거리를 다음 칸으로 이월하는 것도
	 * 그래서 블록 쪽에 있다.
	 */
	void SetHeldSlideDirection(TOptional<EGridDirection> Dir);

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
	virtual void PostRegisterAllComponents() override;
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

	/** 아트(자식 액터든 스태틱 메시든)가 붙어 있으면 true. 파생 클래스가 자기 그레이박스 장식을 숨길 때 쓴다. */
	bool IsUsingArtVisual() const { return VisualActorClass != nullptr || ArtMesh != nullptr; }

	/**
	 * 자식 아트 액터를 순수한 장식으로 만든다.
	 *
	 * 콜리전을 전부 끄고 그리드 생성 무시 태그를 붙인다. 그러지 않으면 아트 메시가 커서
	 * 트레이스를 가로채 잡는 규칙이 메시 모양에 좌우되고, 그리드 생성이 아트를 바닥으로
	 * 구워 조각이 놓인 자리를 지형으로 굳혀 버린다.
	 */
	void SanitiseVisualActor();

	/**
	 * 자기를 담고 있는 퍼즐 구간을 찾아 기억한다. 첫 틱에 한 번만 돈다.
	 *
	 * BeginPlay가 아니라 첫 틱인 이유는 등록 순서 때문이다. 구간도 액터라 자기
	 * BeginPlay에서 등록하는데, 액터의 BeginPlay 순서는 정해져 있지 않다. 블록이 먼저
	 * 돌면 아직 아무 구간도 등록돼 있지 않아 "구간 없음"으로 굳어 버린다. 첫 틱은 모든
	 * BeginPlay가 끝난 뒤이므로 배치 순서에 좌우되지 않는다.
	 */
	void ResolveHomeRegion();

	/**
	 * 저작된 풋프린트 안의 셀이, 지금까지 돈 90도 회전 횟수를 반영해 그리드 위에 놓인
	 * 풋프린트 안에서 어디에 해당하는지 구한다.
	 */
	FIntPoint LocalToWorldOffset(FIntPoint Local) const;

	/**
	 * 서브시스템의 등록부에 들어간다. 조각이 다른 목록에 들어갈 수 있도록 오버라이드한다:
	 * 돌아가는 장애물이 밀 수 있는 블록 사이에 나타나면 안 되는데, 그러면 드래그 코드가
	 * 그것을 밀려 들고 회전 타일이 그것을 낯선 조각으로 취급한다.
	 */
	virtual void RegisterWithSubsystem(UPuzzleSubsystem& Subsystem);
	virtual void UnregisterFromSubsystem(UPuzzleSubsystem& Subsystem);

	/** 이전에 점유했던 셀을 모두 풀고 현재 셀을 그리드에 점유(등록)한다. */
	void ClaimCells();

	/** 액터를 현재 사각 영역의 정확한 중심으로 옮긴다. */
	void SnapToRect();

	/**
	 * 조각이 서 있는 바닥 높이를 바꾼다. 층을 옮긴 엘리베이터만 쓴다.
	 *
	 * 슬라이드 목표와 스냅이 모두 이 값을 기준으로 계산되므로, 새 층에 도착한 뒤 갱신하지
	 * 않으면 차체가 옆으로 밀릴 때 원래 층 높이로 되돌아간다.
	 */
	void SetFloorZ(double InFloorZ) { FloorZ = InFloorZ; }

	EAnimState GetAnimState() const { return AnimState; }
	void SetAnimState(EAnimState InState) { AnimState = InState; }

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Block")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Puzzle Block")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** VisualActorClass를 담는 자리. 클래스가 비어 있으면 아무것도 만들지 않는다. */
	UPROPERTY(VisibleAnywhere, Category = "Puzzle Block|Art")
	TObjectPtr<UChildActorComponent> VisualActor;

	/** ArtMesh를 담는 자리. 메시가 비어 있으면 보이지 않는다. 콜리전은 언제나 꺼져 있다. */
	UPROPERTY(VisibleAnywhere, Category = "Puzzle Block|Art")
	TObjectPtr<UStaticMeshComponent> ArtMeshComponent;

	/** 점유한 사각 영역의 최소 모서리, 그리드 셀 단위. */
	FIntPoint MinCell = FIntPoint::ZeroValue;

	int32 QuarterTurns = 0;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	/** 첫 틱에 자기 사각형을 담는 구간을 찾아 기억한다. */
	TWeakObjectPtr<APuzzleRegion> HomeRegion;

	/** 구간 탐색을 첫 틱에 한 번만 하기 위한 표시. */
	bool bHomeRegionResolved = false;

private:
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

	/** 쥔 채로 계속 밀 방향 (SetHeldSlideDirection). 비어 있으면 다음 칸에서 멈춘다. */
	TOptional<EGridDirection> HeldSlideDir;

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
