// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleElevatorBlock.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Puzzle/PuzzleElevatorDock.h"
#include "Puzzle/PuzzleSubsystem.h"
#include "Player/GridPawn.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APuzzleElevatorBlock::APuzzleElevatorBlock()
{
	FootprintSize = FIntPoint(4, 4);
	MoveAxis = EPuzzleMoveAxis::AxisY;
	Height = 300.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// 다른 그레이박스 머티리얼을 쓴다. 문은 이 블록이 어느 쪽으로 움직일 수 있는지
	// 플레이어가 한눈에 읽어야 하는 유일한 특징이기 때문이다.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DoorMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	// 문은 둘이다. 기획의 "문이 양방향으로 있어 회전 방향은 중요하지 않다"를 그대로 그린다.
	const TCHAR* Names[2] = { TEXT("DoorMesh"), TEXT("FarDoorMesh") };
	TObjectPtr<UStaticMeshComponent>* Slots[2] = { &DoorMesh, &FarDoorMesh };

	for (int32 Index = 0; Index < 2; ++Index)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Names[Index]);
		Mesh->SetupAttachment(SceneRoot);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetGenerateOverlapEvents(false);

		if (CubeFinder.Succeeded())
		{
			Mesh->SetStaticMesh(CubeFinder.Object);
		}
		if (DoorMaterialFinder.Succeeded())
		{
			Mesh->SetMaterial(0, DoorMaterialFinder.Object);
		}

		*Slots[Index] = Mesh;
	}
}

// ---------------------------------------------------------------------------- 문

EGridDirection APuzzleElevatorBlock::GetWorldDoorDirection() const
{
	return LTTSGrid::RotateDirection(DoorDirection, GetQuarterTurns());
}

EPuzzleMoveAxis APuzzleElevatorBlock::GetWorldMoveAxis() const
{
	// 지정된 축이 아니라 문에서 유도한다. 그래야 회전 뒤에 둘이 서로 어긋나는 일이
	// 절대 없다.
	return LTTSPuzzle::AxisForDirection(GetWorldDoorDirection());
}

void APuzzleElevatorBlock::GetDoorFrontCells(EGridDirection Door, TArray<FIntPoint>& OutCells) const
{
	const FIntPoint Step = LTTSGrid::DirOffset(Door);
	const FGridRect Rect = GetRect();

	// 풋프린트에서 문이 향한 모서리를 따라가며 각 셀에서 한 칸 바깥으로 나간다.
	if (Step.X != 0)
	{
		const int32 EdgeX = (Step.X > 0) ? Rect.MaxInclusive().X : Rect.Min.X;
		for (int32 Y = Rect.Min.Y; Y < Rect.MaxExclusive().Y; ++Y)
		{
			OutCells.Emplace(EdgeX + Step.X, Y);
		}
	}
	else
	{
		const int32 EdgeY = (Step.Y > 0) ? Rect.MaxInclusive().Y : Rect.Min.Y;
		for (int32 X = Rect.Min.X; X < Rect.MaxExclusive().X; ++X)
		{
			OutCells.Emplace(X, EdgeY + Step.Y);
		}
	}
}

void APuzzleElevatorBlock::GetDoorFrontCells(TArray<FIntPoint>& OutCells) const
{
	const EGridDirection Door = GetWorldDoorDirection();

	GetDoorFrontCells(Door, OutCells);
	GetDoorFrontCells(LTTSGrid::RotateDirection(Door, 2), OutCells);
}

bool APuzzleElevatorBlock::IsPawnAtDoor(const AGridPawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	TArray<FIntPoint> FrontCells;
	GetDoorFrontCells(FrontCells);
	return FrontCells.Contains(Pawn->GetCurrentCell());
}

// ---------------------------------------------------------------------------- 비주얼

void APuzzleElevatorBlock::PlaceDoorMesh(UStaticMeshComponent* Mesh, int32 Sign) const
{
	if (!Mesh)
	{
		return;
	}

	// 로컬 프레임에 배치한다: 액터의 yaw가 이미 회전을 담당하므로 문 슬랩은 별도 계산
	// 없이 몸체를 따라간다.
	const double CellSize = 100.0;
	const double HalfX = FootprintSize.X * CellSize * 0.5;
	const double HalfY = FootprintSize.Y * CellSize * 0.5;
	const double Thickness = 20.0;

	// --- CUTAWAY DISABLED 2026-09-04: 원래는 GetVisualHeight() ---
	const double VisualHeight = Height;

	// 두께의 절반만큼 더 밖으로 밀어, 슬랩이 몸체에 반쯤 박혀 이음매처럼 보이지 않고
	// 몸체보다 도드라지게 한다.
	const FIntPoint Offset = LTTSGrid::DirOffset(DoorDirection) * Sign;
	const FVector Location(
		Offset.X * (HalfX + Thickness * 0.5),
		Offset.Y * (HalfY + Thickness * 0.5),
		VisualHeight * 0.4);

	const bool bAlongX = (Offset.X != 0);
	const FVector Scale(
		bAlongX ? Thickness / 100.0 : FootprintSize.X * CellSize * 0.7 / 100.0,
		bAlongX ? FootprintSize.Y * CellSize * 0.7 / 100.0 : Thickness / 100.0,
		VisualHeight * 0.6 / 100.0);

	Mesh->SetRelativeLocation(Location);
	Mesh->SetRelativeScale3D(Scale);
}

void APuzzleElevatorBlock::RefreshVisual()
{
	Super::RefreshVisual();

	PlaceDoorMesh(DoorMesh, 1);
	PlaceDoorMesh(FarDoorMesh, -1);

	// 아트 엘리베이터는 자기 문을 갖고 있다. 그레이박스 문 표시를 겹쳐 두면 두 겹이 된다.
	const bool bArt = IsUsingArtVisual();
	if (DoorMesh)
	{
		DoorMesh->SetVisibility(!bArt);
	}
	if (FarDoorMesh)
	{
		FarDoorMesh->SetVisibility(!bArt);
	}
}

void APuzzleElevatorBlock::OnConstruction(const FTransform& Transform)
{
	// 누가 무엇을 입력하든 지정된 풋프린트를 정사각형으로 유지한다: 직사각형 엘리베이터는
	// 돌 때마다 덮는 셀이 바뀌는데, 회전 규칙은 그것을 허용하지 않는다.
	FootprintSize = FIntPoint(4, 4);

	Super::OnConstruction(Transform);
}

// ---------------------------------------------------------------------------- 탑승

bool APuzzleElevatorBlock::TryBoard(AGridPawn* Pawn, FText* OutReason)
{
	if (!Pawn)
	{
		return false;
	}

	if (IsAnimating())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ElevatorMoving", "The elevator is still moving.");
		}
		return false;
	}

	if (Pawn->IsMoving() || !Pawn->IsOnGrid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ElevatorPawnMoving", "Wait until you have stopped walking.");
		}
		return false;
	}

	if (!IsPawnAtDoor(Pawn))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ElevatorNotAtDoor", "Walk around to a door first.");
		}
		return false;
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %s boarded from cell (%d,%d); doors face the %s axis."),
		*GetName(), *GetNameSafe(Pawn), Pawn->GetCurrentCell().X, Pawn->GetCurrentCell().Y,
		*StaticEnum<EGridDirection>()->GetNameStringByValue(static_cast<int64>(GetWorldDoorDirection())));

	OnBoarded.Broadcast(this, Pawn);
	return true;
}

// ---------------------------------------------------------------------------- 승강

bool APuzzleElevatorBlock::StartVerticalTravel(
	double TargetZ, const FVector& ExitWorld, float Speed, float DwellSeconds, AGridPawn* Pawn)
{
	if (!Pawn || IsTravelling() || IsAnimating())
	{
		return false;
	}

	TravelTargetZ = TargetZ;
	TravelExitWorld = ExitWorld;
	TravelSpeed = FMath::Max(Speed, 1.0f);
	TravelDwellSeconds = FMath::Max(DwellSeconds, 0.0f);
	TravelDwellElapsed = 0.0f;
	Rider = Pawn;

	// 좌석은 차체 중심의 바닥 위다. 폰이 문 앞 셀에서 여기까지 직선으로 걸어 들어온다.
	const FVector Seat(GetActorLocation().X, GetActorLocation().Y, GetFloorZ() + Pawn->HeightAboveFloor);
	Pawn->BoardVehicle(this, Seat);

	// Lifting은 IsAnimating()에 포함되므로, 이 순간부터 드래그와 회전이 전부 잠긴다.
	SetAnimState(EAnimState::Lifting);
	TravelPhase = ETravelPhase::WaitingForRider;

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: launching from Z %.0f to Z %.0f at %.0f cm/s."),
		*GetName(), GetFloorZ(), TravelTargetZ, TravelSpeed);

	return true;
}

void APuzzleElevatorBlock::FinishTravel()
{
	AGridPawn* Pawn = Rider.Get();

	TravelPhase = ETravelPhase::None;
	Rider.Reset();
	SetAnimState(EAnimState::Idle);

	OnArrived.Broadcast(this, Pawn);
}

void APuzzleElevatorBlock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (TravelPhase == ETravelPhase::None)
	{
		return;
	}

	AGridPawn* Pawn = Rider.Get();
	if (!Pawn)
	{
		// 승객이 사라졌다. 차체는 지금 높이에 그대로 두고 평소 상태로 돌아간다.
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: the rider vanished mid-travel; stopping here."), *GetName());
		SetFloorZ(GetActorLocation().Z);
		FinishTravel();
		return;
	}

	switch (TravelPhase)
	{
	case ETravelPhase::WaitingForRider:
		// 폰이 차체 안에 완전히 들어와 붙을 때까지 기다린다. 걷는 중에 바닥이 움직이면
		// 폰이 목표점을 영영 따라잡지 못한다.
		if (Pawn->IsRiding())
		{
			TravelPhase = ETravelPhase::Moving;
		}
		break;

	case ETravelPhase::Moving:
	{
		const FVector Location = GetActorLocation();
		const FVector Target(Location.X, Location.Y, TravelTargetZ);
		const FVector NewLocation = FMath::VInterpConstantTo(Location, Target, DeltaSeconds, TravelSpeed);
		SetActorLocation(NewLocation);

		if (NewLocation.Equals(Target, 0.5))
		{
			SetActorLocation(Target);

			// 새 층이 이제 이 차체의 바닥이다. 갱신하지 않으면 옆으로 밀 때 원래 층
			// 높이로 스냅되어 되돌아간다.
			SetFloorZ(TravelTargetZ);

			TravelPhase = ETravelPhase::Dwelling;
			TravelDwellElapsed = 0.0f;

			UE_LOG(LogLTTSGrid, Display, TEXT("%s: arrived at Z %.0f."), *GetName(), TravelTargetZ);
		}
		break;
	}

	case ETravelPhase::Dwelling:
		TravelDwellElapsed += DeltaSeconds;
		if (TravelDwellElapsed >= TravelDwellSeconds)
		{
			Pawn->WalkOntoGrid(TravelExitWorld);
			TravelPhase = ETravelPhase::Unloading;
		}
		break;

	case ETravelPhase::Unloading:
		// 폰이 그리드 위에 다시 서면 조작을 돌려준다.
		if (Pawn->IsOnGrid())
		{
			FinishTravel();
		}
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------- 이동 제약

bool APuzzleElevatorBlock::CanStartMoving(FText* OutReason) const
{
	// 기획: 플레이어가 탑승한 채로 엘리베이터를 드래그할 수 없다.
	if (Rider.IsValid() || IsTravelling())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ElevatorOccupied", "You cannot push the elevator while riding it.");
		}
		return false;
	}

	return true;
}

bool APuzzleElevatorBlock::CanOccupyRect(const FGridRect& Rect, FText* OutReason) const
{
	const AGridActor* CurrentGrid = GetGrid();
	if (!CurrentGrid)
	{
		return true;
	}

	// 위층으로 올라간 차체는 그 층의 바닥 위로만 나갈 수 있다. 셀은 층마다 높이가 하나뿐이라
	// 이 검사가 없으면 차체가 샤프트 셀로 되돌아가면서 원래 층 높이로 뚝 떨어진다.
	const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);

	TArray<FIntPoint> Cells;
	Rect.GatherCells(Cells);

	for (const FIntPoint& Cell : Cells)
	{
		// 구조물이 덮은 셀은 샤프트다. 샤프트 바닥은 아래층 높이로 구워지지만 차체는 위층에서
		// 밀려 들어와 위층 높이에 걸린 채 멈춘다 -- 그게 엘리베이터 통로다. 셀 단위로 따지는
		// 이유는 차체가 한 칸씩 움직여서, 밀어 넣는 동안 반드시 걸친 상태를 지나기 때문이다.
		if (Subsystem && Subsystem->IsDockCell(Cell))
		{
			continue;
		}

		const FGridCellData* Data = CurrentGrid->GetCell(Cell);
		if (!Data)
		{
			continue;	// 바닥 자체가 없는 셀은 기반 클래스가 거부한다
		}

		if (!FMath::IsNearlyEqual(Data->FloorZ, GetFloorZ(), 10.0))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "ElevatorWrongFloor", "The floor over there is at a different level.");
			}
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR

bool APuzzleElevatorBlock::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (!InProperty)
	{
		return true;
	}

	// 셋 다 엘리베이터의 정의상 고정이다: 4x4이고, 문 방향으로 이동하며, 한 번 타는 게
	// 한 번의 스윕이 아니라 의도적인 이동의 연속이 되도록 드래그당 한 셀씩 움직인다.
	const FName Name = InProperty->GetFName();
	return Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, FootprintSize)
		&& Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, MoveAxis)
		&& Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, bOneStepPerDrag);
}

#endif
