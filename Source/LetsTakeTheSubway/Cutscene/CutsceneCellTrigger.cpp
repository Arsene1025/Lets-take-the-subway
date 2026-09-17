// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cutscene/CutsceneCellTrigger.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"
#include "UI/UIManagerSubsystem.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

ACutsceneCellTrigger::ACutsceneCellTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);
	Box->SetHiddenInGame(true);
	Box->ShapeColor = FColor(255, 80, 200);
	RootComponent = Box;
}

void ACutsceneCellTrigger::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: this level has no AGridActor; the cutscene trigger does nothing."), *GetName());
		return;
	}

	// 박스의 XY 범위에 중심이 들어오는 셀을 모은다(AGuideCellTrigger와 같은 규칙).
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

	Grid->OnPawnEnteredCell.AddDynamic(this, &ACutsceneCellTrigger::HandlePawnEnteredCell);

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: cutscene %s on %d cell(s), then %s."),
		*GetName(), *UEnum::GetValueAsString(Kind), Cells.Num(),
		NextLevel.IsNull() ? TEXT("no level change") : *NextLevel.ToSoftObjectPath().ToString());
}

void ACutsceneCellTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Unbind();
	GetWorldTimerManager().ClearTimer(DelayTimer);

	Super::EndPlay(EndPlayReason);
}

void ACutsceneCellTrigger::Unbind()
{
	if (Grid)
	{
		Grid->OnPawnEnteredCell.RemoveDynamic(this, &ACutsceneCellTrigger::HandlePawnEnteredCell);
	}
}

void ACutsceneCellTrigger::HandlePawnEnteredCell(APawn* Pawn, FIntPoint Cell)
{
	// 행인도 셀 진입을 방송하므로 플레이어 폰만 받는다.
	if (!Cells.Contains(Cell) || !Cast<AGridPawn>(Pawn))
	{
		return;
	}

	if (bFired && bOnce)
	{
		return;
	}
	bFired = true;

	if (bOnce)
	{
		Unbind();
	}

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: player entered cell (%d,%d); cutscene in %.1f s."),
		*GetName(), Cell.X, Cell.Y, Delay);

	if (Delay <= 0.0f)
	{
		Fire();
	}
	else
	{
		GetWorldTimerManager().SetTimer(DelayTimer, this, &ACutsceneCellTrigger::Fire, Delay, false);
	}
}

void ACutsceneCellTrigger::Fire()
{
	UUIManagerSubsystem* UI = UUIManagerSubsystem::Get(this);
	if (!UI)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: no UI manager; cutscene skipped."), *GetName());
		return;
	}

	UI->PlayCutscene(Kind, NextLevel);
}
