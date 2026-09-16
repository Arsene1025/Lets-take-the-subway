// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Guide/GuideCellTrigger.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"
#include "UI/Guide/GuideDataSubsystem.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"

AGuideCellTrigger::AGuideCellTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);
	Box->SetHiddenInGame(true);
	Box->ShapeColor = FColor(255, 200, 60);
	RootComponent = Box;
}

void AGuideCellTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (Guide == EGuideType::None)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: Guide is None; the guide trigger does nothing."), *GetName());
		return;
	}

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: this level has no AGridActor; the guide trigger does nothing."), *GetName());
		return;
	}

	// 박스의 XY 범위에 중심이 들어오는 셀을 모은다(ASoundCellTrigger와 같은 규칙).
	const FBox Bounds = Box->Bounds.GetBox();
	const FIntPoint MinCell = Grid->WorldToCell(FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z));
	const FIntPoint MaxCell = Grid->WorldToCell(FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Min.Z));
	const FVector Origin = Grid->GetGridOrigin();

	Cells.Reset();
	for (int32 X = FMath::Min(MinCell.X, MaxCell.X); X <= FMath::Max(MinCell.X, MaxCell.X); ++X)
	{
		for (int32 Y = FMath::Min(MinCell.Y, MaxCell.Y); Y <= FMath::Max(MinCell.Y, MaxCell.Y); ++Y)
		{
			const FIntPoint Cell(X, Y);
			if (!Grid->IsValidCell(Cell))
			{
				continue;
			}

			const double CentreX = Origin.X + (X + 0.5) * Grid->CellSize;
			const double CentreY = Origin.Y + (Y + 0.5) * Grid->CellSize;
			if (CentreX >= Bounds.Min.X && CentreX <= Bounds.Max.X && CentreY >= Bounds.Min.Y && CentreY <= Bounds.Max.Y)
			{
				Cells.Add(Cell);
			}
		}
	}

	if (Cells.IsEmpty())
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: the box covers no cell centre; make it at least one cell (%.0f cm) wide."),
			*GetName(), Grid->CellSize);
		return;
	}

	Grid->OnPawnEnteredCell.AddDynamic(this, &AGuideCellTrigger::HandlePawnEnteredCell);

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: guide %s on %d cell(s)."),
		*GetName(), *UEnum::GetValueAsString(Guide), Cells.Num());
}

void AGuideCellTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Unbind();

	Super::EndPlay(EndPlayReason);
}

void AGuideCellTrigger::Unbind()
{
	if (Grid)
	{
		Grid->OnPawnEnteredCell.RemoveDynamic(this, &AGuideCellTrigger::HandlePawnEnteredCell);
	}
}

void AGuideCellTrigger::HandlePawnEnteredCell(APawn* Pawn, FIntPoint Cell)
{
	// 행인도 셀 진입을 방송하므로 플레이어 폰만 받는다.
	if (!Cells.Contains(Cell) || !Cast<AGridPawn>(Pawn))
	{
		return;
	}

	UGuideDataSubsystem* GuideSubsystem = UGuideDataSubsystem::Get(this);
	if (!GuideSubsystem)
	{
		return;
	}

	// 가이드는 한 번만 뜬다. 이미 봤거나 방금 띄웠으면 더 들을 이유가 없다.
	GuideSubsystem->RequestGuide(Guide);
	Unbind();
}
