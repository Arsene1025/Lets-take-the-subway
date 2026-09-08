// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Puzzle/PuzzleTypes.h"
#include "GridPlayerController.generated.h"

class AGridActor;
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
		Vehicle
	};

	EKind Kind = EKind::None;

	/** Floor: 걸어갈 셀. Block 또는 Lever: 커서가 닿은 셀. */
	FIntPoint Cell = FIntPoint::ZeroValue;

	TWeakObjectPtr<APuzzleBlock> Block;

	TWeakObjectPtr<APuzzleLever> Lever;
	TWeakObjectPtr<AGridTrain> Vehicle;

	/** 레이가 조각에 닿은 위치. 그랩 지점이 된다. */
	FVector HitLocation = FVector::ZeroVector;
};

/**
 * 셀을 클릭하면 그리로 이동하고, 퍼즐 블록을 누른 채 드래그하면 민다.
 *
 * 누름이 둘 중 무엇을 뜻하는지는 커서 아래에 무엇이 있느냐로 정하고, 블록 위의 누름이
 * 드래그였는지 클릭이었는지는 뗄 때 블록이 실제로 이동했는지로 정한다. 덕분에 마우스
 * 버튼 하나가 보조 키 없이 두 역할을 다 한다.
 *
 * 블록은 윗면으로만 잡힌다. 커서 입장에서 옆면은 투명한데, 카메라가 가파른 각도로
 * 내려다보기 때문이다: 블록의 옆면은 커서와 그 뒤 바닥 셀 사이를 가로막고, 그 셀들은
 * 계속 클릭 가능해야 한다.
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

	/** 커서가 무엇 위에 있는지: 블록의 윗면, 바닥 셀, 또는 아무것도 없음. */
	FCursorPick PickUnderCursor() const;

	bool IsDraggingBlock() const { return DraggedBlock.IsValid(); }

	bool IsDraggingLever() const { return DraggedLever.IsValid(); }

	/** 커서가 무엇을 쥐고 있는지 한 줄로. 디버그 오버레이용. */
	FString GetDragStatusText() const;

private:
	void OnPressed();
	void OnReleased();

	/** 커서를 따라가며 쥔 블록을 그쪽으로 한 스텝씩 옮긴다. */
	void UpdateDrag();

	/** 쥔 블록을 놓는다. 블록이 전혀 움직이지 않은 누름은 클릭으로 처리한다. */
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

	/** 뗄 때 0이면 누름은 블록을 민 것이 아니라 클릭이었다는 뜻이다. */
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
