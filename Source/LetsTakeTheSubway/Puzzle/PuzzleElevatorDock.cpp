// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleElevatorDock.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleElevatorBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APuzzleElevatorDock::APuzzleElevatorDock()
{
	// 엘리베이터가 4x4이므로 구조물도 4x4다. 더 크면 차체가 어디에 서 있든 도킹으로
	// 인정되어 "정확한 자리에 가져다 놓는다"는 퍼즐이 사라진다.
	SizeInCells = 4;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IdleFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_B2.MI_GreyBox_B2"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ReadyFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	if (IdleFinder.Succeeded())
	{
		IdleMaterial = IdleFinder.Object;
		if (PadMesh)
		{
			PadMesh->SetMaterial(0, IdleFinder.Object);
		}
	}
	if (ReadyFinder.Succeeded())
	{
		ReadyMaterial = ReadyFinder.Object;
	}
}

FString APuzzleElevatorDock::DescribeTile() const
{
	return FString::Printf(
		TEXT("Elevator dock, exit %s, target cell (%d,%d)."),
		*StaticEnum<EGridDirection>()->GetNameStringByValue(static_cast<int64>(ExitDirection)),
		TargetFloorCell.X, TargetFloorCell.Y);
}

void APuzzleElevatorDock::OnTileReady()
{
	const AGridActor* CurrentGrid = GetGrid();
	if (!CurrentGrid)
	{
		return;
	}

	if (!CurrentGrid->IsValidCell(TargetFloorCell))
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: no target floor cell; falling back to a fixed travel of %.0f cm %s."),
			*GetName(), TravelHeight, bTravelUp ? TEXT("up") : TEXT("down"));
		return;
	}

	if (!CurrentGrid->IsCellWalkableStatic(TargetFloorCell))
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: target floor cell (%d,%d) is not walkable; the rider will look for a cell nearby."),
			*GetName(), TargetFloorCell.X, TargetFloorCell.Y);
	}
}

// ---------------------------------------------------------------------------- 도킹

APuzzleElevatorBlock* APuzzleElevatorDock::FindElevatorOnTop() const
{
	const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
	if (!Subsystem || IsDisabled())
	{
		return nullptr;
	}

	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Subsystem->GetBlocks())
	{
		APuzzleElevatorBlock* Elevator = Cast<APuzzleElevatorBlock>(Entry.Get());
		if (Elevator && FullyContains(*Elevator))
		{
			return Elevator;
		}
	}

	return nullptr;
}

void APuzzleElevatorDock::RefreshDockedLook()
{
	const bool bReady = DockedElevator.IsValid();
	if (bReady == bLookIsReady || !PadMesh)
	{
		return;
	}

	bLookIsReady = bReady;

	// 패드 색이 바뀌는 것만으로 "여기 올려놓으면 된다"와 "이제 탈 수 있다"가 구분된다.
	// 그레이박스 단계에서 새 머티리얼을 만들지 않고 층 색을 빌려 쓴다.
	if (UMaterialInterface* Material = bReady ? ReadyMaterial : IdleMaterial)
	{
		PadMesh->SetMaterial(0, Material);
	}
}

void APuzzleElevatorDock::OnBlockCameToRest(APuzzleBlock& Block)
{
	// 구조물의 반응은 도킹이다. 엘리베이터가 아니면 아무 일도 하지 않는다 -- 일반 블록이
	// 구조물 위에 서 있을 수는 있고, 그건 그저 길을 막고 있는 것이다.
	if (APuzzleElevatorBlock* Elevator = Cast<APuzzleElevatorBlock>(&Block))
	{
		DockedElevator = Elevator;
		RefreshDockedLook();

		UE_LOG(LogLTTSGrid, Display,
			TEXT("%s: %s docked; ready to travel."), *GetName(), *Elevator->GetName());

		if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
		{
			Subsystem->ShowFeedback(
				TEXT("The elevator is in place. Step up to a door and click it."),
				FLinearColor(0.45f, 0.85f, 1.0f));
		}
	}
}

void APuzzleElevatorDock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 도킹은 매 틱 다시 판정한다. 차체가 밀려 나가거나 회전으로 실려 나가는 경로가 여럿이라,
	// 나갈 때마다 알림을 거는 것보다 지금 상태를 그때그때 보는 편이 틀릴 여지가 없다.
	APuzzleElevatorBlock* OnTop = FindElevatorOnTop();
	if (OnTop != DockedElevator.Get())
	{
		DockedElevator = OnTop;
		RefreshDockedLook();
	}
}

// ---------------------------------------------------------------------------- 승강

double APuzzleElevatorDock::GetFarFloorZ() const
{
	const AGridActor* CurrentGrid = GetGrid();

	if (CurrentGrid && CurrentGrid->IsValidCell(TargetFloorCell))
	{
		if (const FGridCellData* Data = CurrentGrid->GetCell(TargetFloorCell))
		{
			// 목표 높이가 실제 바닥에서 나온다. 올라가든 내려가든 층 바닥에 정확히 닿는다.
			return Data->FloorZ;
		}
	}

	return GetPivotWorld().Z + (bTravelUp ? TravelHeight : -TravelHeight);
}

double APuzzleElevatorDock::ComputeTargetZ(const APuzzleElevatorBlock& Elevator) const
{
	const double NearZ = GetPivotWorld().Z;		// 구조물이 놓인 층
	const double FarZ = GetFarFloorZ();			// 반대편 층

	// 차체가 이미 반대편 층에 있으면 구조물 층으로 되돌아간다. 구조물 하나로 왕복이 되어야
	// 플레이어가 층을 잘못 골랐을 때 갇히지 않는다.
	return FMath::IsNearlyEqual(Elevator.GetFloorZ(), FarZ, 10.0) ? NearZ : FarZ;
}

bool APuzzleElevatorDock::FindExitWorld(const APuzzleElevatorBlock& Elevator, double TargetZ, FVector& OutWorld) const
{
	const AGridActor* CurrentGrid = GetGrid();
	if (!CurrentGrid)
	{
		return false;
	}

	// 목적 층 셀이 그 높이에 있으면 그것이 곧 내릴 자리다.
	if (CurrentGrid->IsValidCell(TargetFloorCell))
	{
		if (const FGridCellData* Data = CurrentGrid->GetCell(TargetFloorCell))
		{
			if (FMath::IsNearlyEqual(Data->FloorZ, TargetZ, 10.0))
			{
				OutWorld = CurrentGrid->CellToWorld(TargetFloorCell);
				return true;
			}
		}
	}

	// 아니면 문 앞 셀 중에서 그 높이의 바닥을 가진 것을 고른다. 내려올 때가 이 경우다:
	// 목적 층 셀은 위층 것이므로 쓸 수 없고, 구조물 층의 문 앞이 내릴 자리다.
	TArray<FIntPoint> FrontCells;
	Elevator.GetDoorFrontCells(FrontCells);

	for (const FIntPoint& Cell : FrontCells)
	{
		const FGridCellData* Data = CurrentGrid->GetCell(Cell);
		if (Data && CurrentGrid->IsCellWalkableStatic(Cell)
			&& FMath::IsNearlyEqual(Data->FloorZ, TargetZ, 10.0))
		{
			OutWorld = CurrentGrid->CellToWorld(Cell);
			return true;
		}
	}

	// 마지막 수단: 출구 방향으로 한 칸 바깥의 목표 높이 지점. 그리드가 근처에서 걸을 수
	// 있는 셀을 찾아 준다.
	const FIntPoint Step = LTTSGrid::DirOffset(ExitDirection);
	OutWorld = FVector(
		GetPivotWorld().X + Step.X * (SizeInCells * 0.5 + 0.5) * 100.0,
		GetPivotWorld().Y + Step.Y * (SizeInCells * 0.5 + 0.5) * 100.0,
		TargetZ);
	return true;
}

bool APuzzleElevatorDock::TryLaunch(AGridPawn* Pawn, FText* OutReason)
{
	APuzzleElevatorBlock* Elevator = DockedElevator.Get();

	if (!Elevator || !FullyContains(*Elevator))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockEmpty", "Push the elevator onto the dock first.");
		}
		return false;
	}

	if (!Pawn || !Pawn->IsOnGrid() || Pawn->IsMoving())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockPawnBusy", "Wait until you have stopped walking.");
		}
		return false;
	}

	const double TargetZ = ComputeTargetZ(*Elevator);
	if (FMath::IsNearlyEqual(TargetZ, Elevator->GetFloorZ(), 1.0))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockSameFloor", "The elevator is already on that floor.");
		}
		return false;
	}

	// 출구는 도착 층에서 폰이 내릴 자리다. 올라갈 때와 내려올 때가 서로 다른 셀이므로
	// 목표 높이를 기준으로 고른다.
	FVector ExitWorld;
	if (!FindExitWorld(*Elevator, TargetZ, ExitWorld))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockNoExit", "There is nowhere to step off up there.");
		}
		return false;
	}

	if (!Elevator->StartVerticalTravel(TargetZ, ExitWorld, TravelSpeed, DoorDwellSeconds, Pawn))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockBusy", "The elevator is busy.");
		}
		return false;
	}

	return true;
}

bool APuzzleElevatorDock::IsBusy() const
{
	const APuzzleElevatorBlock* Elevator = DockedElevator.Get();
	return Elevator && Elevator->IsTravelling();
}

// ---------------------------------------------------------------------------- 에디터

void APuzzleElevatorDock::DetectTargetFloor()
{
#if WITH_EDITOR
	const AGridActor* CurrentGrid = AGridActor::FindGrid(GetWorld());
	if (!CurrentGrid)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: no AGridActor in the level; cannot detect a target floor."), *GetName());
		return;
	}

	const FIntPoint Size(SizeInCells, SizeInCells);
	const FIntPoint MinCell = GridFootprint::MinCellFromCentre(*CurrentGrid, GetActorLocation(), Size);
	const FIntPoint Step = LTTSGrid::DirOffset(ExitDirection);

	// 구조물 한 칸 바깥의 셀. 위층 바닥은 여기에 깔려 있어야 폰이 내릴 수 있다.
	const FIntPoint Candidate(
		MinCell.X + (Step.X > 0 ? SizeInCells : (Step.X < 0 ? -1 : SizeInCells / 2)),
		MinCell.Y + (Step.Y > 0 ? SizeInCells : (Step.Y < 0 ? -1 : SizeInCells / 2)));

	if (!CurrentGrid->IsValidCell(Candidate))
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: cell (%d,%d) beyond the exit is outside the grid."), *GetName(), Candidate.X, Candidate.Y);
		return;
	}

	const FGridCellData* Data = CurrentGrid->GetCell(Candidate);
	if (!Data || Data->Type == EGridCellType::NoFloor)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: cell (%d,%d) beyond the exit has no floor. Point ExitDirection at the upper landing."),
			*GetName(), Candidate.X, Candidate.Y);
		return;
	}

	Modify();
	TargetFloorCell = Candidate;

	const double Here = CurrentGrid->CellToWorld(MinCell).Z;
	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: target floor cell (%d,%d) at Z %.0f; travel from Z %.0f is %.0f cm."),
		*GetName(), Candidate.X, Candidate.Y, Data->FloorZ, Here, Data->FloorZ - Here);
#endif
}
