// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleBlock.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APuzzleBlock::APuzzleBlock()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);

	// Visibility 채널만 막고 나머지는 막지 않는다: 클릭 트레이스가 이 채널로 커서가
	// 무엇을 잡았는지 식별하며, 물리와 폰(콜리전이 전혀 없다)은 여기에 전혀 관여하지
	// 않는다.
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BodyMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	if (MaterialFinder.Succeeded())
	{
		BodyMesh->SetMaterial(0, MaterialFinder.Object);
	}

	// 그리드는 블록 자체가 아니라 블록 아래의 바닥을 트레이스해야 한다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

	// 퍼즐은 하나로 연결된 기계 장치다: 카메라가 움직였다고 절반이 언로드되면 플레이어가
	// 풀 수 있는 내용이 조용히 바뀌어 버린다. 이 플래그는 쿠커가 읽는 저작 데이터라서
	// 에디터 빌드에만 존재한다.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

// ---------------------------------------------------------------------------- 조회

FIntPoint APuzzleBlock::GetWorldFootprint() const
{
	return (QuarterTurns % 2 == 0) ? FootprintSize : FIntPoint(FootprintSize.Y, FootprintSize.X);
}

EPuzzleMoveAxis APuzzleBlock::GetWorldMoveAxis() const
{
	return LTTSPuzzle::RotateAxis(MoveAxis, QuarterTurns);
}

void APuzzleBlock::GatherOccupiedCells(TArray<FIntPoint>& OutCells) const
{
	GetRect().GatherCells(OutCells);
}

// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
FBox APuzzleBlock::GetFullBounds() const
{
	if (!Grid)
	{
		return FBox(ForceInit);
	}

	const FGridRect Rect = GetRect();
	const FVector Origin = Grid->GetGridOrigin();

	const FVector Min(
		Origin.X + Rect.Min.X * Grid->CellSize,
		Origin.Y + Rect.Min.Y * Grid->CellSize,
		FloorZ);

	const FVector Max(
		Origin.X + Rect.MaxExclusive().X * Grid->CellSize,
		Origin.Y + Rect.MaxExclusive().Y * Grid->CellSize,
		FloorZ + Height);

	return FBox(Min, Max);
}
#endif

// ---------------------------------------------------------------------------- 배치

void APuzzleBlock::RefreshVisual()
{
	if (!BodyMesh)
	{
		return;
	}

	// 풋프린트는 로컬 프레임에서 지정하고 회전은 액터의 yaw가 담당하므로, 메시는 항상
	// 회전하지 않은 크기 기준으로 조정한다.
	const double CellSize = Grid ? Grid->CellSize : 100.0;

	// --- CUTAWAY DISABLED 2026-09-04: 원래는 GetVisualHeight() ---
	const double VisualHeight = Height;

	BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, VisualHeight * 0.5));
	BodyMesh->SetRelativeScale3D(FVector(
		FootprintSize.X * CellSize / 100.0,
		FootprintSize.Y * CellSize / 100.0,
		VisualHeight / 100.0));
}

// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
void APuzzleBlock::SetCutaway(bool bInCutaway)
{
	if (bCutawayTarget == bInCutaway)
	{
		return;
	}

	bCutawayTarget = bInCutaway;

	UE_LOG(LogLTTSGrid, Verbose, TEXT("%s: cutaway %s."), *GetName(), bInCutaway ? TEXT("on") : TEXT("off"));
}
#endif

void APuzzleBlock::ClaimCells()
{
	if (!Grid)
	{
		return;
	}

	Grid->ClearAllOccupantsOf(this);

	TArray<FIntPoint> Cells;
	GatherOccupiedCells(Cells);
	for (const FIntPoint& Cell : Cells)
	{
		Grid->SetOccupant(Cell, this);
	}
}

void APuzzleBlock::RegisterWithSubsystem(UPuzzleSubsystem& Subsystem)
{
	Subsystem.RegisterBlock(this);
}

void APuzzleBlock::UnregisterFromSubsystem(UPuzzleSubsystem& Subsystem)
{
	Subsystem.UnregisterBlock(this);
}

void APuzzleBlock::SnapToRect()
{
	if (!Grid)
	{
		return;
	}

	SetActorLocation(GridFootprint::CentreFromMinCell(*Grid, MinCell, GetWorldFootprint(), FloorZ));
}

void APuzzleBlock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
}

void APuzzleBlock::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; the block cannot be placed."), *GetName());
		return;
	}

	QuarterTurns = ((FMath::RoundToInt32(GetActorRotation().Yaw / 90.0) % 4) + 4) % 4;

	const FIntPoint WorldFootprint = GetWorldFootprint();
	MinCell = GridFootprint::MinCellFromCentre(*Grid, GetActorLocation(), WorldFootprint);
	FloorZ = Grid->CellToWorld(MinCell).Z;

	// 고치지 않고 보고만 한다: 플랫폼 밖으로 걸쳐 있거나 다른 블록과 겹친 블록은 레벨
	// 버그이며, 조용히 밀어 넣으면 디자이너가 어느 셀을 의도했는지 가려진다.
	TArray<FIntPoint> Cells;
	GatherOccupiedCells(Cells);
	for (const FIntPoint& Cell : Cells)
	{
		if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: covers cell (%d,%d), which is not walkable floor."), *GetName(), Cell.X, Cell.Y);
		}
		else if (Grid->IsCellOccupied(Cell, this))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: cell (%d,%d) is already taken by %s."),
				*GetName(), Cell.X, Cell.Y, *GetNameSafe(Grid->GetOccupant(Cell)));
		}
	}

	ClaimCells();
	SnapToRect();
	RefreshVisual();

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		RegisterWithSubsystem(*Subsystem);
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %dx%d block at cell (%d,%d), %d quarter turn(s)."),
		*GetName(), WorldFootprint.X, WorldFootprint.Y, MinCell.X, MinCell.Y, QuarterTurns);
}

void APuzzleBlock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Grid)
	{
		Grid->ClearAllOccupantsOf(this);
	}

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		UnregisterFromSubsystem(*Subsystem);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- 이동

bool APuzzleBlock::CanSlide(EGridDirection Dir, FText* OutReason) const
{
	if (!Grid)
	{
		return false;
	}

	if (!LTTSPuzzle::AxisAllowsDirection(GetWorldMoveAxis(), Dir))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockWrongAxis", "This block does not move that way.");
		}
		return false;
	}

	const FGridRect Target(MinCell + LTTSGrid::DirOffset(Dir), GetWorldFootprint());

	TArray<FIntPoint> Cells;
	Target.GatherCells(Cells);

	// 폰은 한 걸음의 대부분을 두 셀 사이에서 보내므로, 폰이 향하기로 한 셀도 점유된
	// 것으로 친다: 그 셀로 슬라이드한 블록은 결국 폰 위에 올라서게 된다.
	TArray<FIntPoint> PawnCells;
	if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->GetPawnReservedCells(PawnCells);
	}

	for (const FIntPoint& Cell : Cells)
	{
		if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockNoFloor", "There is no floor that way.");
			}
			return false;
		}

		if (Grid->IsCellOccupied(Cell, this))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockBlocked", "Something is in the way.");
			}
			return false;
		}

		if (PawnCells.Contains(Cell))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockPawnInWay", "You are standing in the way.");
			}
			return false;
		}
	}

	return true;
}

bool APuzzleBlock::StartSlide(EGridDirection Dir)
{
	if (!Grid || IsAnimating() || !CanSlide(Dir))
	{
		return false;
	}

	MinCell += LTTSGrid::DirOffset(Dir);

	// 액터가 실제로 움직이기 전에 점유한다: 이 걸음의 나머지 동안 폰과 다른 블록 모두
	// 목적지를 이미 차지된 것으로 취급해야 한다.
	ClaimCells();

	SlideTarget = GridFootprint::CentreFromMinCell(*Grid, MinCell, GetWorldFootprint(), FloorZ);
	AnimState = EAnimState::Sliding;
	++StepsWhileHeld;

	return true;
}

void APuzzleBlock::SetHeld(bool bInHeld)
{
	if (bHeld == bInHeld)
	{
		return;
	}

	bHeld = bInHeld;

	if (bHeld)
	{
		StepsWhileHeld = 0;
		return;
	}

	// 걸음과 걸음 사이에 놓았다: 블록은 이미 정지 상태이므로, 나중에 도착해서 회전 검사를
	// 일으킬 것이 없다.
	if (!IsAnimating())
	{
		ReportAtRest();
	}
}

void APuzzleBlock::ReportAtRest()
{
	if (StepsWhileHeld <= 0)
	{
		return;
	}

	StepsWhileHeld = 0;

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->NotifyBlockCameToRest(this);
	}
}

void APuzzleBlock::BeginRotation(const FVector& Pivot, int32 TurnSign, float Duration, const FGridRect& NewRect)
{
	if (!Grid)
	{
		return;
	}

	RotationPivot = Pivot;
	RotationTurnSign = (TurnSign >= 0) ? 1 : -1;
	RotationDuration = FMath::Max(Duration, 0.01f);
	RotationElapsed = 0.0f;
	RotationStartLocation = GetActorLocation();
	RotationStartYaw = GetActorRotation().Yaw;
	RotationTargetYaw = RotationStartYaw + 90.0 * RotationTurnSign;

	// 배치 변경 전체를 지금 확정한다. 뒤따르는 애니메이션은 장식일 뿐이다: 그동안의 모든
	// 조회는 이미 블록이 가게 될 위치를 보고한다.
	QuarterTurns = ((QuarterTurns + RotationTurnSign) % 4 + 4) % 4;
	MinCell = NewRect.Min;
	ClaimCells();

	RotationTargetLocation = GridFootprint::CentreFromMinCell(*Grid, MinCell, GetWorldFootprint(), FloorZ);
	AnimState = EAnimState::Rotating;
}

void APuzzleBlock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	// 컷어웨이는 슬라이드·회전과 독립적이다: 납작해진 채로도 블록을 옆으로 밀 수 있고,
	// 방해가 되는 동안에는 계속 납작한 상태여야 한다.
	const float TargetAlpha = bCutawayTarget ? 1.0f : 0.0f;
	if (!FMath::IsNearlyEqual(CutawayAlpha, TargetAlpha))
	{
		CutawayAlpha = FMath::FInterpConstantTo(CutawayAlpha, TargetAlpha, DeltaSeconds, 1.0f / FMath::Max(CutawayBlendTime, 0.01f));
		RefreshVisual();
	}
#endif

	switch (AnimState)
	{
	case EAnimState::Sliding:
	{
		const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), SlideTarget, DeltaSeconds, SlideSpeed);
		SetActorLocation(NewLocation);

		if (NewLocation.Equals(SlideTarget, 0.5))
		{
			SetActorLocation(SlideTarget);
			AnimState = EAnimState::Idle;

			if (!bHeld)
			{
				ReportAtRest();
			}
		}
		break;
	}

	case EAnimState::Rotating:
	{
		RotationElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(RotationElapsed / RotationDuration, 0.0f, 1.0f);
		const double Angle = 90.0 * RotationTurnSign * FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

		const FVector Offset = RotationStartLocation - RotationPivot;
		const FVector Swung = FRotator(0.0, Angle, 0.0).RotateVector(FVector(Offset.X, Offset.Y, 0.0));
		SetActorLocation(FVector(RotationPivot.X + Swung.X, RotationPivot.Y + Swung.Y, RotationStartLocation.Z));

		FRotator Rotation = GetActorRotation();
		Rotation.Yaw = RotationStartYaw + Angle;
		SetActorRotation(Rotation);

		if (Alpha >= 1.0f)
		{
			SetActorLocation(RotationTargetLocation);
			Rotation.Yaw = RotationTargetYaw;
			SetActorRotation(Rotation);
			AnimState = EAnimState::Idle;
		}
		break;
	}

	default:
		break;
	}
}

// ---------------------------------------------------------------------------- 에디터

#if WITH_EDITOR

void APuzzleBlock::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (!bFinished)
	{
		return;		// 드래그 중이다. 매 프레임 스냅하면 기즈모와 충돌한다
	}

	const AGridActor* FoundGrid = AGridActor::FindGrid(GetWorld());
	if (!FoundGrid)
	{
		return;
	}

	// yaw가 방향을 지정하는 채널이므로, 무엇이든 유도하기 전에 90도 단위로 반올림한다.
	// pitch와 roll은 풋프린트를 그리드에서 기울여 버린다.
	FRotator Rotation = GetActorRotation();
	const int32 Turns = ((FMath::RoundToInt32(Rotation.Yaw / 90.0) % 4) + 4) % 4;
	Rotation = FRotator(0.0, Turns * 90.0, 0.0);

	const FIntPoint WorldFootprint = (Turns % 2 == 0) ? FootprintSize : FIntPoint(FootprintSize.Y, FootprintSize.X);
	const FIntPoint SnappedMin = GridFootprint::MinCellFromCentre(*FoundGrid, GetActorLocation(), WorldFootprint);
	if (!FoundGrid->IsValidCell(SnappedMin))
	{
		return;		// 그리드 밖으로 드래그됐다. 다시 끌어올 수 있도록 그대로 둔다
	}

	const double SnapFloorZ = FoundGrid->CellToWorld(SnappedMin).Z;

	Modify();
	SetActorRotation(Rotation);
	SetActorLocation(GridFootprint::CentreFromMinCell(*FoundGrid, SnappedMin, WorldFootprint, SnapFloorZ));
}

void APuzzleBlock::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshVisual();
	PostEditMove(true);
}

#endif
