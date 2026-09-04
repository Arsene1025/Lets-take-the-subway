// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridHUD.h"

#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
#include "Player/GridPawn.h"
#include "Player/GridPlayerController.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Engine/Engine.h"

void AGridHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!LTTSGridDebug::ShouldDrawHUD())
	{
		return;
	}

	AGridPlayerController* GridController = Cast<AGridPlayerController>(GetOwningPlayerController());
	AGridPawn* GridPawn = GridController ? Cast<AGridPawn>(GridController->GetPawn()) : nullptr;
	const AGridActor* Grid = GridPawn ? GridPawn->GetGrid() : AGridActor::FindGrid(GetWorld());

	const FLinearColor Label(0.75f, 0.82f, 0.90f);
	const FLinearColor Value(1.0f, 0.82f, 0.2f);

	float Y = 30.0f;
	const float X = 35.0f;
	const float LineHeight = 23.0f;

	DrawText(TEXT("GRID MOVEMENT"), FLinearColor::White, X, Y, GEngine->GetMediumFont(), 1.1f);
	Y += LineHeight * 1.4f;

	DrawText(TEXT("Left Click: move to cell   Left Drag: push a block"), Label, X, Y);
	Y += LineHeight;
	DrawText(TEXT("Click the elevator from a door-side cell to board"), Label, X, Y);
	Y += LineHeight;
	DrawText(TEXT("Console: ltts.GridDebug 0 | 1 | 2"), Label, X, Y);
	Y += LineHeight * 1.4f;

	if (Grid)
	{
		DrawText(
			FString::Printf(TEXT("Grid %dx%d  walkable %d  blocked %d  noFloor %d  stageClear %d  conditional %d"),
				Grid->SizeInCells.X, Grid->SizeInCells.Y,
				Grid->NumWalkable, Grid->NumBlocked, Grid->NumNoFloor,
				Grid->NumStageClear, Grid->NumConditional),
			Label, X, Y);
		Y += LineHeight;
	}
	else
	{
		DrawText(TEXT("No grid actor in this level."), FLinearColor::Red, X, Y);
		Y += LineHeight;
	}

	if (GridPawn)
	{
		const FIntPoint Current = GridPawn->GetCurrentCell();
		const FIntPoint Goal = GridPawn->GetGoalCell();

		DrawText(
			FString::Printf(TEXT("Current (%d,%d)%s"), Current.X, Current.Y,
				GridPawn->IsMoving() ? TEXT("  [moving]") : TEXT("")),
			Value, X, Y);
		Y += LineHeight;

		DrawText(
			FString::Printf(TEXT("Goal (%d,%d)   steps left %d"), Goal.X, Goal.Y, GridPawn->GetRemainingSteps()),
			Value, X, Y);
		Y += LineHeight;

		if (const FGridCellData* Cell = Grid ? Grid->GetCell(Current) : nullptr)
		{
			DrawText(
				FString::Printf(TEXT("Floor Z %.1f   slope %.1f deg"), Cell->FloorZ, Cell->SlopeDeg),
				Label, X, Y);
			Y += LineHeight;
		}
	}

	if (const UPuzzleSubsystem* Puzzle = UPuzzleSubsystem::Get(this))
	{
		FString Status = FString::Printf(TEXT("Blocks %d   occupied cells %d"),
			Puzzle->GetNumBlocks(), Grid ? Grid->GetNumOccupiedCells() : 0);

		if (Puzzle->IsInputLocked())
		{
			Status += TEXT("   [pieces moving]");
		}
		else if (GridController && GridController->IsDraggingBlock())
		{
			Status += TEXT("   ") + GridController->GetDragStatusText();
		}

		DrawText(Status, Label, X, Y);
		Y += LineHeight;
	}

	if (GridController && !GridController->GetFeedbackText().IsEmpty())
	{
		Y += LineHeight * 0.4f;
		DrawText(GridController->GetFeedbackText(), GridController->GetFeedbackColor(), X, Y, GEngine->GetMediumFont());
	}
}
