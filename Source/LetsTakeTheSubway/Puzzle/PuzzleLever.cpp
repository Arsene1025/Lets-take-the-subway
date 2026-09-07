// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleLever.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridFootprint.h"
#include "Grid/GridTypes.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleRotatingObstacle.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APuzzleLever::APuzzleLever()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostMesh"));
	PostMesh->SetupAttachment(SceneRoot);

	WheelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WheelPivot"));
	WheelPivot->SetupAttachment(SceneRoot);

	WheelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelMesh"));
	WheelMesh->SetupAttachment(WheelPivot);

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(WheelPivot);

	// Same collision rule as a puzzle block: seen by the click trace and nothing else, so
	// the cursor can take hold of the wheel without the lever affecting the pawn or physics.
	for (UStaticMeshComponent* Mesh : { PostMesh.Get(), WheelMesh.Get(), HandleMesh.Get() })
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Mesh->SetGenerateOverlapEvents(false);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	if (CubeFinder.Succeeded())
	{
		PostMesh->SetStaticMesh(CubeFinder.Object);
		HandleMesh->SetStaticMesh(CubeFinder.Object);
	}
	if (CylinderFinder.Succeeded())
	{
		WheelMesh->SetStaticMesh(CylinderFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PostMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WheelMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	if (PostMaterialFinder.Succeeded())
	{
		PostMesh->SetMaterial(0, PostMaterialFinder.Object);
	}
	if (WheelMaterialFinder.Succeeded())
	{
		WheelMesh->SetMaterial(0, WheelMaterialFinder.Object);
		HandleMesh->SetMaterial(0, WheelMaterialFinder.Object);
	}

	// The grid traces the floor the lever stands on, not the lever.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

// ---------------------------------------------------------------------------- Visual

void APuzzleLever::RefreshVisual()
{
	constexpr double PostWidth = 22.0;
	constexpr double WheelThickness = 12.0;

	if (PostMesh)
	{
		PostMesh->SetRelativeLocation(FVector(0.0, 0.0, PostHeight * 0.5));
		PostMesh->SetRelativeScale3D(FVector(PostWidth / 100.0, PostWidth / 100.0, PostHeight / 100.0));
	}

	if (WheelPivot)
	{
		WheelPivot->SetRelativeLocation(FVector(0.0, 0.0, PostHeight));
	}

	if (WheelMesh)
	{
		// The engine cylinder is 100 cm across and 100 cm tall, centred on its origin.
		WheelMesh->SetRelativeLocation(FVector::ZeroVector);
		WheelMesh->SetRelativeScale3D(FVector(
			WheelDiameter / 100.0, WheelDiameter / 100.0, WheelThickness / 100.0));
	}

	if (HandleMesh)
	{
		const double Radius = WheelDiameter * 0.5;
		HandleMesh->SetRelativeLocation(FVector(Radius * 0.7, 0.0, WheelThickness * 0.5 + 8.0));
		HandleMesh->SetRelativeScale3D(FVector(0.16, 0.16, 0.16));
	}
}

void APuzzleLever::SetWheelPreviewAngle(float Degrees)
{
	if (WheelPivot)
	{
		WheelPivot->SetRelativeRotation(FRotator(0.0, Degrees, 0.0));
	}
}

FVector APuzzleLever::GetWheelWorldLocation() const
{
	return WheelPivot ? WheelPivot->GetComponentLocation() : GetActorLocation();
}

// ---------------------------------------------------------------------------- Queries

void APuzzleLever::GetOperatingCells(TArray<FIntPoint>& OutCells) const
{
	static const EGridDirection Directions[4] = {
		EGridDirection::North, EGridDirection::East, EGridDirection::South, EGridDirection::West };

	for (const EGridDirection Dir : Directions)
	{
		OutCells.Add(Cell + LTTSGrid::DirOffset(Dir));
	}
}

bool APuzzleLever::IsPawnAdjacent(const AGridPawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	TArray<FIntPoint> Cells;
	GetOperatingCells(Cells);
	return Cells.Contains(Pawn->GetCurrentCell());
}

// ---------------------------------------------------------------------------- Use

bool APuzzleLever::TryTurn(int32 TurnSign, const AGridPawn* Pawn, FText* OutReason)
{
	if (!Target)
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "LeverNoTarget", "This lever is not wired to anything.");
		}
		return false;
	}

	if (!IsPawnAdjacent(Pawn))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "LeverOutOfReach", "Stand next to the lever to work it.");
		}
		return false;
	}

	return Target->TryRotate(TurnSign, OutReason);
}

// ---------------------------------------------------------------------------- Lifecycle

void APuzzleLever::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
}

void APuzzleLever::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; the lever is disabled."), *GetName());
		return;
	}

	Cell = GridFootprint::MinCellFromCentre(*Grid, GetActorLocation(), FIntPoint(1, 1));

	if (!Grid->IsValidCell(Cell))
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: cell (%d,%d) is outside the grid."), *GetName(), Cell.X, Cell.Y);
		return;
	}

	// The lever is something the player walks up to, so it takes its cell like any other
	// obstruction rather than being walked through.
	Grid->SetOccupant(Cell, this);

	SetActorLocation(GridFootprint::CentreFromMinCell(
		*Grid, Cell, FIntPoint(1, 1), Grid->CellToWorld(Cell).Z));
	RefreshVisual();

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->RegisterLever(this);
	}

	if (!Target)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: no target obstacle set; the lever will do nothing."), *GetName());
	}

	// A lever nobody can reach is a level bug that only shows up when a player walks over to
	// it, so it is worth saying at start-up.
	TArray<FIntPoint> Operating;
	GetOperatingCells(Operating);

	int32 Reachable = 0;
	for (const FIntPoint& Neighbour : Operating)
	{
		if (Grid->IsValidCell(Neighbour) && Grid->IsCellWalkableStatic(Neighbour) && !Grid->IsCellOccupied(Neighbour))
		{
			++Reachable;
		}
	}

	if (Reachable == 0)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: no free floor beside it, so the lever cannot be worked."), *GetName());
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: lever at cell (%d,%d), target %s, %d operating cell(s)."),
		*GetName(), Cell.X, Cell.Y, *GetNameSafe(Target), Reachable);
}

void APuzzleLever::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Grid)
	{
		Grid->ClearAllOccupantsOf(this);
	}

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->UnregisterLever(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- Editor

#if WITH_EDITOR

void APuzzleLever::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (!bFinished)
	{
		return;
	}

	const AGridActor* FoundGrid = AGridActor::FindGrid(GetWorld());
	if (!FoundGrid)
	{
		return;
	}

	const FIntPoint Snapped = GridFootprint::MinCellFromCentre(*FoundGrid, GetActorLocation(), FIntPoint(1, 1));
	if (!FoundGrid->IsValidCell(Snapped))
	{
		return;
	}

	Modify();
	SetActorRotation(FRotator::ZeroRotator);
	SetActorLocation(GridFootprint::CentreFromMinCell(
		*FoundGrid, Snapped, FIntPoint(1, 1), FoundGrid->CellToWorld(Snapped).Z));
}

void APuzzleLever::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshVisual();
	PostEditMove(true);
}

#endif
