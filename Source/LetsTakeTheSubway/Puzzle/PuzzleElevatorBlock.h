// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Puzzle/PuzzleBlock.h"
#include "PuzzleElevatorBlock.generated.h"

class AGridPawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnElevatorBoarded, APuzzleElevatorBlock*, Elevator, APawn*, Pawn);

/**
 * The goal piece: a 4x4 elevator with a door on one face.
 *
 * Unlike a plain block it can only travel along the axis its door faces, which is what makes
 * the puzzle a puzzle -- reaching the player is not a matter of sliding it anywhere, but of
 * getting it onto a rotation tile so the door comes out pointing somewhere reachable.
 *
 * Boarding is deliberately only an announcement for now. Moving the pawn between floors
 * needs the layered grids and link cells sketched in the greybox plan; when that exists it
 * subscribes to OnBoarded and nothing here has to change.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleElevatorBlock : public APuzzleBlock
{
	GENERATED_BODY()

public:
	APuzzleElevatorBlock();

	/** Which face the door is on, in the block's own frame. Rotation turns it with the block. */
	UPROPERTY(EditAnywhere, Category = "Elevator")
	EGridDirection DoorDirection = EGridDirection::North;

	/** Fired when the pawn steps in. Floor travel hangs off this. */
	UPROPERTY(BlueprintAssignable, Category = "Elevator")
	FOnElevatorBoarded OnBoarded;

	/** The door's facing as it lies on the grid right now. */
	EGridDirection GetWorldDoorDirection() const;

	/** The block only travels along the door's axis, whatever the authored MoveAxis says. */
	virtual EPuzzleMoveAxis GetWorldMoveAxis() const override;

	/** The row of cells immediately outside the door. These are where the pawn boards from. */
	void GetDoorFrontCells(TArray<FIntPoint>& OutCells) const;

	/** True when the pawn is standing on one of the door-front cells. */
	bool IsPawnAtDoor(const AGridPawn* Pawn) const;

	/** Step the pawn in, or explain why it cannot. */
	bool TryBoard(AGridPawn* Pawn, FText* OutReason = nullptr);

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

protected:
	virtual void RefreshVisual() override;

	/** A slab on the door face, so the opening is readable in the greybox. */
	UPROPERTY(VisibleAnywhere, Category = "Elevator")
	TObjectPtr<UStaticMeshComponent> DoorMesh;
};
