// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleElevatorBlock.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Puzzle/PuzzleElevatorDock.h"
#include "Puzzle/PuzzleSubsystem.h"
#include "Player/GridPawn.h"
#include "Player/GridPlayerController.h"
// 지금은 쓰지 않는다. GetSeatWorldFor의 ELEVATOR SEAT FACING RIDER 블록을 되살릴 때 필요하다.
#include "Vehicle/VehicleSeat.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

EPuzzleMoveAxis APuzzleElevatorBlock::GetWorldDoorAxis() const
{
	return LTTSPuzzle::RotateAxis(DoorAxis, GetQuarterTurns());
}

EGridDirection APuzzleElevatorBlock::GetWorldDoorDirection() const
{
	// 문은 축의 양쪽에 하나씩 있다. 방향 하나를 집어야 하는 곳(문 앞 셀 계산, 로그)을 위해
	// 양의 방향을 대표로 돌려준다. 반대쪽 문은 이것을 두 번 돌린 방향이다.
	EGridDirection Negative = EGridDirection::South;
	EGridDirection Positive = EGridDirection::North;
	LTTSPuzzle::GetAxisDirections(GetWorldDoorAxis(), Negative, Positive);
	return Positive;
}

EPuzzleMoveAxis APuzzleElevatorBlock::GetWorldMoveAxis() const
{
	// 지정된 축이 아니라 문에서 유도한다. 그래야 회전 뒤에 둘이 서로 어긋나는 일이
	// 절대 없다.
	return GetWorldDoorAxis();
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

void APuzzleElevatorBlock::GetBoardableDoorCells(TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();

	const AGridActor* CurrentGrid = GetGrid();
	if (!CurrentGrid)
	{
		return;
	}

	TArray<FIntPoint> FrontCells;
	GetDoorFrontCells(FrontCells);

	for (const FIntPoint& Cell : FrontCells)
	{
		// 차체와 같은 층이어야 한다. 샤프트에 걸린 차체는 한쪽 문이 선로나 아래층을 향하고
		// 있어서, 그쪽으로 폰을 보내면 걸어갈 길도 없고 도착해 봤자 탈 수도 없다.
		const FGridCellData* Data = CurrentGrid->GetCell(Cell);
		if (Data
			&& CurrentGrid->IsCellWalkableStatic(Cell)
			&& FMath::IsNearlyEqual(Data->FloorZ, GetFloorZ(), 10.0))
		{
			OutCells.AddUnique(Cell);
		}
	}
}

bool APuzzleElevatorBlock::IsPawnAtDoor(const AGridPawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	TArray<FIntPoint> FrontCells;
	GetBoardableDoorCells(FrontCells);
	return FrontCells.Contains(Pawn->GetCurrentCell());
}

FVector APuzzleElevatorBlock::GetSeatWorldFor(const AGridPawn& Pawn) const
{
	// --- ELEVATOR SEAT FACING RIDER DISABLED 2026-09-11 ---
	// 한동안 열차와 같은 규칙을 썼다: 문 축 위의 자리는 폰의 것을 그대로 두고 수직으로만
	// 들어오기. 열차에서는 그것이 옳지만 엘리베이터는 아니다(기획). 되살리려면 아래 #if 0을
	// 1로 바꾸고 그 아래 return 문을 지운다. 아래 include도 그래서 남겨 두었다.
#if 0
	const EPuzzleMoveAxis EdgeAxis = LTTSPuzzle::RotateAxis(GetWorldDoorAxis(), 1);
	const FVector EdgeAxisWorld = (EdgeAxis == EPuzzleMoveAxis::AxisX)
		? FVector::XAxisVector
		: FVector::YAxisVector;

	// 가장 바깥 셀의 중심까지만 허용한다. 그보다 밖은 차체가 아니다.
	const double CellSize = GetGrid() ? GetGrid()->CellSize : 100.0;
	const FIntPoint Footprint = GetWorldFootprint();
	const int32 NumCells = (EdgeAxis == EPuzzleMoveAxis::AxisX) ? Footprint.X : Footprint.Y;
	const double HalfExtent = FMath::Max((NumCells * CellSize - CellSize) * 0.5, 0.0);

	return LTTSVehicle::SeatFacingRider(
		Pawn.GetActorLocation(),
		GetActorLocation(),
		EdgeAxisWorld,
		HalfExtent,
		GetFloorZ() + Pawn.HeightAboveFloor);
#endif

	// 엘리베이터는 **차체 한가운데**로 모인다. 열차와 다른 이유는 탈것의 생김새가 다르기
	// 때문이다: 열차는 45 m짜리 객차라 탄 문 앞에 그대로 서 있는 것이 자연스럽지만,
	// 엘리베이터는 4 m짜리 방이고 문이 양쪽에 하나씩이라 어느 쪽으로 들어왔든 가운데 서는
	// 것이 사람이 하는 짓에 가깝다.
	//
	// 내릴 때도 같은 규칙이 이어진다: 좌석이 가운데이므로 FindArrivalExit가 고르는 "좌석에
	// 가장 가까운 문 앞 칸"은 문 한가운데 칸이 되고, 폰은 거기로 똑바로 걸어 나간다.
	return FVector(
		GetActorLocation().X,
		GetActorLocation().Y,
		GetFloorZ() + Pawn.HeightAboveFloor);
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
	EGridDirection Negative = EGridDirection::South;
	EGridDirection Positive = EGridDirection::North;
	LTTSPuzzle::GetAxisDirections(DoorAxis, Negative, Positive);

	const FIntPoint Offset = LTTSGrid::DirOffset(Positive) * Sign;
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
		*StaticEnum<EPuzzleMoveAxis>()->GetNameStringByValue(static_cast<int64>(GetWorldDoorAxis())));

	OnBoarded.Broadcast(this, Pawn);
	return true;
}

// ---------------------------------------------------------------------------- 승강

bool APuzzleElevatorBlock::BeginTravel(double TargetZ, float Speed, AGridPawn* Pawn)
{
	if (!Pawn || IsTravelling() || IsAnimating())
	{
		return false;
	}

	TravelTargetZ = TargetZ;
	TravelSpeed = FMath::Max(Speed, 1.0f);
	Rider = Pawn;

	// 좌석은 차체 한가운데다. 폰이 문 앞 셀에서 여기까지 직선으로 걸어 들어온다.
	Pawn->BoardVehicle(this, GetSeatWorldFor(*Pawn));

	// Lifting은 IsAnimating()에 포함되므로, 이 순간부터 드래그와 회전이 전부 잠긴다.
	SetAnimState(EAnimState::Lifting);
	TravelPhase = ETravelPhase::WaitingForRider;

	return true;
}

bool APuzzleElevatorBlock::StartVerticalTravel(
	double TargetZ, const FVector& ExitWorld, float Speed, float DwellSeconds, AGridPawn* Pawn)
{
	if (!BeginTravel(TargetZ, Speed, Pawn))
	{
		return false;
	}

	TravelExitWorld = ExitWorld;
	TravelDwellSeconds = FMath::Max(DwellSeconds, 0.0f);
	TravelDwellElapsed = 0.0f;
	bHoldAtTarget = false;

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: launching from Z %.0f to Z %.0f at %.0f cm/s."),
		*GetName(), GetFloorZ(), TravelTargetZ, TravelSpeed);

	return true;
}

bool APuzzleElevatorBlock::StartHoldingTravel(double TargetZ, float Speed, AGridPawn* Pawn)
{
	if (!BeginTravel(TargetZ, Speed, Pawn))
	{
		return false;
	}

	// 내릴 자리가 없다. 있을 수도 없다 -- 목표 높이는 층이 아니라 허공이다.
	TravelExitWorld = FVector::ZeroVector;
	TravelDwellSeconds = 0.0f;
	TravelDwellElapsed = 0.0f;
	bHoldAtTarget = true;

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: stage-clear ride from Z %.0f to Z %.0f at %.0f cm/s; it will hold there."),
		*GetName(), GetFloorZ(), TravelTargetZ, TravelSpeed);

	return true;
}

void APuzzleElevatorBlock::FinishTravel()
{
	AGridPawn* Pawn = Rider.Get();

	TravelPhase = ETravelPhase::None;
	bHoldAtTarget = false;
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

			if (bHoldAtTarget)
			{
				// FloorZ는 건드리지 않는다. 여기는 층이 아니라 허공이고, 층으로 기록하면
				// 구조물이 "내 층의 차체가 아니다"라며 도킹을 풀어 버린다. 그러면 연출이
				// 시작되기도 전에 잠금이 한 겹 풀린다.
				TravelPhase = ETravelPhase::Holding;

				UE_LOG(LogLTTSGrid, Display,
					TEXT("%s: holding at Z %.0f with %s aboard; the cutscene takes over."),
					*GetName(), TravelTargetZ, *GetNameSafe(Pawn));

				OnHoldReached.Broadcast(this, Pawn);
				break;
			}

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

	case ETravelPhase::Holding:
		// 종착이다. 폰은 차 안에 그대로 있고 조작은 잠긴 채로 둔다. 여기서 빠져나가는
		// 길은 연출이 씬을 넘기는 것뿐이다.
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------- 구버전 이전

void APuzzleElevatorBlock::PostLoad()
{
	Super::PostLoad();

	// 예전에는 문을 방향 하나(DoorDirection)로 적었다. 동서로 열리게 놓아 둔 차체가 조용히
	// 남북으로 바뀌면 퍼즐을 푸는 방법이 통째로 달라지므로, 저장된 방향을 축으로 옮긴다.
	// 표식을 세워 두 번 일어나지 않게 한다 -- 그러지 않으면 새로 지정한 축을 다음 로드가
	// 옛 방향으로 되돌린다.
	if (!bDoorAxisMigrated)
	{
		DoorAxis = LTTSPuzzle::AxisForDirection(DoorDirection);
		bDoorAxisMigrated = true;
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

void APuzzleElevatorBlock::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 저작에서 축을 골랐다면 구버전 이전은 끝난 것이다. 여기서 표식을 세우지 않으면 다음
	// 로드의 PostLoad가 옛 DoorDirection으로 되돌려 방금 고른 값을 지운다.
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(APuzzleElevatorBlock, DoorAxis))
	{
		bDoorAxisMigrated = true;
	}
}

#endif

// ---------------------------------------------------------------------------- 콘솔
//
// 열차의 ltts.TrainArrive와 같은 자리다. 엘리베이터 탑승은 마우스로만 시작할 수 있어서
// 자동 시험이 닿지 못하는 구석이었다 -- 특히 스테이지 클리어 연출은 탑승부터 컷신까지
// 한 번에 이어져야 하는데, 그 사슬을 손으로만 확인할 수 있었다.
//
// 클릭 경로와 **같은 함수**(RequestElevatorBoarding)를 부른다. 시험용 경로를 따로 만들면
// 시험이 통과해도 실제 조작이 통과한다는 보장이 없다.

namespace
{
	APuzzleElevatorBlock* FindElevator(UWorld* World, const FString& Filter)
	{
		APuzzleElevatorBlock* Fallback = nullptr;

		for (TActorIterator<APuzzleElevatorBlock> It(World); It; ++It)
		{
			APuzzleElevatorBlock* Elevator = *It;
			if (!Elevator)
			{
				continue;
			}

			if (!Filter.IsEmpty())
			{
				if (Elevator->GetName().Contains(Filter) || Elevator->GetActorNameOrLabel().Contains(Filter))
				{
					return Elevator;
				}
				continue;
			}

			// 이름을 주지 않았으면 지금 구조물 위에 올라가 있는 차체를 고른다. 시험하려는
			// 것이 대개 그것이고, 아니면 아무거나 골라 "구조물 위로 먼저"만 듣게 된다.
			if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(Elevator))
			{
				if (Subsystem->FindDockUnder(*Elevator))
				{
					return Elevator;
				}
			}

			if (!Fallback)
			{
				Fallback = Elevator;
			}
		}

		return Fallback;
	}

	void ElevatorRideCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.ElevatorRide: run this in play mode."));
			return;
		}

		AGridPlayerController* Controller =
			Cast<AGridPlayerController>(World->GetFirstPlayerController());
		if (!Controller)
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.ElevatorRide: no grid player controller."));
			return;
		}

		const FString Filter = Args.IsValidIndex(0) ? Args[0] : FString();
		APuzzleElevatorBlock* Elevator = FindElevator(World, Filter);

		if (!Elevator)
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("ltts.ElevatorRide: no elevator matching '%s'."), *Filter);
			return;
		}

		UE_LOG(LogLTTSGrid, Display,
			TEXT("ltts.ElevatorRide: asking to board %s."), *Elevator->GetActorNameOrLabel());

		Controller->RequestElevatorBoarding(Elevator);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GElevatorRideCommand(
	TEXT("ltts.ElevatorRide"),
	TEXT("Board a docked elevator as if it were clicked: ltts.ElevatorRide [name substring]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ElevatorRideCommand));
