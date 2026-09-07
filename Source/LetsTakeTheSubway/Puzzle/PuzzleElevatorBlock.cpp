// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleElevatorBlock.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APuzzleElevatorBlock::APuzzleElevatorBlock()
{
	FootprintSize = FIntPoint(4, 4);
	MoveAxis = EPuzzleMoveAxis::AxisY;
	Height = 300.0f;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SceneRoot);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DoorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	DoorMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		DoorMesh->SetStaticMesh(CubeFinder.Object);
	}

	// 다른 그레이박스 머티리얼을 쓴다. 문은 이 블록이 어느 쪽으로 움직일 수 있는지
	// 플레이어가 한눈에 읽어야 하는 유일한 특징이기 때문이다.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DoorMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));
	if (DoorMaterialFinder.Succeeded())
	{
		DoorMesh->SetMaterial(0, DoorMaterialFinder.Object);
	}
}

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

void APuzzleElevatorBlock::RefreshVisual()
{
	Super::RefreshVisual();

	if (!DoorMesh)
	{
		return;
	}

	// 로컬 프레임에 배치한다: 액터의 yaw가 이미 회전을 담당하므로 문 슬랩은 별도 계산
	// 없이 몸체를 따라간다.
	const double CellSize = 100.0;
	const double HalfX = FootprintSize.X * CellSize * 0.5;
	const double HalfY = FootprintSize.Y * CellSize * 0.5;
	const double Thickness = 20.0;

	// 몸체가 지금 그려지는 높이를 기준으로 재므로, 블록이 납작해질 때 문도 슬랩 위에
	// 떠 있지 않고 같이 내려간다.
	// --- CUTAWAY DISABLED 2026-09-04: 원래는 GetVisualHeight() ---
	const double VisualHeight = Height;

	// 두께의 절반만큼 더 밖으로 밀어, 슬랩이 몸체에 반쯤 박혀 이음매처럼 보이지 않고
	// 몸체보다 도드라지게 한다.
	const FIntPoint Offset = LTTSGrid::DirOffset(DoorDirection);
	const FVector Location(
		Offset.X * (HalfX + Thickness * 0.5),
		Offset.Y * (HalfY + Thickness * 0.5),
		VisualHeight * 0.4);

	const bool bAlongX = (Offset.X != 0);
	const FVector Scale(
		bAlongX ? Thickness / 100.0 : FootprintSize.X * CellSize * 0.7 / 100.0,
		bAlongX ? FootprintSize.Y * CellSize * 0.7 / 100.0 : Thickness / 100.0,
		VisualHeight * 0.6 / 100.0);

	DoorMesh->SetRelativeLocation(Location);
	DoorMesh->SetRelativeScale3D(Scale);
}

void APuzzleElevatorBlock::OnConstruction(const FTransform& Transform)
{
	// 누가 무엇을 입력하든 지정된 풋프린트를 정사각형으로 유지한다: 직사각형 엘리베이터는
	// 돌 때마다 덮는 셀이 바뀌는데, 회전 규칙은 그것을 허용하지 않는다.
	FootprintSize = FIntPoint(4, 4);

	Super::OnConstruction(Transform);
}

void APuzzleElevatorBlock::GetDoorFrontCells(TArray<FIntPoint>& OutCells) const
{
	const EGridDirection Door = GetWorldDoorDirection();
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

	if (Pawn->IsMoving())
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
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ElevatorNotAtDoor", "Walk around to the door first.");
		}
		return false;
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %s boarded from cell (%d,%d); door faces %s."),
		*GetName(), *GetNameSafe(Pawn), Pawn->GetCurrentCell().X, Pawn->GetCurrentCell().Y,
		*StaticEnum<EGridDirection>()->GetNameStringByValue(static_cast<int64>(GetWorldDoorDirection())));

	OnBoarded.Broadcast(this, Pawn);
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
