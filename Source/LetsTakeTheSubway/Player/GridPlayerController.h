// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Puzzle/PuzzleTypes.h"
#include "GridPlayerController.generated.h"

class AGridActor;
class AGridPawn;
class APuzzleBlock;
class UInputAction;
class UInputMappingContext;

/** What the cursor is currently over. Resolved once per query so every consumer agrees. */
struct FCursorPick
{
	enum class EKind : uint8
	{
		None,
		Floor,
		Block
	};

	EKind Kind = EKind::None;

	/** Floor: the cell to walk to. Block: the cell the cursor landed on. */
	FIntPoint Cell = FIntPoint::ZeroValue;

	TWeakObjectPtr<APuzzleBlock> Block;

	/** Where the ray met the block. Becomes the grab point. */
	FVector HitLocation = FVector::ZeroVector;
};

/**
 * Click a cell to move there, or press and drag a puzzle block to push it.
 *
 * Which of the two a press means is decided by what is under the cursor, and whether a press
 * on a block was a drag or a click is decided at release by whether the block actually
 * travelled. That keeps a single mouse button doing both jobs without a modifier.
 *
 * A block is only grabbed by its top face. Its sides are see-through as far as the cursor is
 * concerned, because the camera looks down at a steep angle: a block's side stands between
 * the cursor and the floor cells behind it, and those cells have to stay clickable.
 *
 * Input objects are built in C++, so no IA_/IMC_ assets exist.
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

	/** Cell under the cursor. Returns false when the cursor is not over the grid. */
	bool TraceCursorToCell(FIntPoint& OutCell) const;

	/** Raw cursor hit, so a caller can tell a block apart from the floor it stands on. */
	bool TraceCursor(FHitResult& OutHit) const;

	/** What the cursor is over: a block's top face, a floor cell, or nothing. */
	FCursorPick PickUnderCursor() const;

	bool IsDraggingBlock() const { return DraggedBlock.IsValid(); }

	/** One line describing what the cursor is holding, for the debug overlay. */
	FString GetDragStatusText() const;

private:
	void OnPressed();
	void OnReleased();

	/** Follow the cursor and step the held block towards it. */
	void UpdateDrag();

	/** Let go of the held block, resolving a press that never moved it as a click. */
	void FinishDrag();

	/** Redraw the hover overlay for whatever the cursor is over. */
	void UpdateHover();

	AGridActor* GetGrid() const;
	AGridPawn* GetGridPawn() const;

	// Created with NewObject at runtime, so they need a UPROPERTY to stay rooted, and
	// Transient so they are never serialized.
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

	// ---------------------------------------------------------------- Drag state

	TWeakObjectPtr<APuzzleBlock> DraggedBlock;

	/** Where on the block the cursor grabbed it. The drag plane passes through here. */
	FVector GrabPoint = FVector::ZeroVector;

	/**
	 * Grab point relative to the block's centre.
	 *
	 * Held constant through the drag so the block follows the cursor from wherever it was
	 * taken hold of, rather than snapping its middle under the pointer.
	 */
	FVector GrabOffset = FVector::ZeroVector;

	/** Which way the held block may travel, read once when it is grabbed. */
	EPuzzleMoveAxis DragAxis = EPuzzleMoveAxis::None;

	/** The direction last refused, so one obstacle produces one message rather than one a frame. */
	EGridDirection LastRefusedDir = EGridDirection::North;
	bool bHasRefusedDir = false;

	/** Zero at release means the press was a click on the block, not a push. */
	int32 StepsThisDrag = 0;
};
