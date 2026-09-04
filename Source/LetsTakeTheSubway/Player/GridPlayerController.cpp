// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridPlayerController.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleBlock.h"
#include "Puzzle/PuzzleElevatorBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "CollisionQueryParams.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

AGridPlayerController::AGridPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AGridPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Click-to-move needs a visible, unlocked cursor. Without this the viewport captures it
	// and the deprojected click position is wrong.
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	FeedbackText = TEXT("Click a cell to move. Drag a block to push it.");
}

void AGridPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!MappingContext)
	{
		MappingContext = NewObject<UInputMappingContext>(this, TEXT("GridMappingContext"));

		ClickAction = NewObject<UInputAction>(this, TEXT("GridClick"));
		ClickAction->ValueType = EInputActionValueType::Boolean;

		MappingContext->MapKey(ClickAction, EKeys::LeftMouseButton);
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(MappingContext, 0);
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInput->BindAction(ClickAction, ETriggerEvent::Started, this, &AGridPlayerController::OnPressed);
		EnhancedInput->BindAction(ClickAction, ETriggerEvent::Completed, this, &AGridPlayerController::OnReleased);
	}
}

AGridActor* AGridPlayerController::GetGrid() const
{
	return AGridActor::FindGrid(GetWorld());
}

AGridPawn* AGridPlayerController::GetGridPawn() const
{
	return Cast<AGridPawn>(GetPawn());
}

void AGridPlayerController::ShowFeedback(const FString& Message, const FLinearColor& Color)
{
	FeedbackText = Message;
	FeedbackColor = Color;
}

bool AGridPlayerController::TraceCursor(FHitResult& OutHit) const
{
	const AGridActor* Grid = GetGrid();
	if (!Grid)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return false;
	}

	// GetHitResultUnderCursorByChannel cannot ignore actors, and the pawn sits between the
	// camera and the floor, so trace manually.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LTTSGridClick), /*bTraceComplex*/ false, GetPawn());

	return GetWorld()->LineTraceSingleByChannel(
		OutHit,
		WorldOrigin,
		WorldOrigin + WorldDirection * 100000.0,
		Grid->TraceChannel,
		Params);
}

bool AGridPlayerController::TraceCursorToCell(FIntPoint& OutCell) const
{
	const AGridActor* Grid = GetGrid();

	FHitResult Hit;
	if (!Grid || !TraceCursor(Hit))
	{
		return false;
	}

	OutCell = Grid->WorldToCell(Hit.Location);
	return Grid->IsValidCell(OutCell);
}

APuzzleBlock* AGridPlayerController::FindBlockUnderCursor(const FHitResult& Hit) const
{
	if (APuzzleBlock* Direct = Cast<APuzzleBlock>(Hit.GetActor()))
	{
		return Direct;
	}

	// The ray may have landed on the floor beside a block's mesh, or on scenery standing in
	// the same cell; the occupancy map is the authority on what is actually there.
	const AGridActor* Grid = GetGrid();
	const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
	if (!Grid || !Subsystem)
	{
		return nullptr;
	}

	const FIntPoint Cell = Grid->WorldToCell(Hit.Location);
	return Grid->IsValidCell(Cell) ? Subsystem->FindBlockAtCell(*Grid, Cell) : nullptr;
}

void AGridPlayerController::OnPressed()
{
	AGridActor* Grid = GetGrid();
	AGridPawn* GridPawn = GetGridPawn();

	if (!Grid || !GridPawn)
	{
		ShowFeedback(TEXT("Grid or pawn is not ready."), FLinearColor::Red);
		return;
	}

	if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		if (Subsystem->IsInputLocked())
		{
			ShowFeedback(TEXT("Wait for the pieces to settle."), FLinearColor(1.0f, 0.65f, 0.05f));
			return;
		}
	}

	FHitResult Hit;
	if (!TraceCursor(Hit))
	{
		ShowFeedback(TEXT("Click somewhere on the grid."), FLinearColor::Red);
		return;
	}

	if (APuzzleBlock* Block = FindBlockUnderCursor(Hit))
	{
		// Held either way, even when the block cannot move: releasing without travel is a
		// click, which is how an elevator is boarded.
		DraggedBlock = Block;
		GrabPoint = Hit.Location;
		DragAxis = Block->GetWorldMoveAxis();
		AppliedOffset = 0;
		LastRefusedOffset = MIN_int32;
		StepsThisDrag = 0;
		Block->SetHeld(true);
		return;
	}

	const FIntPoint Cell = Grid->WorldToCell(Hit.Location);
	if (!Grid->IsValidCell(Cell))
	{
		ShowFeedback(TEXT("Click somewhere on the grid."), FLinearColor::Red);
		return;
	}

	GridPawn->RequestMoveToCell(Cell);
}

void AGridPlayerController::OnReleased()
{
	FinishDrag();
}

void AGridPlayerController::UpdateDrag()
{
	APuzzleBlock* Block = DraggedBlock.Get();
	const AGridActor* Grid = GetGrid();

	if (!Block || !Grid)
	{
		FinishDrag();
		return;
	}

	// The release event is lost when the cursor leaves the viewport with the button down,
	// which would otherwise leave the block stuck to the mouse.
	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		FinishDrag();
		return;
	}

	if (Block->IsAnimating())
	{
		return;		// one cell at a time; wait for the step to land
	}

	if (DragAxis == EPuzzleMoveAxis::None)
	{
		return;		// immovable: held only so a release still registers as a click
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return;
	}

	// Intersected with the plane the block was grabbed on rather than traced: a trace would
	// hit the block's own top face, and the cell under that hit slides away with perspective
	// as the block gets taller.
	const FVector Cursor = FMath::LinePlaneIntersection(
		WorldOrigin,
		WorldOrigin + WorldDirection * 100000.0,
		FPlane(GrabPoint, FVector::UpVector));

	const FVector Delta = Cursor - GrabPoint;

	// A free block commits to whichever way the cursor first travelled half a cell, and
	// keeps it for the rest of the drag. Without that the block would jitter between the two
	// axes whenever the cursor moved diagonally.
	if (DragAxis == EPuzzleMoveAxis::Both)
	{
		const double Threshold = Grid->CellSize * 0.5;
		if (FMath::Max(FMath::Abs(Delta.X), FMath::Abs(Delta.Y)) < Threshold)
		{
			return;
		}
		DragAxis = (FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y)) ? EPuzzleMoveAxis::AxisX : EPuzzleMoveAxis::AxisY;
	}

	EGridDirection Negative;
	EGridDirection Positive;
	if (!LTTSPuzzle::GetAxisDirections(DragAxis, Negative, Positive))
	{
		return;
	}

	const double AlongAxis = (DragAxis == EPuzzleMoveAxis::AxisX) ? Delta.X : Delta.Y;
	const int32 Desired = FMath::RoundToInt32(AlongAxis / Grid->CellSize);

	if (Desired == AppliedOffset)
	{
		return;
	}

	const int32 Step = (Desired > AppliedOffset) ? 1 : -1;
	const EGridDirection Dir = (Step > 0) ? Positive : Negative;

	if (Block->StartSlide(Dir))
	{
		AppliedOffset += Step;
		++StepsThisDrag;
		LastRefusedOffset = MIN_int32;
		return;
	}

	FText Reason;
	Block->CanSlide(Dir, &Reason);
	if (Desired != LastRefusedOffset)
	{
		LastRefusedOffset = Desired;
		ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
	}
}

void AGridPlayerController::FinishDrag()
{
	APuzzleBlock* Block = DraggedBlock.Get();
	DraggedBlock.Reset();

	if (!Block)
	{
		return;
	}

	const int32 Steps = StepsThisDrag;
	AppliedOffset = 0;
	StepsThisDrag = 0;
	LastRefusedOffset = MIN_int32;
	DragAxis = EPuzzleMoveAxis::None;

	Block->SetHeld(false);

	if (Steps > 0)
	{
		return;		// a push; the rotation check fires when the last step lands
	}

	// A press that moved nothing is a click on the piece.
	if (APuzzleElevatorBlock* Elevator = Cast<APuzzleElevatorBlock>(Block))
	{
		FText Reason;
		if (Elevator->TryBoard(GetGridPawn(), &Reason))
		{
			ShowFeedback(TEXT("You step into the elevator."), FLinearColor(0.45f, 0.85f, 1.0f));
		}
		else
		{
			ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
		}
		return;
	}

	const EPuzzleMoveAxis Axis = Block->GetWorldMoveAxis();
	const TCHAR* AxisText =
		(Axis == EPuzzleMoveAxis::AxisX) ? TEXT("east and west") :
		(Axis == EPuzzleMoveAxis::AxisY) ? TEXT("north and south") :
		(Axis == EPuzzleMoveAxis::Both) ? TEXT("any direction") : TEXT("nowhere");

	ShowFeedback(FString::Printf(TEXT("Drag to push. This block moves %s."), AxisText), FLinearColor::White);
}

FString AGridPlayerController::GetDragStatusText() const
{
	const APuzzleBlock* Block = DraggedBlock.Get();
	if (!Block)
	{
		return FString();
	}

	return FString::Printf(TEXT("Holding %s (%d step(s))"), *Block->GetName(), StepsThisDrag);
}

void AGridPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (DraggedBlock.IsValid())
	{
		UpdateDrag();
	}

	if (!LTTSGridDebug::ShouldDrawWorld())
	{
		if (bHadHover)
		{
			FGridRuntimeDebugDrawer::ClearHover(GetWorld());
			bHadHover = false;
			LastHoveredCell = FIntPoint(MIN_int32, MIN_int32);
		}
		return;
	}

	const AGridActor* Grid = GetGrid();
	if (!Grid)
	{
		return;
	}

	FIntPoint Cell;
	if (!TraceCursorToCell(Cell))
	{
		if (bHadHover)
		{
			FGridRuntimeDebugDrawer::ClearHover(GetWorld());
			bHadHover = false;
			LastHoveredCell = FIntPoint(MIN_int32, MIN_int32);
		}
		return;
	}

	// Only redraw when the hovered cell actually changes: the batch is persistent.
	if (bHadHover && Cell == LastHoveredCell)
	{
		return;
	}

	FGridRuntimeDebugDrawer::DrawHover(GetWorld(), *Grid, Cell, Grid->CanPawnEnter(Cell, GetPawn()));
	LastHoveredCell = Cell;
	bHadHover = true;
}
