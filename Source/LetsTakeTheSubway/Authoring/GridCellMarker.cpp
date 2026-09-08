// Copyright Epic Games, Inc. All Rights Reserved.

#include "Authoring/GridCellMarker.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	FColor MarkerColorForType(EGridCellType Type)
	{
		switch (Type)
		{
		case EGridCellType::Blocked:		return FColor(220, 50, 50);
		case EGridCellType::StageClear:		return FColor(40, 200, 220);
		case EGridCellType::Conditional:	return FColor(235, 200, 40);
		case EGridCellType::Walkable:		return FColor(60, 200, 90);
		default:							return FColor::White;
		}
	}
}

AGridCellMarkerBase::AGridCellMarkerBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bIsEditorOnlyActor = true;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetGenerateOverlapEvents(false);
	Box->SetBoxExtent(FVector(50.0, 50.0, 10.0));
	Box->ShapeColor = MarkerColorForType(CellType);
	Box->bHiddenInGame = true;
}

void AGridCellMarkerBase::BeginPlay()
{
	Super::BeginPlay();

	// bIsEditorOnlyActor가 쿡된 빌드에서는 이들을 걸러 주지만, PIE는 에디터 레벨을 그대로
	// 복제한다. 여기서 파괴해 두면 "마커는 런타임에 절대 영향을 주지 않는다"가 관례가
	// 아니라 보장이 된다.
	if (UWorld* World = GetWorld())
	{
		if (World->IsGameWorld())
		{
			Destroy();
		}
	}
}

#if WITH_EDITOR

void AGridCellMarkerBase::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (!bFinished)
	{
		return;		// 드래그 도중 스냅하면 기즈모와 충돌한다
	}

	if (const AGridActor* Grid = AGridActor::FindGrid(GetWorld()))
	{
		SnapToGrid(*Grid);
	}

	NotifyGrid();
}

void AGridCellMarkerBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateVisual();
	NotifyGrid();
}

void AGridCellMarkerBase::PostEditUndo()
{
	Super::PostEditUndo();

	UpdateVisual();
	NotifyGrid();
}

void AGridCellMarkerBase::Destroyed()
{
	NotifyGrid();
	Super::Destroyed();
}

void AGridCellMarkerBase::SnapToGrid(const AGridActor& Grid)
{
	const FIntPoint Cell = Grid.WorldToCell(GetActorLocation());
	if (!Grid.IsValidCell(Cell))
	{
		return;		// 그리드 밖으로 드래그됨; 다시 끌어올 수 있도록 그 자리에 둔다
	}

	const FVector Target = Grid.CellToWorld(Cell) + FVector(0.0, 0.0, 10.0);

	Modify();
	SetActorLocation(Target);
}

void AGridCellMarkerBase::NotifyGrid()
{
	UWorld* World = GetWorld();

	// Destroyed()는 레벨이 내려가거나 가비지 컬렉션 중에도 호출된다. 그때 구우면
	// 반쯤 파괴된 액터를 순회하게 되므로, 살아 있는 에디터 월드에서만 반응한다.
	if (!World || World->WorldType != EWorldType::Editor || World->bIsTearingDown || IsGarbageCollecting())
	{
		return;
	}

	for (TActorIterator<AGridActor> It(World); It; ++It)
	{
		It->OnMarkerChanged();
	}
}

void AGridCellMarkerBase::UpdateVisual()
{
	if (Box)
	{
		Box->ShapeColor = MarkerColorForType(CellType);
		Box->MarkRenderStateDirty();
	}
}

#endif	// WITH_EDITOR

// ---------------------------------------------------------------------------- 단일 셀

AGridCellMarker::AGridCellMarker()
{
	if (Box)
	{
		Box->SetBoxExtent(FVector(50.0, 50.0, 10.0));
	}
}

void AGridCellMarker::GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const
{
	const FIntPoint Cell = Grid.WorldToCell(GetActorLocation());
	if (Grid.IsValidCell(Cell))
	{
		OutCells.Add(Cell);
	}
}

// ---------------------------------------------------------------------------- 셀 박스

AGridBoxMarker::AGridBoxMarker()
{
	if (Box)
	{
		Box->SetBoxExtent(FVector(SizeInCells.X * 50.0, SizeInCells.Y * 50.0, 10.0));
	}
}

FIntPoint AGridBoxMarker::GetMinCell(const AGridActor& Grid) const
{
	// 액터는 덮는 직사각형의 중심에 있으므로, 절반만큼 되돌아가면 최소 모서리가 나온다.
	// +0.25 셀만큼 밀어 두는 건 중심이 정확히 경계 위에 있을 때 floor()가 한 셀 낮게
	// 떨어지는 걸 막기 위해서다.
	const FVector HalfSpan(
		SizeInCells.X * Grid.CellSize * 0.5,
		SizeInCells.Y * Grid.CellSize * 0.5,
		0.0);

	const FVector MinCorner = GetActorLocation() - HalfSpan + FVector(Grid.CellSize * 0.25, Grid.CellSize * 0.25, 0.0);
	return Grid.WorldToCell(MinCorner);
}

void AGridBoxMarker::GatherCells(const AGridActor& Grid, TArray<FIntPoint>& OutCells) const
{
	const FIntPoint MinCell = GetMinCell(Grid);

	for (int32 OffsetY = 0; OffsetY < SizeInCells.Y; ++OffsetY)
	{
		for (int32 OffsetX = 0; OffsetX < SizeInCells.X; ++OffsetX)
		{
			const FIntPoint Cell(MinCell.X + OffsetX, MinCell.Y + OffsetY);
			if (Grid.IsValidCell(Cell))
			{
				OutCells.Add(Cell);
			}
		}
	}
}

#if WITH_EDITOR

void AGridBoxMarker::SnapToGrid(const AGridActor& Grid)
{
	// 직사각형의 가장자리가 셀 경계에 오도록 스냅한다: 셀 수가 짝수면 중심이 경계 위에,
	// 홀수면 셀 중심 위에 온다.
	const FIntPoint MinCell = GetMinCell(Grid);
	if (!Grid.IsValidCell(MinCell))
	{
		return;
	}

	const FVector Origin = Grid.GetGridOrigin();
	const FVector Target(
		Origin.X + (MinCell.X + SizeInCells.X * 0.5) * Grid.CellSize,
		Origin.Y + (MinCell.Y + SizeInCells.Y * 0.5) * Grid.CellSize,
		Grid.CellToWorld(MinCell).Z + 10.0);

	Modify();
	SetActorLocation(Target);
}

void AGridBoxMarker::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Box)
	{
		Box->SetBoxExtent(FVector(SizeInCells.X * 50.0, SizeInCells.Y * 50.0, 10.0));
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif	// WITH_EDITOR
