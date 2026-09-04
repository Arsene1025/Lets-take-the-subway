// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridPlayerController.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleBlock.h"
#include "Puzzle/PuzzleElevatorBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Camera/PlayerCameraManager.h"
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
	const FCursorPick Pick = PickUnderCursor();
	if (Pick.Kind == FCursorPick::EKind::None)
	{
		return false;
	}

	OutCell = Pick.Cell;
	return true;
}

FCursorPick AGridPlayerController::PickUnderCursor() const
{
	FCursorPick Pick;

	const AGridActor* Grid = GetGrid();
	if (!Grid)
	{
		return Pick;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return Pick;
	}

	const FVector RayEnd = WorldOrigin + WorldDirection * 100000.0;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LTTSGridClick), /*bTraceComplex*/ false, GetPawn());

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, RayEnd, Grid->TraceChannel, Params))
	{
		// Only the top face grabs. A block's sides face the camera and stand in front of the
		// floor behind them, so treating a side hit as a grab would make those cells
		// unreachable by clicking.
		if (APuzzleBlock* Block = Cast<APuzzleBlock>(Hit.GetActor()))
		{
			if (Hit.ImpactNormal.Z > 0.7)
			{
				Pick.Kind = FCursorPick::EKind::Block;
				Pick.Block = Block;
				Pick.Cell = Grid->WorldToCell(Hit.Location);
				Pick.HitLocation = Hit.Location;
				return Pick;
			}
		}
	}

	// Second pass with every block ignored. Run for a side hit, for a miss, and for scenery
	// alike, so the floor answer is produced by one rule rather than depending on what the
	// first ray happened to touch.
	TArray<AActor*> BlockActors;
	if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->GetBlockActors(BlockActors);
	}
	Params.AddIgnoredActors(BlockActors);

	FHitResult FloorHit;
	if (!GetWorld()->LineTraceSingleByChannel(FloorHit, WorldOrigin, RayEnd, Grid->TraceChannel, Params))
	{
		return Pick;
	}

	const FIntPoint Cell = Grid->WorldToCell(FloorHit.Location);
	if (!Grid->IsValidCell(Cell))
	{
		return Pick;
	}

	Pick.Kind = FCursorPick::EKind::Floor;
	Pick.Cell = Cell;
	Pick.HitLocation = FloorHit.Location;
	return Pick;
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

	const FCursorPick Pick = PickUnderCursor();

	if (Pick.Kind == FCursorPick::EKind::Block)
	{
		APuzzleBlock* Block = Pick.Block.Get();
		if (!Block)
		{
			return;
		}

		// Held either way, even when the block cannot move: releasing without travel is a
		// click, which is how an elevator is boarded.
		DraggedBlock = Block;
		GrabPoint = Pick.HitLocation;
		GrabOffset = Pick.HitLocation - Block->GetActorLocation();
		DragAxis = Block->GetWorldMoveAxis();
		bHasRefusedDir = false;
		StepsThisDrag = 0;
		Block->SetHeld(true);
		return;
	}

	if (Pick.Kind == FCursorPick::EKind::Floor)
	{
		GridPawn->RequestMoveToCell(Pick.Cell);
		return;
	}

	ShowFeedback(TEXT("Click somewhere on the grid."), FLinearColor::Red);
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

	// Measured from where the block is now rather than from where the drag started, so a
	// free block can be led around a corner: each step is chosen afresh against the cursor.
	const FVector Target = Cursor - GrabOffset;
	const FVector Current = Block->GetActorLocation();

	double DeltaX = (DragAxis == EPuzzleMoveAxis::AxisY) ? 0.0 : Target.X - Current.X;
	double DeltaY = (DragAxis == EPuzzleMoveAxis::AxisX) ? 0.0 : Target.Y - Current.Y;

	const double Threshold = Grid->CellSize * 0.5;

	// Try the axis the cursor has pulled furthest along first. Falling back to the other one
	// lets a block that is jammed against a wall still slide along it.
	EGridDirection Candidates[2];
	int32 NumCandidates = 0;

	const bool bPreferX = FMath::Abs(DeltaX) >= FMath::Abs(DeltaY);
	const double Primary = bPreferX ? DeltaX : DeltaY;
	const double Secondary = bPreferX ? DeltaY : DeltaX;

	if (FMath::Abs(Primary) >= Threshold)
	{
		Candidates[NumCandidates++] = bPreferX
			? (Primary > 0.0 ? EGridDirection::East : EGridDirection::West)
			: (Primary > 0.0 ? EGridDirection::North : EGridDirection::South);
	}
	if (FMath::Abs(Secondary) >= Threshold)
	{
		Candidates[NumCandidates++] = bPreferX
			? (Secondary > 0.0 ? EGridDirection::North : EGridDirection::South)
			: (Secondary > 0.0 ? EGridDirection::East : EGridDirection::West);
	}

	if (NumCandidates == 0)
	{
		bHasRefusedDir = false;		// cursor is back within half a cell; re-arm the message
		return;
	}

	for (int32 Index = 0; Index < NumCandidates; ++Index)
	{
		if (Block->StartSlide(Candidates[Index]))
		{
			++StepsThisDrag;
			bHasRefusedDir = false;
			return;
		}
	}

	// Nothing moved. Report the direction the player was actually pulling towards.
	const EGridDirection Refused = Candidates[0];
	if (!bHasRefusedDir || LastRefusedDir != Refused)
	{
		FText Reason;
		Block->CanSlide(Refused, &Reason);
		LastRefusedDir = Refused;
		bHasRefusedDir = true;
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
	StepsThisDrag = 0;
	bHasRefusedDir = false;
	DragAxis = EPuzzleMoveAxis::None;
	GrabOffset = FVector::ZeroVector;

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

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	// Run before the drag and the hover so both see the heights the player is looking at.
	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		const AGridPawn* GridPawn = GetGridPawn();
		const float SweepRadius = GridPawn ? GridPawn->BallRadius * 0.5f : 25.0f;
		const FVector CameraLocation = PlayerCameraManager
			? PlayerCameraManager->GetCameraLocation()
			: FVector::ZeroVector;

		Subsystem->UpdateOcclusion(CameraLocation, GridPawn, SweepRadius);
	}
#endif

	if (DraggedBlock.IsValid())
	{
		UpdateDrag();
	}

	UpdateHover();
}

void AGridPlayerController::UpdateHover()
{
	const AGridActor* Grid = GetGrid();

	auto Clear = [this]()
	{
		if (bHadHover)
		{
			FGridRuntimeDebugDrawer::ClearHover(GetWorld());
			bHadHover = false;
			LastHoveredCell = FIntPoint(MIN_int32, MIN_int32);
			LastHoverKind = FCursorPick::EKind::None;
			LastHoveredBlock.Reset();
		}
	};

	if (!LTTSGridDebug::ShouldDrawWorld() || !Grid)
	{
		Clear();
		return;
	}

	const FCursorPick Pick = PickUnderCursor();
	if (Pick.Kind == FCursorPick::EKind::None)
	{
		Clear();
		return;
	}

	// The batch is persistent, so only redraw when the answer actually changes.
	if (bHadHover
		&& Pick.Kind == LastHoverKind
		&& Pick.Cell == LastHoveredCell
		&& Pick.Block == LastHoveredBlock)
	{
		return;
	}

	if (Pick.Kind == FCursorPick::EKind::Block)
	{
		if (const APuzzleBlock* Block = Pick.Block.Get())
		{
			// The whole footprint, so the player can see what they are about to take hold of
			// rather than the single cell the ray happened to land in.
			TArray<FIntPoint> Cells;
			Block->GetRect().GatherCells(Cells);
			FGridRuntimeDebugDrawer::DrawHoverCells(GetWorld(), *Grid, Cells, /*bEnterable*/ true);
		}
	}
	else
	{
		FGridRuntimeDebugDrawer::DrawHover(GetWorld(), *Grid, Pick.Cell, Grid->CanPawnEnter(Pick.Cell, GetPawn()));
	}

	LastHoveredCell = Pick.Cell;
	LastHoverKind = Pick.Kind;
	LastHoveredBlock = Pick.Block;
	bHadHover = true;
}
