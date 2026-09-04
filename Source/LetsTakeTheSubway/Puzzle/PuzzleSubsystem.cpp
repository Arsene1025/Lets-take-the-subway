// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleSubsystem.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"
#include "Player/GridPlayerController.h"
#include "Puzzle/PuzzleBlock.h"
#include "Puzzle/PuzzleRotationTile.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UPuzzleSubsystem* UPuzzleSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UPuzzleSubsystem>() : nullptr;
}

bool UPuzzleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Editor worlds have no gameplay, and the editor preview of a block is driven entirely
	// by its own construction script.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ---------------------------------------------------------------------------- Registries

void UPuzzleSubsystem::RegisterBlock(APuzzleBlock* Block)
{
	if (Block)
	{
		Blocks.AddUnique(Block);
	}
}

void UPuzzleSubsystem::UnregisterBlock(APuzzleBlock* Block)
{
	Blocks.RemoveAll([Block](const TWeakObjectPtr<APuzzleBlock>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Block;
	});
}

void UPuzzleSubsystem::RegisterTile(APuzzleRotationTile* Tile)
{
	if (Tile)
	{
		Tiles.AddUnique(Tile);
	}
}

void UPuzzleSubsystem::UnregisterTile(APuzzleRotationTile* Tile)
{
	Tiles.RemoveAll([Tile](const TWeakObjectPtr<APuzzleRotationTile>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Tile;
	});
}

// ---------------------------------------------------------------------------- Queries

bool UPuzzleSubsystem::IsInputLocked() const
{
	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Blocks)
	{
		if (const APuzzleBlock* Block = Entry.Get())
		{
			if (Block->IsAnimating())
			{
				return true;
			}
		}
	}

	for (const TWeakObjectPtr<APuzzleRotationTile>& Entry : Tiles)
	{
		if (const APuzzleRotationTile* Tile = Entry.Get())
		{
			if (Tile->IsRotating())
			{
				return true;
			}
		}
	}

	return false;
}

APuzzleBlock* UPuzzleSubsystem::FindBlockAtCell(const AGridActor& Grid, FIntPoint Cell) const
{
	return Cast<APuzzleBlock>(Grid.GetOccupant(Cell));
}

void UPuzzleSubsystem::GetBlockActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reserve(OutActors.Num() + Blocks.Num());
	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Blocks)
	{
		if (APuzzleBlock* Block = Entry.Get())
		{
			OutActors.Add(Block);
		}
	}
}

void UPuzzleSubsystem::UpdateOcclusion(const FVector& CameraLocation, const APawn* Pawn, float SweepRadius)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSet<const APuzzleBlock*> Blocking;

	if (Pawn)
	{
		// Tested against each block's authored volume rather than by tracing its collision.
		// A physics sweep would read the flattened mesh, so a block that ducked out of the
		// way would immediately stop occluding, stand up, and occlude again every frame.
		const FVector PawnLocation = Pawn->GetActorLocation();
		const FVector Extent(FMath::Max(SweepRadius, 1.0f));

		for (const TWeakObjectPtr<APuzzleBlock>& Entry : Blocks)
		{
			const APuzzleBlock* Block = Entry.Get();
			if (!Block)
			{
				continue;
			}

			const FBox Bounds = Block->GetFullBounds();
			if (!Bounds.IsValid)
			{
				continue;
			}

			FVector HitLocation;
			FVector HitNormal;
			float HitTime = 0.0f;
			if (FMath::LineExtentBoxIntersection(Bounds, CameraLocation, PawnLocation, Extent, HitLocation, HitNormal, HitTime))
			{
				Blocking.Add(Block);
			}
		}
	}

	NumOccludingBlocks = Blocking.Num();

	// Every block is told either way, so one that stopped blocking stands back up.
	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Blocks)
	{
		if (APuzzleBlock* Block = Entry.Get())
		{
			Block->SetCutaway(Blocking.Contains(Block));
		}
	}
}

AGridPawn* UPuzzleSubsystem::GetGridPawn() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return Controller ? Cast<AGridPawn>(Controller->GetPawn()) : nullptr;
}

AGridPlayerController* UPuzzleSubsystem::GetGridController() const
{
	const UWorld* World = GetWorld();
	return World ? Cast<AGridPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

void UPuzzleSubsystem::GetPawnReservedCells(TArray<FIntPoint>& OutCells) const
{
	const AGridPawn* Pawn = GetGridPawn();
	if (!Pawn)
	{
		return;
	}

	OutCells.Add(Pawn->GetCurrentCell());

	if (const TOptional<FIntPoint> Next = Pawn->GetNextCell())
	{
		OutCells.Add(Next.GetValue());
	}
}

void UPuzzleSubsystem::ShowFeedback(const FString& Message, const FLinearColor& Color) const
{
	if (AGridPlayerController* Controller = GetGridController())
	{
		Controller->ShowFeedback(Message, Color);
	}
}

// ---------------------------------------------------------------------------- Rules

void UPuzzleSubsystem::NotifyBlockCameToRest(APuzzleBlock* Block)
{
	if (!Block)
	{
		return;
	}

	for (const TWeakObjectPtr<APuzzleRotationTile>& Entry : Tiles)
	{
		APuzzleRotationTile* Tile = Entry.Get();
		if (!Tile || !Tile->FullyContains(*Block))
		{
			continue;
		}

		// Tiles never overlap, so at most one can hold the block; stop at the first.
		FText Reason;
		if (!Tile->TryRotate(&Reason))
		{
			ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
		}
		return;
	}
}
