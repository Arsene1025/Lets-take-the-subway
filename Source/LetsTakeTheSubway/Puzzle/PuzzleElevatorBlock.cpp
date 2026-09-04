// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleElevatorBlock.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APuzzleElevatorBlock::APuzzleElevatorBlock()
{
	FootprintSize = FIntPoint(4, 4);
	MoveAxis = EPuzzleMoveAxis::AxisY;
	Height = 300.0f;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SceneRoot);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DoorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	DoorMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		DoorMesh->SetStaticMesh(CubeFinder.Object);
	}

	// A different greybox material, because the door is the one feature of this block the
	// player has to read at a glance to know which way it can travel.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DoorMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));
	if (DoorMaterialFinder.Succeeded())
	{
		DoorMesh->SetMaterial(0, DoorMaterialFinder.Object);
	}
}

EGridDirection APuzzleElevatorBlock::GetWorldDoorDirection() const
{
	return LTTSGrid::RotateDirection(DoorDirection, GetQuarterTurns());
}

EPuzzleMoveAxis APuzzleElevatorBlock::GetWorldMoveAxis() const
{
	// Derived from the door rather than from the authored axis, so the two can never drift
	// apart after a rotation.
	return LTTSPuzzle::AxisForDirection(GetWorldDoorDirection());
}

void APuzzleElevatorBlock::RefreshVisual()
{
	Super::RefreshVisual();

	if (!DoorMesh)
	{
		return;
	}

	// Placed in the local frame: the actor's yaw already carries the rotation, so the door
	// slab follows the body without any extra maths.
	const double CellSize = 100.0;
	const double HalfX = FootprintSize.X * CellSize * 0.5;
	const double HalfY = FootprintSize.Y * CellSize * 0.5;
	const double Thickness = 20.0;

	// Pushed a half thickness further out so the slab stands proud of the body rather than
	// half sunk into it, where it would read as a seam.
	const FIntPoint Offset = LTTSGrid::DirOffset(DoorDirection);
	const FVector Location(
		Offset.X * (HalfX + Thickness * 0.5),
		Offset.Y * (HalfY + Thickness * 0.5),
		Height * 0.4);

	const bool bAlongX = (Offset.X != 0);
	const FVector Scale(
		bAlongX ? Thickness / 100.0 : FootprintSize.X * CellSize * 0.7 / 100.0,
		bAlongX ? FootprintSize.Y * CellSize * 0.7 / 100.0 : Thickness / 100.0,
		Height * 0.6 / 100.0);

	DoorMesh->SetRelativeLocation(Location);
	DoorMesh->SetRelativeScale3D(Scale);
}

void APuzzleElevatorBlock::OnConstruction(const FTransform& Transform)
{
	// Keep the authored footprint square whatever someone types in: a rectangular elevator
	// would change the cells it covers on every turn, which the rotation rules do not allow.
	FootprintSize = FIntPoint(4, 4);

	Super::OnConstruction(Transform);
}

void APuzzleElevatorBlock::GetDoorFrontCells(TArray<FIntPoint>& OutCells) const
{
	const EGridDirection Door = GetWorldDoorDirection();
	const FIntPoint Step = LTTSGrid::DirOffset(Door);
	const FGridRect Rect = GetRect();

	// Walk the door-facing edge of the footprint and step one cell out from each.
	if (Step.X != 0)
	{
		const int32 EdgeX = (Step.X > 0) ? Rect.MaxInclusive().X : Rect.Min.X;
		for (int32 Y = Rect.Min.Y; Y < Rect.MaxExclusive().Y; ++Y)
		{
			OutCells.Emplace(EdgeX + Step.X, Y);
		}
	}
	else
	{
		const int32 EdgeY = (Step.Y > 0) ? Rect.MaxInclusive().Y : Rect.Min.Y;
		for (int32 X = Rect.Min.X; X < Rect.MaxExclusive().X; ++X)
		{
			OutCells.Emplace(X, EdgeY + Step.Y);
		}
	}
}

bool APuzzleElevatorBlock::IsPawnAtDoor(const AGridPawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	TArray<FIntPoint> FrontCells;
	GetDoorFrontCells(FrontCells);
	return FrontCells.Contains(Pawn->GetCurrentCell());
}

bool APuzzleElevatorBlock::TryBoard(AGridPawn* Pawn, FText* OutReason)
{
	if (!Pawn)
	{
		return false;
	}

	if (IsAnimating())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ElevatorMoving", "The elevator is still moving.");
		}
		return false;
	}

	if (Pawn->IsMoving())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ElevatorPawnMoving", "Wait until you have stopped walking.");
		}
		return false;
	}

	if (!IsPawnAtDoor(Pawn))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ElevatorNotAtDoor", "Walk around to the door first.");
		}
		return false;
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %s boarded from cell (%d,%d); door faces %s."),
		*GetName(), *GetNameSafe(Pawn), Pawn->GetCurrentCell().X, Pawn->GetCurrentCell().Y,
		*StaticEnum<EGridDirection>()->GetNameStringByValue(static_cast<int64>(GetWorldDoorDirection())));

	OnBoarded.Broadcast(this, Pawn);
	return true;
}

#if WITH_EDITOR

bool APuzzleElevatorBlock::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (!InProperty)
	{
		return true;
	}

	// Both are fixed by what an elevator is: 4x4, and travelling along its door.
	const FName Name = InProperty->GetFName();
	return Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, FootprintSize)
		&& Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, MoveAxis);
}

#endif
