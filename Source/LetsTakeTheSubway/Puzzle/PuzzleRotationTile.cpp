// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleRotationTile.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APuzzleRotationTile::APuzzleRotationTile()
{
	CornerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CornerMesh"));
	CornerMesh->SetupAttachment(SceneRoot);
	CornerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CornerMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CornerMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CornerMesh->SetStaticMesh(CubeFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	if (MaterialFinder.Succeeded())
	{
		CornerMesh->SetMaterial(0, MaterialFinder.Object);
	}
}

// ------------------------------------------------------------------------ 생명주기

void APuzzleRotationTile::PostLoad()
{
	Super::PostLoad();

	// 예전에는 방향이 bool 하나였다. 그 시절에 반시계로 배치해 둔 타일이 조용히 시계로 바뀌면
	// 푸는 방법이 통째로 바뀌므로, 저장된 값을 새 설정으로 옮긴다.
	if (!bClockwise_DEPRECATED)
	{
		Direction = EPuzzleRotationDirection::CounterClockwise;
		bClockwise_DEPRECATED = true;
	}
}

void APuzzleRotationTile::OnTileReady()
{
	Super::OnTileReady();

	// 교대 순서는 한 플레이 안에서만 의미가 있다. 재시작하면 다시 시계부터다.
	NextFreeTurnSign = 1;
}

// ------------------------------------------------------------------------ 방향

int32 APuzzleRotationTile::ResolveTurnSign() const
{
	return LTTSPuzzle::ResolveTurnSign(Direction, NextFreeTurnSign);
}

void APuzzleRotationTile::RefreshVisual()
{
	Super::RefreshVisual();

	if (CornerMesh)
	{
		const double CellSize = GetGrid() ? GetGrid()->CellSize : 100.0;
		const double Inset = (SizeInCells - 1) * CellSize * 0.5;
		CornerMesh->SetRelativeLocation(FVector(Inset, Inset, 14.0));
		CornerMesh->SetRelativeScale3D(FVector(CellSize / 200.0, CellSize / 200.0, 0.12));
	}
}

FString APuzzleRotationTile::DescribeTile() const
{
	if (Direction == EPuzzleRotationDirection::Free)
	{
		return FString::Printf(TEXT("Rotation tile, alternating (next: %s)."),
			LTTSPuzzle::DescribeTurnSign(NextFreeTurnSign));
	}

	return FString::Printf(TEXT("Rotation tile, %s."),
		LTTSPuzzle::DescribeTurnSign(LTTSPuzzle::ResolveTurnSign(Direction, 1)));
}

void APuzzleRotationTile::OnBlockCameToRest(APuzzleBlock& Block)
{
	// 회전판의 반응은 공간을 돌리는 것이다. 돌 수 없는 이유는 플레이어에게 보여 준다.
	FText Reason;
	if (!TryRotate(&Reason))
	{
		if (!Reason.IsEmpty())
		{
			if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
			{
				Subsystem->ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
			}
		}
	}
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

	const int32 TurnSign = ResolveTurnSign();

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

		// 타일 위에 올라와 있으면서 돌지 않는 조각은 회전 전체를 막는다. 그것만 빼고 돌리면
		// 남은 조각이 그 자리로 들어와 겹친다.
		if (!Block->bCanRotate)
		{
			if (OutReason)
			{
				*OutReason = FText::Format(
					NSLOCTEXT("LTTSPuzzle", "TileBlockFixed", "Cannot rotate: {0} does not turn."),
					FText::FromString(Block->GetName()));
			}
			return false;
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

	// 거부된 회전은 순서를 소모하지 않는다: 검사 패스가 돌려보낸 뒤에는 여기까지 오지 못한다.
	// 그래야 바닥이 모자라 한 번 막혔다고 해서 다음 방향이 뒤집히지 않는다.
	if (Direction == EPuzzleRotationDirection::Free)
	{
		NextFreeTurnSign = -TurnSign;
	}

	Subsystem->ShowFeedback(
		FString::Printf(TEXT("The space turns %s."), LTTSPuzzle::DescribeTurnSign(TurnSign)),
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
