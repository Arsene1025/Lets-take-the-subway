// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleRotationTile.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APuzzleRotationTile::APuzzleRotationTile()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(SceneRoot);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PadMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PadMesh->SetGenerateOverlapEvents(false);

	CornerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CornerMesh"));
	CornerMesh->SetupAttachment(SceneRoot);
	CornerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CornerMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CornerMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		PadMesh->SetStaticMesh(CubeFinder.Object);
		CornerMesh->SetStaticMesh(CubeFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	if (MaterialFinder.Succeeded())
	{
		PadMesh->SetMaterial(0, MaterialFinder.Object);
		CornerMesh->SetMaterial(0, MaterialFinder.Object);
	}

	// 패드는 바닥에 놓인 장식이다. 그리드가 이것을 지오메트리로 트레이스하면 안 된다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

	// 쿠커가 읽는 저작 데이터. 에디터 빌드 전용.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void APuzzleRotationTile::RefreshVisual()
{
	const double CellSize = Grid ? Grid->CellSize : 100.0;
	const double Span = SizeInCells * CellSize / 100.0;

	// 에디터 그리드 오버레이는 셀 쿼드를 바닥에서 2 cm 위에 그리므로, 바닥에 딱 붙은 패드는
	// 그것과 같은 평면에 놓여 뷰포트에서 찢어져 보인다. 둘 다 피해서 띄운다.
	if (PadMesh)
	{
		PadMesh->SetRelativeLocation(FVector(0.0, 0.0, 5.0));
		PadMesh->SetRelativeScale3D(FVector(Span, Span, 0.04));
	}

	if (CornerMesh)
	{
		const double Inset = (SizeInCells - 1) * CellSize * 0.5;
		CornerMesh->SetRelativeLocation(FVector(Inset, Inset, 14.0));
		CornerMesh->SetRelativeScale3D(FVector(CellSize / 200.0, CellSize / 200.0, 0.12));
	}
}

void APuzzleRotationTile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
}

void APuzzleRotationTile::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; the tile is disabled."), *GetName());
		bDisabled = true;
		return;
	}

	const FIntPoint Size(SizeInCells, SizeInCells);
	Region = FGridRect(GridFootprint::MinCellFromCentre(*Grid, GetActorLocation(), Size), Size);

	TArray<FIntPoint> Cells;
	Region.GatherCells(Cells);

	for (const FIntPoint& Cell : Cells)
	{
		if (!Grid->IsValidCell(Cell))
		{
			UE_LOG(LogLTTSGrid, Error,
				TEXT("%s: cell (%d,%d) is outside the grid; the tile is disabled."), *GetName(), Cell.X, Cell.Y);
			bDisabled = true;
			return;
		}

		if (!Grid->IsCellWalkableStatic(Cell))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: cell (%d,%d) is not walkable floor; rotations landing there will be refused."),
				*GetName(), Cell.X, Cell.Y);
		}
	}

	// 변의 길이가 짝수이므로 영역의 중심이 셀 모서리에 떨어진다. 회전한 모든 풋프린트를
	// 재는 기준이 되는 회전축이 정확히 그 점이다.
	const FVector Origin = Grid->GetGridOrigin();
	PivotWorld = FVector(
		Origin.X + (Region.Min.X + SizeInCells * 0.5) * Grid->CellSize,
		Origin.Y + (Region.Min.Y + SizeInCells * 0.5) * Grid->CellSize,
		Grid->CellToWorld(Region.Min).Z);

	SetActorLocation(FVector(PivotWorld.X, PivotWorld.Y, PivotWorld.Z));
	RefreshVisual();

	// 겹치는 타일들은 그 사이에 멈춘 블록을 둘 다 자기 것이라 주장하게 되고, 어느 쪽이
	// 이기는지는 등록 순서가 결정하게 된다.
	for (TActorIterator<APuzzleRotationTile> It(GetWorld()); It; ++It)
	{
		const APuzzleRotationTile* Other = *It;
		if (Other && Other != this && !Other->bDisabled && Other->Region.Overlaps(Region))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: overlaps rotation tile %s. Tiles must be disjoint."), *GetName(), *Other->GetName());
		}
	}

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->RegisterTile(this);
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %dx%d rotation tile at cell (%d,%d), %s."),
		*GetName(), SizeInCells, SizeInCells, Region.Min.X, Region.Min.Y,
		bClockwise ? TEXT("clockwise") : TEXT("counter-clockwise"));
}

void APuzzleRotationTile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->UnregisterTile(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- 포함 판정

bool APuzzleRotationTile::FullyContains(const APuzzleBlock& Block) const
{
	return !bDisabled && Region.ContainsRect(Block.GetRect());
}

bool APuzzleRotationTile::Straddles(const APuzzleBlock& Block) const
{
	if (bDisabled)
	{
		return false;
	}

	const FGridRect BlockRect = Block.GetRect();
	return Region.Overlaps(BlockRect) && !Region.ContainsRect(BlockRect);
}

// ---------------------------------------------------------------------------- 회전

bool APuzzleRotationTile::TryRotate(FText* OutReason)
{
	if (bDisabled || bRotating || !Grid)
	{
		return false;
	}

	UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
	if (!Subsystem)
	{
		return false;
	}

	const int32 TurnSign = bClockwise ? 1 : -1;

	// 첫 번째 패스(검사 패스): 모든 피스가 들어맞는다는 것이 확인되기 전에는 아무것도
	// 커밋하지 않는다. 중간에 실패한 회전은 플레이어가 도달할 수 없는 상태로 퍼즐을 남긴다.
	TArray<APuzzleBlock*> Inside;
	TArray<FGridRect> Destinations;

	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Subsystem->GetBlocks())
	{
		APuzzleBlock* Block = Entry.Get();
		if (!Block)
		{
			continue;
		}

		if (Straddles(*Block))
		{
			if (OutReason)
			{
				*OutReason = FText::Format(
					NSLOCTEXT("LTTSPuzzle", "TileStraddled", "Cannot rotate: {0} is half on the tile."),
					FText::FromString(Block->GetName()));
			}
			return false;
		}

		if (!FullyContains(*Block))
		{
			continue;
		}

		const FGridRect Destination = GridFootprint::RotateRect(Block->GetRect(), Region, TurnSign);

		TArray<FIntPoint> Cells;
		Destination.GatherCells(Cells);
		for (const FIntPoint& Cell : Cells)
		{
			if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
			{
				if (OutReason)
				{
					*OutReason = NSLOCTEXT("LTTSPuzzle", "TileNoFloor", "Cannot rotate: there is no floor to turn into.");
				}
				return false;
			}
		}

		Inside.Add(Block);
		Destinations.Add(Destination);
	}

	if (Inside.IsEmpty())
	{
		return false;
	}

	// 목적지 셀에 서 있는 그 외의 무언가가 회전을 막는다. 위의 블록들은 모두 계산에 포함되어
	// 있으므로, 여기서 잡히는 것은 타일이 실어 나르지 않는 피스다: 레버, 또는 타일과 겹친
	// 돌아가는 장애물. 이들은 블록 레지스트리에 없어서, 이 검사가 없으면 타일이 블록을 그
	// 안으로 조용히 돌려 넣게 된다.
	{
		TSet<const AActor*> Participants;
		Participants.Reserve(Inside.Num());
		for (const APuzzleBlock* Block : Inside)
		{
			Participants.Add(Block);
		}

		for (const FGridRect& Destination : Destinations)
		{
			TArray<FIntPoint> Cells;
			Destination.GatherCells(Cells);

			for (const FIntPoint& Cell : Cells)
			{
				const AActor* Occupant = Grid->GetOccupant(Cell);
				if (Occupant && !Participants.Contains(Occupant))
				{
					if (OutReason)
					{
						*OutReason = FText::Format(
							NSLOCTEXT("LTTSPuzzle", "TileBlockedByObject", "Cannot rotate: {0} is in the way."),
							FText::FromString(Occupant->GetName()));
					}
					return false;
				}
			}
		}
	}

	// 폰도 공간의 일부다. 먼저 멈추는 이유는, 두 셀 사이에 걸린 폰은 회전축을 중심으로 실어
	// 나를 단일 셀이 없기 때문이다.
	AGridPawn* Pawn = Subsystem->GetGridPawn();
	PendingPawnCell.Reset();

	if (Pawn && Region.Contains(Pawn->GetCurrentCell()))
	{
		const FIntPoint Local = Pawn->GetCurrentCell() - Region.Min;
		const FIntPoint Rotated = Region.Min + GridFootprint::RotateLocalCell(Local, SizeInCells, TurnSign);

		if (!Grid->IsCellWalkableStatic(Rotated))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "TilePawnNoFloor", "Cannot rotate: you would end up off the floor.");
			}
			return false;
		}

		PendingPawnCell = Rotated;
	}

	if (Pawn && Pawn->IsMoving())
	{
		Pawn->StopAndSnapToCurrentCell(TEXT("the space is rotating"));
	}

	// 두 번째 패스(커밋 패스). 각 블록이 목적지 셀을 즉시 점유하므로, 비주얼은 0.4 s가
	// 걸리더라도 다른 모든 시스템의 관점에서는 교체가 원자적이다.
	for (int32 Index = 0; Index < Inside.Num(); ++Index)
	{
		Inside[Index]->BeginRotation(PivotWorld, TurnSign, RotateDuration, Destinations[Index]);
	}

	bRotating = true;
	RotationElapsed = 0.0f;

	Subsystem->ShowFeedback(
		FString::Printf(TEXT("The space turns %s."), bClockwise ? TEXT("clockwise") : TEXT("counter-clockwise")),
		FLinearColor(0.45f, 0.85f, 1.0f));

	return true;
}

void APuzzleRotationTile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bRotating)
	{
		return;
	}

	RotationElapsed += DeltaSeconds;
	if (RotationElapsed < RotateDuration)
	{
		return;
	}

	bRotating = false;

	// 블록과 함께 호를 그리며 돌리지 않고 끝에서 옮긴다: 폰은 셀 중심 사이를 구르는 공이라,
	// 호를 따라 보간하면 서 있다고 기록된 셀과 그려지는 위치가 어긋난다.
	if (PendingPawnCell.IsSet())
	{
		if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
		{
			if (AGridPawn* Pawn = Subsystem->GetGridPawn())
			{
				Pawn->TeleportToCell(PendingPawnCell.GetValue());
			}
		}
		PendingPawnCell.Reset();
	}
}

// ---------------------------------------------------------------------------- 에디터

#if WITH_EDITOR

void APuzzleRotationTile::PostEditMove(bool bFinished)
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

	const FIntPoint Size(SizeInCells, SizeInCells);
	const FIntPoint SnappedMin = GridFootprint::MinCellFromCentre(*FoundGrid, GetActorLocation(), Size);
	if (!FoundGrid->IsValidCell(SnappedMin))
	{
		return;
	}

	Modify();
	SetActorRotation(FRotator::ZeroRotator);
	SetActorLocation(GridFootprint::CentreFromMinCell(
		*FoundGrid, SnappedMin, Size, FoundGrid->CellToWorld(SnappedMin).Z));
}

void APuzzleRotationTile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshVisual();
	PostEditMove(true);
}

#endif
