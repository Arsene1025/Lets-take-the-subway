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

/**
 * Click a cell to move there, or press and drag a puzzle block to push it.
 *
 * Which of the two a press means is decided by what is under the cursor, and whether a press
 * on a block was a drag or a click is decided at release by whether the block actually
 * travelled. That keeps a single mouse button doing both jobs without a modifier.
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

	bool IsDraggingBlock() const { return DraggedBlock.IsValid(); }

	/** One line describing what the cursor is holding, for the debug overlay. */
	FString GetDragStatusText() const;

private:
	void OnPressed();
	void OnReleased();

	/** Follow the cursor and step the held block along its axis. */
	void UpdateDrag();

	/** Let go of the held block, resolving a press that never moved it as a click. */
	void FinishDrag();

	AGridActor* GetGrid() const;
	AGridPawn* GetGridPawn() const;

	/** The block under the cursor, by mesh hit first and cell occupancy second. */
	APuzzleBlock* FindBlockUnderCursor(const FHitResult& Hit) const;

	// Created with NewObject at runtime, so they need a UPROPERTY to stay rooted, and
	// Transient so they are never serialized.
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ClickAction;

	FString FeedbackText;
	FLinearColor FeedbackColor = FLinearColor::White;

	FIntPoint LastHoveredCell = FIntPoint(MIN_int32, MIN_int32);
	bool bHadHover = false;

	// ---------------------------------------------------------------- Drag state

	TWeakObjectPtr<APuzzleBlock> DraggedBlock;

	/** Where on the block the cursor grabbed it. Cursor travel is measured from here. */
	FVector GrabPoint = FVector::ZeroVector;

	/** Which way the held block is allowed to travel, decided once when it is grabbed. */
	EPuzzleMoveAxis DragAxis = EPuzzleMoveAxis::None;

	/** Steps already applied, so the block returns when the cursor comes back. */
	int32 AppliedOffset = 0;

	/** The offset last refused, so one obstacle produces one message rather than one per frame. */
	int32 LastRefusedOffset = MIN_int32;

	/** Zero at release means the press was a click on the block, not a push. */
	int32 StepsThisDrag = 0;
};
