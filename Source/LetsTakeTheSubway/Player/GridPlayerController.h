// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "GameFramework/PlayerController.h"
#include "Puzzle/PuzzleTypes.h"
#include "GridPlayerController.generated.h"

class AGridActor;
class AGridEscalator;
class AGridPawn;
class AGridTrain;
class APuzzleBlock;
class APuzzleLever;
class UInputAction;
class UInputMappingContext;

/** 커서가 지금 무엇 위에 있는지. 쿼리마다 한 번만 판정하므로 모든 소비자가 같은 답을 본다. */
struct FCursorPick
{
	enum class EKind : uint8
	{
		None,
		Floor,
		Block,
		Lever,

		/**
		 * 열차. 폰이 지금 정차 중인 열차의 문 앞 셀에 서 있을 때만 잡힌다.
		 *
		 * 조건을 픽 단계에 두는 이유는 차체가 크기 때문이다. 언제나 잡히게 하면 4 m짜리
		 * 객차가 승강장 셀을 가려 그 뒤를 영영 클릭할 수 없고, 반대로 절대 안 잡히게 하면
		 * 탈 방법이 없다.
		 */
		Vehicle,

		/**
		 * 에스컬레이터. 열차와 같은 이유로 폰이 지금 탈 수 있을 때만 잡힌다: 차체가 경사를
		 * 통째로 덮고 있어서 언제나 잡히게 하면 그 뒤 셀을 영영 클릭할 수 없다.
		 */
		Escalator
	};

	EKind Kind = EKind::None;

	/** Floor: 걸어갈 셀. Block 또는 Lever: 커서가 닿은 셀. */
	FIntPoint Cell = FIntPoint::ZeroValue;

	TWeakObjectPtr<APuzzleBlock> Block;

	TWeakObjectPtr<APuzzleLever> Lever;
	TWeakObjectPtr<AGridTrain> Vehicle;
	TWeakObjectPtr<AGridEscalator> Escalator;

	/** 레이가 조각에 닿은 위치. 그랩 지점이 된다. */
	FVector HitLocation = FVector::ZeroVector;

	/**
	 * Block일 때: 모든 블록을 무시하고 다시 트레이스한 바닥 셀.
	 *
	 * 블록 위의 누름이 드래그가 아니라 클릭으로 끝나면 폰이 이리로 간다. 덕분에 블록이
	 * 카메라 쪽으로 옆면을 세우고 그 뒤 셀을 가려도 그 셀로 걸어갈 수 있다.
	 */
	FIntPoint FloorCell = FIntPoint::ZeroValue;

	/** 블록 뒤에서 바닥을 찾지 못했다면(그리드 밖 등) FloorCell은 의미가 없다. */
	bool bHasFloorCell = false;
};

/**
 * 셀을 클릭하면 그리로 이동하고, 퍼즐 블록을 누른 채 드래그하면 민다.
 *
 * 마우스 버튼 하나가 보조 키 없이 두 역할을 다 하되, 무엇을 눌렀느냐가 아니라 **끌었느냐**로
 * 나눈다: 커서가 화면에서 DragStartThresholdPixels 이상 움직이면 드래그(블록 밀기)이고,
 * 그 전에 떼면 클릭(폰 이동)이다. 그래서 블록 위의 누름은 판정이 날 때까지 보류된다.
 *
 * 블록은 어느 면으로든 잡힌다. 카메라가 가파른 각도로 내려다보므로 블록의 옆면은 커서와
 * 그 뒤 바닥 셀 사이를 가로막지만, 그 셀은 이제 블록을 클릭해서 갈 수 있다: 블록 픽은
 * 블록을 무시하고 다시 트레이스한 FloorCell을 함께 들고 다닌다.
 *
 * 입력 오브젝트는 C++에서 만들므로 IA_/IMC_ 애셋은 없다.
 */
UCLASS()
class LETSTAKETHESUBWAY_API AGridPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGridPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	void ShowFeedback(const FString& Message, const FLinearColor& Color);
	const FString& GetFeedbackText() const { return FeedbackText; }
	const FLinearColor& GetFeedbackColor() const { return FeedbackColor; }

	/** 커서 아래의 셀. 커서가 그리드 위에 없으면 false를 돌려준다. */
	bool TraceCursorToCell(FIntPoint& OutCell) const;

	/** 가공하지 않은 커서 히트. 호출자가 블록과 그 아래 바닥을 구분할 수 있게 한다. */
	bool TraceCursor(FHitResult& OutHit) const;

	/** 커서가 무엇 위에 있는지: 블록, 레버, 열차, 바닥 셀, 또는 아무것도 없음. */
	FCursorPick PickUnderCursor() const;

	bool IsDraggingBlock() const { return DraggedBlock.IsValid(); }

	bool IsDraggingLever() const { return DraggedLever.IsValid(); }

	/** 커서가 무엇을 쥐고 있는지 한 줄로. 디버그 오버레이용. */
	FString GetDragStatusText() const;

	/**
	 * 누름이 클릭이 아니라 드래그로 읽히기 시작하는 화면상 거리 (픽셀).
	 *
	 * 월드 거리가 아니라 화면 거리로 재는 이유는, 카메라에서 멀거나 높은 블록일수록 같은
	 * 손짓이 더 큰 월드 거리로 환산되어 손맛이 자리마다 달라지기 때문이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Input", meta = (ClampMin = "0.0"))
	float DragStartThresholdPixels = 8.0f;

private:
	void OnPressed();
	void OnReleased();

	/** 모든 블록을 무시하고 커서 아래 바닥 셀을 구한다. 바닥 판정의 유일한 규칙이다. */
	bool TraceFloorIgnoringBlocks(
		const FVector& RayOrigin,
		const FVector& RayEnd,
		FCollisionQueryParams Params,
		FIntPoint& OutCell,
		FVector& OutHitLocation) const;

	/** 보류 중인 누름이 드래그가 될 만큼 커서가 움직였는지 본다. */
	void UpdatePendingPress();

	/** 보류가 드래그로 확정됐다. 누른 지점 그대로 블록을 쥔다. */
	void BeginBlockDrag(const FCursorPick& Pick);

	/** 블록 위의 누름이 끌리지 않고 끝났다: 엘리베이터면 탑승, 아니면 그 뒤 바닥으로 이동. */
	void HandleBlockClick(const FCursorPick& Pick);

	/** 바닥 셀 하나로 가라는 명령. 막힌 옆 칸이면 부딪히는 연출을 먼저 낸다. */
	void MovePawnToCell(const FIntPoint& Cell);

	/** 커서를 따라가며 쥔 블록을 그쪽으로 한 스텝씩 옮긴다. */
	void UpdateDrag();

	/** 쥔 블록을 놓는다. 한 칸도 못 간 드래그에는 왜 못 갔는지 알려 준다. */
	void FinishDrag();

	/** 쥔 휠 둘레로 커서가 얼마나 돌았는지 추적하고, 회전을 한 번 발동한다. */
	void UpdateLeverDrag();

	/** 휠을 놓고 원래 자리로 되돌린다. */
	void FinishLeverDrag();

	/** 커서 아래 대상에 맞춰 호버 오버레이를 다시 그린다. */
	void UpdateHover();

	AGridActor* GetGrid() const;
	AGridPawn* GetGridPawn() const;

	// 런타임에 NewObject로 만들므로 루트에 붙들어 두려면 UPROPERTY가 필요하고,
	// 직렬화되지 않도록 Transient로 둔다.
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ClickAction;

	FString FeedbackText;
	FLinearColor FeedbackColor = FLinearColor::White;

	FIntPoint LastHoveredCell = FIntPoint(MIN_int32, MIN_int32);
	FCursorPick::EKind LastHoverKind = FCursorPick::EKind::None;
	TWeakObjectPtr<APuzzleBlock> LastHoveredBlock;
	bool bHadHover = false;

	// ---------------------------------------------------------------- 누름 보류
	//
	// 블록 위의 누름은 블록을 밀라는 뜻일 수도, 그 뒤 바닥으로 걸어가라는 뜻일 수도 있다.
	// 커서가 움직여 봐야 알 수 있으므로 판정이 날 때까지 블록을 쥐지 않는다. 미리 쥐었다가
	// 무르면 SetHeld의 강조 표시가 눌렀다 뗄 때마다 깜빡인다.

	/** 누른 순간의 픽. 드래그로 확정되면 이 지점을, 클릭이면 이 FloorCell을 쓴다. */
	FCursorPick PressPick;

	/** 누른 순간의 화면상 커서 위치. 드래그 판정의 기준점이다. */
	FVector2D PressScreenPosition = FVector2D::ZeroVector;

	bool bPressPending = false;

	// ---------------------------------------------------------------- 드래그 상태

	TWeakObjectPtr<APuzzleBlock> DraggedBlock;

	/** 커서가 블록의 어디를 잡았는지. 드래그 평면이 이 점을 지난다. */
	FVector GrabPoint = FVector::ZeroVector;

	/**
	 * 블록 중심 기준의 그랩 지점.
	 *
	 * 드래그 내내 일정하게 유지해서, 블록의 중심이 포인터 아래로 튀어 오지 않고 잡은
	 * 자리 그대로 커서를 따라오게 한다.
	 */
	FVector GrabOffset = FVector::ZeroVector;

	/** 쥔 블록이 이동할 수 있는 축. 잡을 때 한 번 읽는다. */
	EPuzzleMoveAxis DragAxis = EPuzzleMoveAxis::None;

	/** 마지막으로 거부된 방향. 장애물 하나당 메시지 하나만 내고, 프레임마다 내지 않기 위해서다. */
	EGridDirection LastRefusedDir = EGridDirection::North;
	bool bHasRefusedDir = false;

	/** 뗄 때 0이면 드래그는 했지만 블록이 한 칸도 가지 못했다는 뜻이다. */
	int32 StepsThisDrag = 0;

	// ---------------------------------------------------------------- 레버 드래그
	//
	// 휠은 미는 게 아니라 돌리는 것이므로, 제스처를 바닥 위 거리가 아니라 화면상 휠
	// 위치를 중심으로 쓸어 돈 각도로 잰다.

	TWeakObjectPtr<APuzzleLever> DraggedLever;

	/** 휠의 화면상 위치. 드래그 각도는 이 점을 중심으로 잰다. */
	FVector2D LeverScreenCentre = FVector2D::ZeroVector;

	/** 지난 프레임의 휠 중심 기준 커서 각도 (도 단위). */
	double LeverLastAngle = 0.0;

	/** 휠을 잡은 뒤 쓸어 돈 누적 각도. 부호 있음; 반시계 방향이 양수다. */
	double LeverSweptAngle = 0.0;

	/** 드래그 한 번은 90도 회전 한 번이므로, 한 번 발동하면 휠은 더 반응하지 않는다. */
	bool bLeverTurnSpent = false;
};
