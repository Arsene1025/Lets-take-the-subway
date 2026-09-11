// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleFloorTile.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "Puzzle/PuzzleBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APuzzleFloorTile::APuzzleFloorTile()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(SceneRoot);
	PadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PadMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PadMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		PadMesh->SetStaticMesh(CubeFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	if (MaterialFinder.Succeeded())
	{
		PadMesh->SetMaterial(0, MaterialFinder.Object);
	}

	// 패드는 바닥에 놓인 장식이다. 그리드가 이것을 지오메트리로 트레이스하면 안 된다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

	// 쿠커가 읽는 저작 데이터. 에디터 빌드 전용.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

// ---------------------------------------------------------------------------- 포함 판정

bool APuzzleFloorTile::FullyContains(const APuzzleBlock& Block) const
{
	return !bDisabled && Region.ContainsRect(Block.GetRect());
}

bool APuzzleFloorTile::Straddles(const APuzzleBlock& Block) const
{
	if (bDisabled)
	{
		return false;
	}

	const FGridRect BlockRect = Block.GetRect();
	return Region.Overlaps(BlockRect) && !Region.ContainsRect(BlockRect);
}

// ---------------------------------------------------------------------------- 층 높이

double APuzzleFloorTile::ResolveFloorZ(const AGridActor& InGrid, FIntPoint MinCell, double /*PlacedZ*/) const
{
	// 셀 하나에 바닥은 하나다. 그러므로 셀의 바닥 높이가 곧 이 타일이 놓인 층이다.
	return InGrid.CellToWorld(MinCell).Z;
}

// ---------------------------------------------------------------------------- 비주얼

void APuzzleFloorTile::RefreshVisual()
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
}

void APuzzleFloorTile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
}

// ---------------------------------------------------------------------------- 생명주기

void APuzzleFloorTile::BeginPlay()
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
				TEXT("%s: cell (%d,%d) is not walkable floor; pieces landing there will be refused."),
				*GetName(), Cell.X, Cell.Y);
		}
	}

	// 변의 길이가 짝수이면 영역의 중심이 셀 모서리에 떨어진다. 회전한 모든 풋프린트를
	// 재는 기준이 되는 회전축이 정확히 그 점이다.
	//
	// 높이는 파생 클래스가 정한다. 배치된 Z를 먼저 읽어 두는 이유는 그것이 셀에서 층을
	// 알아낼 수 없을 때의 유일한 근거이기 때문이다(샤프트 위의 위층 구조물).
	const double PlacedZ = GetActorLocation().Z;
	const FVector Origin = Grid->GetGridOrigin();
	PivotWorld = FVector(
		Origin.X + (Region.Min.X + SizeInCells * 0.5) * Grid->CellSize,
		Origin.Y + (Region.Min.Y + SizeInCells * 0.5) * Grid->CellSize,
		ResolveFloorZ(*Grid, Region.Min, PlacedZ));

	SetActorLocation(PivotWorld);
	RefreshVisual();

	for (TActorIterator<APuzzleFloorTile> It(GetWorld()); It; ++It)
	{
		const APuzzleFloorTile* Other = *It;
		if (Other && Other != this && !Other->bDisabled && Other->Region.Overlaps(Region)
			&& !CanCoexistWith(*Other))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: overlaps floor tile %s. Tiles must be disjoint."), *GetName(), *Other->GetName());
		}
	}

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->RegisterTile(this);
	}

	OnTileReady();

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %dx%d floor tile at cell (%d,%d). %s"),
		*GetName(), SizeInCells, SizeInCells, Region.Min.X, Region.Min.Y, *DescribeTile());
}

void APuzzleFloorTile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->UnregisterTile(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- 에디터

#if WITH_EDITOR

void APuzzleFloorTile::PostEditMove(bool bFinished)
{
	// 클래스 기본값(CDO)과 블루프린트 템플릿에는 월드도, 배치된 트랜스폼도 없다.
	// 블루프린트 에디터의 Class Defaults를 편집하는 것도 이 경로를 지나므로, 여기서
	// 막지 않으면 그리드를 찾아 스냅하려다 에디터가 죽는다.
	if (IsTemplate())
	{
		return;
	}

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
		*FoundGrid, SnappedMin, Size,
		ResolveFloorZ(*FoundGrid, SnappedMin, GetActorLocation().Z)));
}

void APuzzleFloorTile::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 클래스 기본값(CDO)과 블루프린트 템플릿에는 월드도, 배치된 트랜스폼도 없다.
	// 블루프린트 에디터의 Class Defaults를 편집하는 것도 이 경로를 지나므로, 여기서
	// 막지 않으면 그리드를 찾아 스냅하려다 에디터가 죽는다.
	if (IsTemplate())
	{
		return;
	}

	RefreshVisual();
	PostEditMove(true);
}

#endif
