// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleElevatorDock.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleElevatorBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
	// 짝의 층 높이는 여기서 읽지 않는다. 액터의 BeginPlay 순서는 정해져 있지 않아, 먼저 도는
	// 구조물이 아직 층을 정하지 못한 짝을 보면 Z 0을 찍는다. 로그가 검증 도구인 이상 틀린
	// 숫자를 남기느니 첫 틱까지 미루는 편이 낫다(LogPairingOnce).
	if (bIsClear)
	{
		return FString::Printf(
			TEXT("Elevator dock at Z %.0f, stage-clear exit: rides %.0f cm %s then the cutscene."),
			GetFloorZ(), ClearTravelHeight, bClearTravelUp ? TEXT("up") : TEXT("down"));
	}

	if (const APuzzleElevatorDock* Target = GetTargetDock())
	{
		return FString::Printf(
			TEXT("Elevator dock at Z %.0f, paired with %s."), GetFloorZ(), *Target->GetName());
	}

	return FString::Printf(
		TEXT("Elevator dock at Z %.0f (legacy), exit %s, target cell (%d,%d)."),
		GetFloorZ(),
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

	// 클리어 셀에 판정을 맡기기로 했다면 그리드의 사건을 직접 듣는다. 게임모드를 거치지 않는
	// 이유는 어느 구조물이 출구인지를 아는 쪽이 구조물 자신이기 때문이다.
	if (bClearOnStageClear)
	{
		// CurrentGrid는 const다. 구독은 그리드를 바꾸는 일이므로 비const 쪽을 다시 얻는다.
		GetGrid()->OnStageClear.AddDynamic(this, &APuzzleElevatorDock::HandleStageClear);
	}

	// 엔딩 승강기에는 갈 층이 없는 것이 정상이다. 짝도 목표 셀도 따지지 않는다.
	if (bIsClear)
	{
		return;
	}

	// 짝이 있으면 목표는 거기서 나온다. 구버전 값은 쳐다보지 않는다. 짝이 제대로 놓였는지는
	// 여기서 따질 수 없다 -- 그 구조물이 아직 자기 층을 정하지 않았을 수 있다. 첫 틱으로 미룬다.
	if (GetTargetDock())
	{
		return;
	}

	if (!CurrentGrid->IsValidCell(TargetFloorCell))
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: no target dock and no target floor cell; falling back to a fixed travel of %.0f cm %s."),
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

// ---------------------------------------------------------------------------- 짝과 층

APuzzleElevatorDock* APuzzleElevatorDock::GetTargetDock() const
{
	if (APuzzleElevatorDock* Explicit = TargetDock.Get())
	{
		return Explicit;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 배치는 게임이 도는 동안 바뀌지 않으므로 한 번만 훑는다. 에디터에서는 디자이너가 방금
	// 반대쪽에 값을 넣었을 수 있으니 매번 다시 본다.
	const bool bCanCache = World->IsGameWorld();
	if (bCanCache && bReverseTargetResolved)
	{
		return ReverseTargetDock.Get();
	}

	APuzzleElevatorDock* Found = nullptr;
	for (TActorIterator<APuzzleElevatorDock> It(const_cast<UWorld*>(World)); It; ++It)
	{
		APuzzleElevatorDock* Other = *It;
		if (Other && Other != this && Other->TargetDock.Get() == this)
		{
			Found = Other;
			break;
		}
	}

	if (bCanCache)
	{
		ReverseTargetDock = Found;
		bReverseTargetResolved = true;
	}

	return Found;
}

double APuzzleElevatorDock::ResolveFloorZ(const AGridActor& InGrid, FIntPoint MinCell, double PlacedZ) const
{
	// 구조물 하나가 왕복하던 시절에는 샤프트 셀이 곧 이 구조물의 층이었다. 그대로 둔다.
	if (!GetTargetDock())
	{
		return Super::ResolveFloorZ(InGrid, MinCell, PlacedZ);
	}

	// 짝을 이룬 구조물은 층마다 하나씩 같은 XY에 겹쳐 놓인다. 그 XY의 셀은 아래층 높이
	// 하나로 구워지므로 셀에서는 위층을 알 수 없다. 대신 구조물 둘레 한 칸 -- 차체의 문이
	// 열릴 자리 -- 의 바닥 중에서 디자이너가 놓은 높이에 가장 가까운 것을 층으로 삼는다.
	// 그래서 저작은 "대충 그 층 높이에 놓기"로 끝난다.
	double BestZ = PlacedZ;
	double BestDistance = TNumericLimits<double>::Max();

	for (int32 Offset = -1; Offset <= SizeInCells; ++Offset)
	{
		const FIntPoint Ring[4] =
		{
			FIntPoint(MinCell.X + Offset, MinCell.Y - 1),
			FIntPoint(MinCell.X + Offset, MinCell.Y + SizeInCells),
			FIntPoint(MinCell.X - 1, MinCell.Y + Offset),
			FIntPoint(MinCell.X + SizeInCells, MinCell.Y + Offset)
		};

		for (const FIntPoint& Cell : Ring)
		{
			const FGridCellData* Data = InGrid.GetCell(Cell);
			if (!Data || !InGrid.IsCellWalkableStatic(Cell))
			{
				continue;
			}

			const double Distance = FMath::Abs(Data->FloorZ - PlacedZ);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestZ = Data->FloorZ;
			}
		}
	}

	return BestZ;
}

bool APuzzleElevatorDock::CanCoexistWith(const APuzzleFloorTile& Other) const
{
	// 층이 다른 엘리베이터 구조물끼리는 같은 XY를 나눠 가진다. 그것이 바로 샤프트다.
	const APuzzleElevatorDock* OtherDock = Cast<APuzzleElevatorDock>(&Other);
	return OtherDock && !FMath::IsNearlyEqual(OtherDock->GetFloorZ(), GetFloorZ(), 10.0);
}

bool APuzzleElevatorDock::FullyContains(const APuzzleBlock& Block) const
{
	if (!Super::FullyContains(Block))
	{
		return false;
	}

	// 짝이 없으면 같은 XY에 다른 구조물도 없다. 층을 따질 이유가 없고, 따지면 오히려
	// 구버전 배치(차체가 샤프트 셀 높이에 걸려 있는)가 도킹을 잃는다.
	if (!GetTargetDock())
	{
		return true;
	}

	return FMath::IsNearlyEqual(Block.GetFloorZ(), GetFloorZ(), 10.0);
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
				TEXT("The elevator is in place. Click it to ride."),
				FLinearColor(0.45f, 0.85f, 1.0f));
		}
	}
}

void APuzzleElevatorDock::LogPairingOnce()
{
	if (bPairingLogged)
	{
		return;
	}
	bPairingLogged = true;

	// 엔딩 승강기는 짝이 없어도, 있어도 그쪽으로 가지 않는다. 짝 이야기를 찍으면 헷갈린다.
	if (bIsClear)
	{
		UE_LOG(LogLTTSGrid, Display,
			TEXT("%s: stage-clear exit at Z %.0f; rides %.0f cm %s at %.0f cm/s, then the cutscene."),
			*GetName(), GetFloorZ(), ClearTravelHeight,
			bClearTravelUp ? TEXT("up") : TEXT("down"),
			ClearTravelSpeed > 0.0f ? ClearTravelSpeed : TravelSpeed);
		return;
	}

	const APuzzleElevatorDock* Target = GetTargetDock();
	if (!Target)
	{
		return;
	}

	// 이제는 모든 구조물이 BeginPlay를 지났으므로 짝의 층 높이를 믿을 수 있다.
	if (FMath::IsNearlyEqual(Target->GetFloorZ(), GetFloorZ(), 10.0))
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: its target dock %s sits at the same floor (Z %.0f); the elevator would have nowhere to go."),
			*GetName(), *Target->GetName(), GetFloorZ());
		return;
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: Z %.0f -> %s at Z %.0f (%.0f cm %s)."),
		*GetName(), GetFloorZ(), *Target->GetName(), Target->GetFloorZ(),
		FMath::Abs(Target->GetFloorZ() - GetFloorZ()),
		Target->GetFloorZ() > GetFloorZ() ? TEXT("up") : TEXT("down"));
}

void APuzzleElevatorDock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	LogPairingOnce();

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
	// 짝이 있으면 목표는 그 구조물의 층이다. 올라가는 것과 내려가는 것이 같은 규칙 하나가
	// 되고, 차체가 지금 어느 층에 있는지 따질 필요조차 없다 -- 이 구조물이 자기 층의 차체만
	// 자기 것으로 치므로(FullyContains), 여기까지 왔다는 것은 이미 출발 층이라는 뜻이다.
	if (const APuzzleElevatorDock* Target = GetTargetDock())
	{
		return Target->GetFloorZ();
	}

	const double NearZ = GetPivotWorld().Z;		// 구조물이 놓인 층
	const double FarZ = GetFarFloorZ();			// 반대편 층

	// 차체가 이미 반대편 층에 있으면 구조물 층으로 되돌아간다. 구조물 하나로 왕복이 되어야
	// 플레이어가 층을 잘못 골랐을 때 갇히지 않는다.
	return FMath::IsNearlyEqual(Elevator.GetFloorZ(), FarZ, 10.0) ? NearZ : FarZ;
}

bool APuzzleElevatorDock::FindArrivalExit(
	const APuzzleElevatorBlock& Elevator, double TargetZ, const FVector& SeatWorld, FVector& OutWorld) const
{
	const AGridActor* CurrentGrid = GetGrid();
	if (!CurrentGrid)
	{
		return false;
	}

	TArray<FIntPoint> FrontCells;
	Elevator.GetDoorFrontCells(FrontCells);

	bool bFound = false;
	double BestDistanceSq = TNumericLimits<double>::Max();

	for (const FIntPoint& Cell : FrontCells)
	{
		const FGridCellData* Data = CurrentGrid->GetCell(Cell);
		if (!Data
			|| !CurrentGrid->IsCellWalkableStatic(Cell)
			|| !FMath::IsNearlyEqual(Data->FloorZ, TargetZ, 10.0))
		{
			continue;
		}

		// 문 앞에 설 자리가 여럿이면 좌석에 가장 가까운 칸으로 내린다. 탈 때와 같은 규칙이라
		// 폰이 들어간 문으로 똑바로 걸어 나온다.
		const FVector World = CurrentGrid->CellToWorld(Cell);
		const double DistanceSq = FVector::DistSquaredXY(World, SeatWorld);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			OutWorld = World;
			bFound = true;
		}
	}

	return bFound;
}

bool APuzzleElevatorDock::FindExitWorld(
	const APuzzleElevatorBlock& Elevator, double TargetZ, const FVector& SeatWorld, FVector& OutWorld) const
{
	const AGridActor* CurrentGrid = GetGrid();
	if (!CurrentGrid)
	{
		return false;
	}

	// 짝이 있으면 도착 층의 주인은 그 구조물이다. 내릴 자리도 거기서 나온다.
	if (const APuzzleElevatorDock* Target = GetTargetDock())
	{
		if (Target->FindArrivalExit(Elevator, TargetZ, SeatWorld, OutWorld))
		{
			return true;
		}

		// 문 앞에 설 자리가 없다. 도착 구조물이 가리키는 방향 한 칸 바깥에 내려놓으면
		// WalkOntoGrid가 그 근처에서 걸을 수 있는 셀을 찾아 준다.
		const FIntPoint Step = LTTSGrid::DirOffset(Target->ExitDirection);
		const FVector TargetPivot = Target->GetPivotWorld();
		OutWorld = FVector(
			TargetPivot.X + Step.X * (Target->SizeInCells * 0.5 + 0.5) * CurrentGrid->CellSize,
			TargetPivot.Y + Step.Y * (Target->SizeInCells * 0.5 + 0.5) * CurrentGrid->CellSize,
			TargetZ);
		return true;
	}

	// --- 구버전: 목적 층 셀이 그 높이에 있으면 그것이 곧 내릴 자리다.
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
	if (FindArrivalExit(Elevator, TargetZ, SeatWorld, OutWorld))
	{
		return true;
	}

	// 마지막 수단: 출구 방향으로 한 칸 바깥의 목표 높이 지점. 그리드가 근처에서 걸을 수
	// 있는 셀을 찾아 준다.
	const FIntPoint Step = LTTSGrid::DirOffset(ExitDirection);
	OutWorld = FVector(
		GetPivotWorld().X + Step.X * (SizeInCells * 0.5 + 0.5) * CurrentGrid->CellSize,
		GetPivotWorld().Y + Step.Y * (SizeInCells * 0.5 + 0.5) * CurrentGrid->CellSize,
		TargetZ);
	return true;
}

bool APuzzleElevatorDock::CanLaunch(FText* OutReason) const
{
	const APuzzleElevatorBlock* Elevator = DockedElevator.Get();

	if (!Elevator || !FullyContains(*Elevator))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockEmpty", "Push the elevator onto the dock first.");
		}
		return false;
	}

	if (Elevator->IsTravelling() || Elevator->IsAnimating())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockBusy", "The elevator is busy.");
		}
		return false;
	}

	// 엔딩 승강기는 여기서 끝이다. 목표 층도 내릴 자리도 없는 것이 정상이므로, 그 둘을
	// 따지는 아래 검사를 통과할 수 없고 통과할 필요도 없다.
	if (bIsClear)
	{
		return true;
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

	// 내릴 자리가 있는지는 폰 없이도 알 수 있다. 차체 중심을 기준으로 한 번 찾아 본다 --
	// 실제로 고르는 칸은 폰의 좌석에 따라 달라지지만, 있느냐 없느냐는 같은 답이다.
	FVector ExitWorld;
	if (!FindExitWorld(*Elevator, TargetZ, Elevator->GetActorLocation(), ExitWorld))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockNoExit", "There is nowhere to step off over there.");
		}
		return false;
	}

	return true;
}

bool APuzzleElevatorDock::TryLaunch(AGridPawn* Pawn, FText* OutReason)
{
	if (!CanLaunch(OutReason))
	{
		return false;
	}

	APuzzleElevatorBlock* Elevator = DockedElevator.Get();

	if (!Pawn || !Pawn->IsOnGrid() || Pawn->IsMoving())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockPawnBusy", "Wait until you have stopped walking.");
		}
		return false;
	}

	// 엔딩 승강기: 정해진 높이만큼 움직이다 멈춘다. 목표는 층이 아니라 이 구조물 층에서 잰
	// 거리이고, 도착해도 내려 주지 않는다.
	if (bIsClear)
	{
		const double ClearTargetZ = GetFloorZ()
			+ (bClearTravelUp ? ClearTravelHeight : -ClearTravelHeight);
		const float Speed = ClearTravelSpeed > 0.0f ? ClearTravelSpeed : TravelSpeed;

		// 멈춰 서는 순간을 받아 연출을 부른다. AddUnique이므로 같은 차체를 다시 태워도
		// 핸들러가 겹치지 않는다.
		Elevator->OnHoldReached.AddUniqueDynamic(this, &APuzzleElevatorDock::HandleHoldReached);

		if (!Elevator->StartHoldingTravel(ClearTargetZ, Speed, Pawn))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "DockBusy", "The elevator is busy.");
			}
			return false;
		}

		return true;
	}

	const double TargetZ = ComputeTargetZ(*Elevator);

	// 출구는 도착 층에서 폰이 내릴 자리다. 폰이 차 안에서 설 자리(좌석)를 먼저 구해 두는
	// 이유는, 문 앞에 설 칸이 여럿일 때 들어간 문으로 그대로 나오게 하기 위해서다.
	const FVector Seat = Elevator->GetSeatWorldFor(*Pawn);

	FVector ExitWorld;
	if (!FindExitWorld(*Elevator, TargetZ, Seat, ExitWorld))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "DockNoExit", "There is nowhere to step off over there.");
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

// ---------------------------------------------------------------------------- 스테이지 클리어

void APuzzleElevatorDock::HandleStageClear(APawn* Pawn, FIntPoint Cell)
{
	// 행인은 StageClear 셀을 밟아도 아무 일이 없어야 한다. 지금은 행인이 그 사건을 내지
	// 않지만, 스테이지 클리어는 플레이어의 사건이라는 사실을 여기에도 적어 둔다.
	if (!Cast<AGridPawn>(Pawn) || bIsClear)
	{
		return;
	}

	bIsClear = true;

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: stage cleared at cell (%d,%d); this dock is now the way out."),
		*GetName(), Cell.X, Cell.Y);
}

void APuzzleElevatorDock::HandleHoldReached(APuzzleElevatorBlock* Elevator, APawn* Pawn)
{
	// 연출은 한 번뿐이다. 엔딩은 되풀이되지 않는다.
	if (bClearCutsceneFired)
	{
		return;
	}
	bClearCutsceneFired = true;

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: stage-clear cutscene starts with %s aboard %s."),
		*GetName(), *GetNameSafe(Pawn), *GetNameSafe(Elevator));

	// 블루프린트가 파생 클래스로 구현했다면 그쪽이, 레벨 블루프린트가 매달았다면 이쪽이
	// 받는다. 둘 다 두어야 구조물을 블루프린트로 바꾸지 않고도 연출을 붙일 수 있다.
	OnStageClearCutscene(Elevator, Pawn);
	OnClearCutscene.Broadcast(this, Elevator, Pawn);
}

void APuzzleElevatorDock::OnStageClearCutscene_Implementation(APuzzleElevatorBlock* Elevator, APawn* Rider)
{
	// 비어 있다. 열차의 문 연출 훅과 같은 규칙이다: 규칙은 C++가 쥐고 있고, 연출이 없어도
	// 게임은 성립한다. 페이드·시퀀스·씬 전환은 블루프린트가 여기를 덮어써서 한다.
	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: no cutscene is bound. Override OnStageClearCutscene or bind OnClearCutscene."),
		*GetName());
}

void APuzzleElevatorDock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AGridActor* CurrentGrid = GetGrid())
	{
		CurrentGrid->OnStageClear.RemoveDynamic(this, &APuzzleElevatorDock::HandleStageClear);
	}

	Super::EndPlay(EndPlayReason);
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
