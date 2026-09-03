// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridPlayerController.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
#include "Player/GridPawn.h"

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

	FeedbackText = TEXT("Click a cell to move.");
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
		EnhancedInput->BindAction(ClickAction, ETriggerEvent::Started, this, &AGridPlayerController::OnClickDestination);
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

bool AGridPlayerController::TraceCursorToCell(FIntPoint& OutCell) const
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

	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(
			Hit,
			WorldOrigin,
			WorldOrigin + WorldDirection * 100000.0,
			Grid->TraceChannel,
			Params))
	{
		return false;
	}

	OutCell = Grid->WorldToCell(Hit.Location);
	return Grid->IsValidCell(OutCell);
}

void AGridPlayerController::OnClickDestination()
{
	AGridActor* Grid = GetGrid();
	AGridPawn* GridPawn = GetGridPawn();

	if (!Grid || !GridPawn)
	{
		ShowFeedback(TEXT("Grid or pawn is not ready."), FLinearColor::Red);
		return;
	}

	FIntPoint Cell;
	if (!TraceCursorToCell(Cell))
	{
		ShowFeedback(TEXT("Click somewhere on the grid."), FLinearColor::Red);
		return;
	}

	GridPawn->RequestMoveToCell(Cell);
}

void AGridPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

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
