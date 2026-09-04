// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PuzzleSubsystem.generated.h"

class AGridActor;
class AGridPawn;
class AGridPlayerController;
class APuzzleBlock;
class APuzzleRotationTile;

/**
 * Keeps track of the puzzle pieces in the level and the rules that span more than one of
 * them.
 *
 * The grid actor owns cell occupancy because that is a property of cells, and every mover
 * benefits from it. What lives here instead is everything that needs to see the puzzle as a
 * whole: which blocks and tiles exist, whether an animation is currently running so input
 * should be ignored, and the "a block came to rest, does a tile want to spin it" rule.
 *
 * Keeping the registries here also means the drag code never has to sweep the level with an
 * actor iterator while the player is moving the mouse.
 */
UCLASS()
class LETSTAKETHESUBWAY_API UPuzzleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem for an actor's world, or null outside a game world. */
	static UPuzzleSubsystem* Get(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	void RegisterBlock(APuzzleBlock* Block);
	void UnregisterBlock(APuzzleBlock* Block);

	void RegisterTile(APuzzleRotationTile* Tile);
	void UnregisterTile(APuzzleRotationTile* Tile);

	const TArray<TWeakObjectPtr<APuzzleBlock>>& GetBlocks() const { return Blocks; }

	int32 GetNumBlocks() const { return Blocks.Num(); }

	/**
	 * True while anything is mid-animation.
	 *
	 * Input is dropped rather than queued during a rotation: the player cannot see the cells
	 * the pieces are heading for, so a click made mid-spin is almost never the one they would
	 * make once it settled.
	 */
	bool IsInputLocked() const;

	APuzzleBlock* FindBlockAtCell(const AGridActor& Grid, FIntPoint Cell) const;

	/** The cell the pawn stands on plus the one it is walking into, if any. */
	void GetPawnReservedCells(TArray<FIntPoint>& OutCells) const;

	AGridPawn* GetGridPawn() const;
	AGridPlayerController* GetGridController() const;

	/** Route a message to the on-screen feedback line, if there is a controller to show it. */
	void ShowFeedback(const FString& Message, const FLinearColor& Color) const;

	/**
	 * A block has finished the last step of a drag. If it now sits entirely inside a rotation
	 * tile, that tile turns the space.
	 */
	void NotifyBlockCameToRest(APuzzleBlock* Block);

private:
	TArray<TWeakObjectPtr<APuzzleBlock>> Blocks;
	TArray<TWeakObjectPtr<APuzzleRotationTile>> Tiles;
};
