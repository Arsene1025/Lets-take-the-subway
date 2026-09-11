// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleSubsystem.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"
#include "Player/GridPlayerController.h"
#include "Puzzle/PuzzleBlock.h"
#include "Puzzle/PuzzleLever.h"
#include "Puzzle/PuzzleElevatorBlock.h"
#include "Puzzle/PuzzleElevatorDock.h"
#include "Puzzle/PuzzleFloorTile.h"
#include "Puzzle/PuzzleRegion.h"
#include "Puzzle/PuzzleRotatingObstacle.h"
#include "Puzzle/PuzzleRotationTile.h"
#include "Vehicle/GridTrain.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UPuzzleSubsystem* UPuzzleSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UPuzzleSubsystem>() : nullptr;
}

bool UPuzzleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 에디터 월드에는 게임플레이가 없고, 에디터에서 보이는 블록의 미리보기는 전부 블록
	// 자신의 컨스트럭션 스크립트가 담당한다.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ---------------------------------------------------------------------------- 등록부

void UPuzzleSubsystem::RegisterBlock(APuzzleBlock* Block)
{
	if (Block)
	{
		Blocks.AddUnique(Block);
	}
}

void UPuzzleSubsystem::UnregisterBlock(APuzzleBlock* Block)
{
	Blocks.RemoveAll([Block](const TWeakObjectPtr<APuzzleBlock>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Block;
	});
}

void UPuzzleSubsystem::RegisterTile(APuzzleFloorTile* Tile)
{
	if (Tile)
	{
		Tiles.AddUnique(Tile);
	}
}

void UPuzzleSubsystem::UnregisterTile(APuzzleFloorTile* Tile)
{
	Tiles.RemoveAll([Tile](const TWeakObjectPtr<APuzzleFloorTile>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Tile;
	});
}

void UPuzzleSubsystem::RegisterObstacle(APuzzleRotatingObstacle* Obstacle)
{
	if (Obstacle)
	{
		Obstacles.AddUnique(Obstacle);
	}
}

void UPuzzleSubsystem::UnregisterObstacle(APuzzleRotatingObstacle* Obstacle)
{
	Obstacles.RemoveAll([Obstacle](const TWeakObjectPtr<APuzzleRotatingObstacle>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Obstacle;
	});
}

void UPuzzleSubsystem::RegisterLever(APuzzleLever* Lever)
{
	if (Lever)
	{
		Levers.AddUnique(Lever);
	}
}

void UPuzzleSubsystem::UnregisterLever(APuzzleLever* Lever)
{
	Levers.RemoveAll([Lever](const TWeakObjectPtr<APuzzleLever>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Lever;
	});
}

void UPuzzleSubsystem::RegisterRegion(APuzzleRegion* Region)
{
	if (Region)
	{
		Regions.AddUnique(Region);
	}
}

void UPuzzleSubsystem::UnregisterRegion(APuzzleRegion* Region)
{
	Regions.RemoveAll([Region](const TWeakObjectPtr<APuzzleRegion>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Region;
	});
}

void UPuzzleSubsystem::RegisterVehicle(AGridTrain* Vehicle)
{
	if (Vehicle)
	{
		Vehicles.AddUnique(Vehicle);
	}
}

void UPuzzleSubsystem::UnregisterVehicle(AGridTrain* Vehicle)
{
	Vehicles.RemoveAll([Vehicle](const TWeakObjectPtr<AGridTrain>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Vehicle;
	});
}

// ---------------------------------------------------------------------------- 조회

APuzzleRegion* UPuzzleSubsystem::FindRegionContaining(const FGridRect& Rect) const
{
	for (const TWeakObjectPtr<APuzzleRegion>& Entry : Regions)
	{
		APuzzleRegion* Region = Entry.Get();
		if (Region && Region->ContainsRect(Rect))
		{
			return Region;
		}
	}

	return nullptr;
}

bool UPuzzleSubsystem::IsDockCell(FIntPoint Cell) const
{
	for (const TWeakObjectPtr<APuzzleFloorTile>& Entry : Tiles)
	{
		const APuzzleElevatorDock* Dock = Cast<APuzzleElevatorDock>(Entry.Get());
		if (Dock && !Dock->IsDisabled() && Dock->GetRegion().Contains(Cell))
		{
			return true;
		}
	}

	return false;
}

APuzzleElevatorDock* UPuzzleSubsystem::FindDockUnder(const APuzzleElevatorBlock& Elevator) const
{
	for (const TWeakObjectPtr<APuzzleFloorTile>& Entry : Tiles)
	{
		APuzzleElevatorDock* Dock = Cast<APuzzleElevatorDock>(Entry.Get());
		if (Dock && Dock->FullyContains(Elevator))
		{
			return Dock;
		}
	}

	return nullptr;
}

bool UPuzzleSubsystem::IsInputLocked() const
{
	// 폰이 타고 내리는 동안은 조작을 받지 않는다. 그동안 플레이어는 조각들이 어디로 가는지
	// 볼 수 없고, 폰 자신도 셀 위에 있지 않다.
	if (const AGridPawn* Pawn = GetGridPawn())
	{
		if (!Pawn->IsOnGrid())
		{
			return true;
		}
	}

	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Blocks)
	{
		if (const APuzzleBlock* Block = Entry.Get())
		{
			if (Block->IsAnimating())
			{
				return true;
			}
		}
	}

	for (const TWeakObjectPtr<APuzzleFloorTile>& Entry : Tiles)
	{
		if (const APuzzleFloorTile* Tile = Entry.Get())
		{
			if (Tile->IsBusy())
			{
				return true;
			}
		}
	}

	for (const TWeakObjectPtr<APuzzleRotatingObstacle>& Entry : Obstacles)
	{
		if (const APuzzleRotatingObstacle* Obstacle = Entry.Get())
		{
			if (Obstacle->IsAnimating())
			{
				return true;
			}
		}
	}

	return false;
}

APuzzleBlock* UPuzzleSubsystem::FindBlockAtCell(const AGridActor& Grid, FIntPoint Cell) const
{
	return Cast<APuzzleBlock>(Grid.GetOccupant(Cell));
}

void UPuzzleSubsystem::GetBlockActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reserve(OutActors.Num() + Blocks.Num() + Obstacles.Num() + Levers.Num() + Vehicles.Num());

	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Blocks)
	{
		if (APuzzleBlock* Block = Entry.Get())
		{
			OutActors.Add(Block);
		}
	}

	for (const TWeakObjectPtr<APuzzleRotatingObstacle>& Entry : Obstacles)
	{
		if (APuzzleRotatingObstacle* Obstacle = Entry.Get())
		{
			OutActors.Add(Obstacle);
		}
	}

	for (const TWeakObjectPtr<APuzzleLever>& Entry : Levers)
	{
		if (APuzzleLever* Lever = Entry.Get())
		{
			OutActors.Add(Lever);
		}
	}

	for (const TWeakObjectPtr<AGridTrain>& Entry : Vehicles)
	{
		if (AGridTrain* Vehicle = Entry.Get())
		{
			OutActors.Add(Vehicle);
		}
	}
}

// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
void UPuzzleSubsystem::UpdateOcclusion(const FVector& CameraLocation, const APawn* Pawn, float SweepRadius)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TSet<const APuzzleBlock*> Blocking;

	if (Pawn)
	{
		// 콜리전을 트레이스하는 대신 각 블록의 지정된 부피와 검사한다. 물리 스윕은
		// 납작해진 메시를 읽으므로, 비켜 준 블록이 곧바로 가림을 멈추고, 일어서고,
		// 다시 가리기를 매 프레임 반복하게 된다.
		const FVector PawnLocation = Pawn->GetActorLocation();
		const FVector Extent(FMath::Max(SweepRadius, 1.0f));

		for (const TWeakObjectPtr<APuzzleBlock>& Entry : Blocks)
		{
			const APuzzleBlock* Block = Entry.Get();
			if (!Block)
			{
				continue;
			}

			const FBox Bounds = Block->GetFullBounds();
			if (!Bounds.IsValid)
			{
				continue;
			}

			FVector HitLocation;
			FVector HitNormal;
			float HitTime = 0.0f;
			if (FMath::LineExtentBoxIntersection(Bounds, CameraLocation, PawnLocation, Extent, HitLocation, HitNormal, HitTime))
			{
				Blocking.Add(Block);
			}
		}
	}

	NumOccludingBlocks = Blocking.Num();

	// 모든 블록에 어느 쪽이든 알려 주므로, 더 이상 가리지 않는 블록은 다시 일어선다.
	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Blocks)
	{
		if (APuzzleBlock* Block = Entry.Get())
		{
			Block->SetCutaway(Blocking.Contains(Block));
		}
	}
}
#endif

AGridPawn* UPuzzleSubsystem::GetGridPawn() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	return Controller ? Cast<AGridPawn>(Controller->GetPawn()) : nullptr;
}

AGridPlayerController* UPuzzleSubsystem::GetGridController() const
{
	const UWorld* World = GetWorld();
	return World ? Cast<AGridPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

void UPuzzleSubsystem::GetPawnReservedCells(TArray<FIntPoint>& OutCells) const
{
	const AGridPawn* Pawn = GetGridPawn();
	if (!Pawn)
	{
		return;
	}

	// 탈것에 실려 있는 폰은 그리드 위에 없다. 그동안 마지막에 서 있던 셀을 계속 잡고 있으면
	// 엘리베이터가 떠난 뒤에도 그 자리가 영영 막힌다.
	if (!Pawn->IsOnGrid())
	{
		return;
	}

	OutCells.Add(Pawn->GetCurrentCell());

	if (const TOptional<FIntPoint> Next = Pawn->GetNextCell())
	{
		OutCells.Add(Next.GetValue());
	}
}

void UPuzzleSubsystem::ShowFeedback(const FString& Message, const FLinearColor& Color) const
{
	if (AGridPlayerController* Controller = GetGridController())
	{
		Controller->ShowFeedback(Message, Color);
	}
}

// ---------------------------------------------------------------------------- 규칙

void UPuzzleSubsystem::NotifyBlockCameToRest(APuzzleBlock* Block)
{
	if (!Block)
	{
		return;
	}

	for (const TWeakObjectPtr<APuzzleFloorTile>& Entry : Tiles)
	{
		APuzzleFloorTile* Tile = Entry.Get();
		if (!Tile || !Tile->FullyContains(*Block))
		{
			continue;
		}

		// 타일은 절대 겹치지 않으므로 블록을 담는 타일은 많아야 하나다. 첫 번째에서 멈춘다.
		// 무엇을 할지는 타일이 정한다: 회전판은 공간을 돌리고, 구조물은 엘리베이터를 받는다.
		Tile->OnBlockCameToRest(*Block);
		return;
	}
}
