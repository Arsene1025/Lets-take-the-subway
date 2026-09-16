// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundCellTrigger.h"

#include "Sound/GameSoundSubsystem.h"
#include "Grid/GridActor.h"
#include "NPC/GridNPC.h"
#include "Player/GridPawn.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"

ASoundCellTrigger::ASoundCellTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);
	Box->SetHiddenInGame(true);
	Box->ShapeColor = FColor(80, 200, 255);
	RootComponent = Box;
}

void ASoundCellTrigger::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSSound, Warning, TEXT("%s: this level has no AGridActor; the sound trigger does nothing."), *GetName());
		return;
	}

	// 박스의 XY 범위에 중심이 들어오는 셀을 모은다. 셀 가장자리에 걸친 박스가 이웃 셀까지 먹지 않게
	// 중심으로 판정한다.
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
		UE_LOG(LogLTTSSound, Warning,
			TEXT("%s: the box covers no cell centre; make it at least one cell (%.0f cm) wide."),
			*GetName(), Grid->CellSize);
		return;
	}

	Grid->OnPawnEnteredCell.AddDynamic(this, &ASoundCellTrigger::HandlePawnEnteredCell);

	UE_LOG(LogLTTSSound, Display, TEXT("%s: '%s' on %d cell(s)."), *GetName(), *SoundKey.ToString(), Cells.Num());
}

void ASoundCellTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Grid)
	{
		Grid->OnPawnEnteredCell.RemoveDynamic(this, &ASoundCellTrigger::HandlePawnEnteredCell);
	}

	Super::EndPlay(EndPlayReason);
}

void ASoundCellTrigger::HandlePawnEnteredCell(APawn* Pawn, FIntPoint Cell)
{
	if (!Cells.Contains(Cell))
	{
		return;
	}

	const bool bIsPlayer = Cast<AGridPawn>(Pawn) != nullptr;
	const bool bIsNPC = Cast<AGridNPC>(Pawn) != nullptr;
	if (!((bIsPlayer && bTriggerOnPlayer) || (bIsNPC && bTriggerOnNPC)))
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (MinRetriggerSeconds > 0.0f && LastTriggerTime >= 0.0 && Now - LastTriggerTime < MinRetriggerSeconds)
	{
		return;
	}

	UGameSoundSubsystem* Sound = UGameSoundSubsystem::Get(this);
	if (!Sound)
	{
		return;
	}

	const bool bPlayed = bSpatial
		? Sound->PlaySoundAtLocation(SoundKey, Pawn ? Pawn->GetActorLocation() : GetActorLocation()) != nullptr
		: Sound->PlaySound2D(SoundKey) != nullptr;

	if (bPlayed)
	{
		LastTriggerTime = Now;
	}
}
