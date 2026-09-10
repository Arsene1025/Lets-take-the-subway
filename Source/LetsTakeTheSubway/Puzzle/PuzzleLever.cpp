// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleLever.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridFootprint.h"
#include "Grid/GridTypes.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleRotatingObstacle.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APuzzleLever::APuzzleLever()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostMesh"));
	PostMesh->SetupAttachment(SceneRoot);

	WheelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WheelPivot"));
	WheelPivot->SetupAttachment(SceneRoot);

	WheelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelMesh"));
	WheelMesh->SetupAttachment(WheelPivot);

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(WheelPivot);

	// 퍼즐 블록과 같은 콜리전 규칙: 클릭 트레이스에만 보이고 그 외에는 아무것에도 잡히지
	// 않는다. 그래서 레버가 폰이나 물리에 영향을 주지 않으면서도 커서로 휠을 잡을 수 있다.
	for (UStaticMeshComponent* Mesh : { PostMesh.Get(), WheelMesh.Get(), HandleMesh.Get() })
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Mesh->SetGenerateOverlapEvents(false);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	if (CubeFinder.Succeeded())
	{
		PostMesh->SetStaticMesh(CubeFinder.Object);
		HandleMesh->SetStaticMesh(CubeFinder.Object);
	}
	if (CylinderFinder.Succeeded())
	{
		WheelMesh->SetStaticMesh(CylinderFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PostMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WheelMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	if (PostMaterialFinder.Succeeded())
	{
		PostMesh->SetMaterial(0, PostMaterialFinder.Object);
	}
	if (WheelMaterialFinder.Succeeded())
	{
		WheelMesh->SetMaterial(0, WheelMaterialFinder.Object);
		HandleMesh->SetMaterial(0, WheelMaterialFinder.Object);
	}

	// 그리드는 레버가 아니라 레버가 서 있는 바닥을 트레이스한다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

// ---------------------------------------------------------------------------- 비주얼

void APuzzleLever::RefreshVisual()
{
	constexpr double PostWidth = 22.0;
	constexpr double WheelThickness = 12.0;

	if (PostMesh)
	{
		PostMesh->SetRelativeLocation(FVector(0.0, 0.0, PostHeight * 0.5));
		PostMesh->SetRelativeScale3D(FVector(PostWidth / 100.0, PostWidth / 100.0, PostHeight / 100.0));
	}

	if (WheelPivot)
	{
		WheelPivot->SetRelativeLocation(FVector(0.0, 0.0, PostHeight));
	}

	if (WheelMesh)
	{
		// 엔진 실린더는 지름 100 cm, 높이 100 cm이며 원점이 중심이다.
		WheelMesh->SetRelativeLocation(FVector::ZeroVector);
		WheelMesh->SetRelativeScale3D(FVector(
			WheelDiameter / 100.0, WheelDiameter / 100.0, WheelThickness / 100.0));
	}

	if (HandleMesh)
	{
		const double Radius = WheelDiameter * 0.5;
		HandleMesh->SetRelativeLocation(FVector(Radius * 0.7, 0.0, WheelThickness * 0.5 + 8.0));
		HandleMesh->SetRelativeScale3D(FVector(0.16, 0.16, 0.16));
	}
}

void APuzzleLever::SetWheelPreviewAngle(float Degrees)
{
	if (WheelPivot)
	{
		WheelPivot->SetRelativeRotation(FRotator(0.0, Degrees, 0.0));
	}
}

FVector APuzzleLever::GetWheelWorldLocation() const
{
	return WheelPivot ? WheelPivot->GetComponentLocation() : GetActorLocation();
}

// ---------------------------------------------------------------------------- 조회

void APuzzleLever::GetOperatingCells(TArray<FIntPoint>& OutCells) const
{
	static const EGridDirection Directions[4] = {
		EGridDirection::North, EGridDirection::East, EGridDirection::South, EGridDirection::West };

	for (const EGridDirection Dir : Directions)
	{
		OutCells.Add(Cell + LTTSGrid::DirOffset(Dir));
	}
}

bool APuzzleLever::IsPawnAdjacent(const AGridPawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}

	TArray<FIntPoint> Cells;
	GetOperatingCells(Cells);
	return Cells.Contains(Pawn->GetCurrentCell());
}

// ---------------------------------------------------------------------------- 사용

bool APuzzleLever::TryTurn(int32 TurnSign, const AGridPawn* Pawn, FText* OutReason)
{
	if (!Target)
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "LeverNoTarget", "This lever is not wired to anything.");
		}
		return false;
	}

	if (!IsPawnAdjacent(Pawn))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "LeverOutOfReach", "Stand next to the lever to work it.");
		}
		return false;
	}

	// 방향이 고정된 레버는 반대쪽 요청을 장애물까지 내려보내지 않는다. 이것은 장애물이 돌 수
	// 있느냐의 문제가 아니라 이 휠이 그쪽으로는 안 돌아간다는 사실이므로, 사유도 휠을 가리킨다.
	if (!AllowsTurn(TurnSign))
	{
		if (OutReason)
		{
			*OutReason = (Direction == EPuzzleRotationDirection::Clockwise)
				? NSLOCTEXT("LTTSPuzzle", "LeverOnlyClockwise", "This wheel only turns clockwise.")
				: NSLOCTEXT("LTTSPuzzle", "LeverOnlyCounterClockwise", "This wheel only turns counter-clockwise.");
		}
		return false;
	}

	return Target->TryRotate(TurnSign, OutReason);
}

// ---------------------------------------------------------------------------- 생명주기

void APuzzleLever::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
}

void APuzzleLever::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; the lever is disabled."), *GetName());
		return;
	}

	Cell = GridFootprint::MinCellFromCentre(*Grid, GetActorLocation(), FIntPoint(1, 1));

	if (!Grid->IsValidCell(Cell))
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: cell (%d,%d) is outside the grid."), *GetName(), Cell.X, Cell.Y);
		return;
	}

	// 레버는 플레이어가 걸어가서 다가가는 대상이므로, 통과해 지나가는 대신 다른 장애물처럼
	// 자기 셀을 점유한다.
	Grid->SetOccupant(Cell, this);

	SetActorLocation(GridFootprint::CentreFromMinCell(
		*Grid, Cell, FIntPoint(1, 1), Grid->CellToWorld(Cell).Z));
	RefreshVisual();

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->RegisterLever(this);
	}

	if (!Target)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: no target obstacle set; the lever will do nothing."), *GetName());
	}

	// 아무도 닿을 수 없는 레버는 플레이어가 걸어가 봐야 드러나는 레벨 버그이므로, 시작
	// 시점에 미리 알려 둘 가치가 있다.
	TArray<FIntPoint> Operating;
	GetOperatingCells(Operating);

	int32 Reachable = 0;
	for (const FIntPoint& Neighbour : Operating)
	{
		if (Grid->IsValidCell(Neighbour) && Grid->IsCellWalkableStatic(Neighbour) && !Grid->IsCellOccupied(Neighbour))
		{
			++Reachable;
		}
	}

	if (Reachable == 0)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: no free floor beside it, so the lever cannot be worked."), *GetName());
	}

	const TCHAR* DirectionText = (Direction == EPuzzleRotationDirection::Free)
		? TEXT("either way")
		: LTTSPuzzle::DescribeTurnSign(LTTSPuzzle::ResolveTurnSign(Direction, 1));

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: lever at cell (%d,%d), target %s, turns %s, %d operating cell(s)."),
		*GetName(), Cell.X, Cell.Y, *GetNameSafe(Target), DirectionText, Reachable);
}

void APuzzleLever::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Grid)
	{
		Grid->ClearAllOccupantsOf(this);
	}

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->UnregisterLever(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- 에디터

#if WITH_EDITOR

void APuzzleLever::PostEditMove(bool bFinished)
{
	// 클래스 기본값(CDO)과 블루프린트 템플릿에는 월드도, 배치된 트랜스폼도 없다.
	// 블루프린트 에디터의 Class Defaults를 편집하는 것도 이 경로를 지나므로, 여기서
	// 막지 않으면 그리드를 찾아 스냅하려다 에디터가 죽는다.
	if (IsTemplate())
	{
		return;
	}

	Super::PostEditMove(bFinished);
	if (!bFinished)
	{
		return;
	}

	const AGridActor* FoundGrid = AGridActor::FindGrid(GetWorld());
	if (!FoundGrid)
	{
		return;
	}

	const FIntPoint Snapped = GridFootprint::MinCellFromCentre(*FoundGrid, GetActorLocation(), FIntPoint(1, 1));
	if (!FoundGrid->IsValidCell(Snapped))
	{
		return;
	}

	Modify();
	SetActorRotation(FRotator::ZeroRotator);
	SetActorLocation(GridFootprint::CentreFromMinCell(
		*FoundGrid, Snapped, FIntPoint(1, 1), FoundGrid->CellToWorld(Snapped).Z));
}

void APuzzleLever::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 클래스 기본값(CDO)과 블루프린트 템플릿에는 월드도, 배치된 트랜스폼도 없다.
	// 블루프린트 에디터의 Class Defaults를 편집하는 것도 이 경로를 지나므로, 여기서
	// 막지 않으면 그리드를 찾아 스냅하려다 에디터가 죽는다.
	if (IsTemplate())
	{
		return;
	}

	RefreshVisual();
	PostEditMove(true);
}

#endif
