// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleBlock.h"

#include "LetsTakeTheSubway.h"
#include "Art/ArtMaterialUtil.h"
#include "Grid/GridActor.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleRegion.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

	// 아트 액터 자리. 클래스를 지정하기 전에는 비어 있어 아무 비용도 들지 않는다.
	VisualActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("VisualActor"));
	VisualActor->SetupAttachment(SceneRoot);

	// 아트 스태틱 메시 자리. 메시를 지정하기 전에는 아무것도 그리지 않는다.
	//
	// 콜리전을 여기서 한 번 끄고 다시는 켜지 않는다. 커서 판정과 그리드 트레이스는 프록시
	// 큐브가 전담해야 조각을 잡는 규칙이 아트 메시의 모양에 좌우되지 않는다.
	ArtMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArtMesh"));
	ArtMeshComponent->SetupAttachment(SceneRoot);
	ArtMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArtMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArtMeshComponent->SetGenerateOverlapEvents(false);

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
	// 고정된 조각은 축이 없다. 드래그 코드와 HUD가 모두 이 값을 읽으므로, 여기서 한 번
	// 답해 두면 "밀 수 없다"가 잡는 순간부터 일관되게 보인다.
	if (!bCanMove)
	{
		return EPuzzleMoveAxis::None;
	}

	return LTTSPuzzle::RotateAxis(MoveAxis, QuarterTurns);
}

FIntPoint APuzzleBlock::LocalToWorldOffset(FIntPoint Local) const
{
	FIntPoint Point = Local;
	FIntPoint Size = FootprintSize;

	// W x H 사각형을 한 번 돌리면 로컬 (x, y)는 (H-1-y, x)로 가고 두 변이 맞바뀐다.
	for (int32 Turn = 0; Turn < GetQuarterTurns(); ++Turn)
	{
		Point = FIntPoint(Size.Y - 1 - Point.Y, Point.X);
		Size = FIntPoint(Size.Y, Size.X);
	}

	return Point;
}

FGridRect APuzzleBlock::GetWorldHollowRect() const
{
	if (!HasHollow())
	{
		return FGridRect(GetRect().Min, FIntPoint::ZeroValue);
	}

	const FIntPoint LowLocal = LocalToWorldOffset(HollowOffset);
	const FIntPoint HighLocal = LocalToWorldOffset(HollowOffset + HollowSize - FIntPoint(1, 1));

	// 양 끝 모서리를 모두 변환한 뒤 다시 합친다: 홀수 번 돌면 둘의 역할이 뒤바뀌므로,
	// 성분별 최솟값을 취하면 특수 처리 없이 어느 방향에서도 맞는다.
	const FIntPoint MinLocal(FMath::Min(LowLocal.X, HighLocal.X), FMath::Min(LowLocal.Y, HighLocal.Y));
	const FIntPoint Size = (GetQuarterTurns() % 2 == 0)
		? HollowSize
		: FIntPoint(HollowSize.Y, HollowSize.X);

	return FGridRect(GetRect().Min + MinLocal, Size);
}

void APuzzleBlock::GatherOccupiedCells(TArray<FIntPoint>& OutCells) const
{
	if (!HasHollow())
	{
		GetRect().GatherCells(OutCells);
		return;
	}

	// 빈 영역은 일부러 점유하지 않는다. L자 벤치가 감싸고 있는 기둥이 그 자리에 선다.
	const FGridRect Hollow = GetWorldHollowRect();

	TArray<FIntPoint> All;
	GetRect().GatherCells(All);

	OutCells.Reserve(OutCells.Num() + All.Num());
	for (const FIntPoint& Cell : All)
	{
		if (!Hollow.Contains(Cell))
		{
			OutCells.Add(Cell);
		}
	}
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

void APuzzleBlock::SanitiseVisualActor()
{
	AActor* Child = VisualActor ? VisualActor->GetChildActor() : nullptr;
	if (!Child)
	{
		return;
	}

	// 그리드 생성이 아트를 바닥으로 구우면 조각이 우연히 놓인 자리가 지형으로 굳는다.
	Child->Tags.AddUnique(LTTSGrid::GenerationIgnoreTag());

	// 커서 판정은 그레이박스 프록시가 맡는다. 아트가 트레이스를 가로채면 조각을 잡는
	// 규칙이 메시 모양에 따라 달라지고, 아트가 바뀔 때마다 조작감이 흔들린다.
	TArray<UPrimitiveComponent*> Primitives;
	Child->GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetCollisionResponseToAllChannels(ECR_Ignore);
		Primitive->SetGenerateOverlapEvents(false);
	}
}

void APuzzleBlock::RefreshVisual()
{
	const bool bArt = IsUsingArtVisual();

	if (VisualActor)
	{
		if (VisualActor->GetChildActorClass() != VisualActorClass)
		{
			VisualActor->SetChildActorClass(VisualActorClass);
		}

		// 아트 원점은 풋프린트 중심의 바닥이다. 액터 원점과 같은 규약이라 보정이 없다.
		VisualActor->SetRelativeLocation(FVector::ZeroVector);
		SanitiseVisualActor();
	}

	if (ArtMeshComponent)
	{
		if (ArtMeshComponent->GetStaticMesh() != ArtMesh)
		{
			ArtMeshComponent->SetStaticMesh(ArtMesh);
		}

		// 아트 원점은 풋프린트 중심의 바닥이다. 보정이 필요한 메시만 ArtMeshOffset을 쓴다.
		ArtMeshComponent->SetRelativeTransform(ArtMeshOffset);
		ArtMeshComponent->SetVisibility(ArtMesh != nullptr);
		LTTSArt::ReplaceDefaultMaterials(*ArtMeshComponent, ArtFallbackMaterial);
	}

	if (!BodyMesh)
	{
		return;
	}

	// 아트가 붙으면 큐브는 보이지 않게만 하고 콜리전은 남긴다. 눈에 보이는 것은 아트이고,
	// 커서가 잡는 것은 언제나 풋프린트와 정확히 같은 이 상자다.
	BodyMesh->SetVisibility(!bArt);

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

void APuzzleBlock::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// 자식 아트 액터는 컴포넌트 등록 때 생기고, 그리드는 어떤 BeginPlay보다 먼저
	// (GridActor::PostInitializeComponents) 다시 구워진다. 그 사이에 아트가 콜리전을 켠 채
	// 남아 있지 않도록 여기서 먼저 정리한다. BeginPlay의 RefreshVisual이 한 번 더 하지만
	// 그때는 이미 늦다.
	SanitiseVisualActor();
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

	// 소속 구간은 첫 틱에 정한다. 이유는 ResolveHomeRegion의 주석을 보라.

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

	// 조각 자신의 사정을 먼저 본다. 누가 타고 있는 엘리베이터는 어느 방향이든 못 움직이므로,
	// 축이나 목적지를 따지기 전에 답이 정해진다.
	if (!CanStartMoving(OutReason))
	{
		return false;
	}

	// 고정된 조각은 축을 따지기 전에 끝난다. 사유를 축 문구와 나누는 이유는 두 상황이
	// 플레이어에게 다른 뜻이기 때문이다: 하나는 "다른 쪽으로 밀어라"이고 다른 하나는
	// "이 물건은 밀리지 않는다"이다.
	if (!bCanMove)
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockFixed", "This object is fixed in place.");
		}
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

	const FIntPoint Delta = LTTSGrid::DirOffset(Dir);
	const FGridRect Target(MinCell + Delta, GetWorldFootprint());

	if (!CanOccupyRect(Target, OutReason))
	{
		return false;
	}

	// 구간 경계는 바닥이나 점유보다 먼저 본다. 퍼즐의 규칙이지 지형 사정이 아니므로,
	// 플레이어에게 "여기까지가 이 퍼즐이다"라고 말해 주는 편이 "바닥이 없다"보다 정확하다.
	if (const APuzzleRegion* Region = HomeRegion.Get())
	{
		if (Region->bClampBlocks && !Region->ContainsRect(Target))
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "BlockLeavesRegion", "This piece cannot leave the puzzle area.");
			}
			return false;
		}
	}

	// 목적지에서 **실제로 점유할** 셀을 본다. 사각형 전체가 아니다.
	//
	// 슬라이드는 회전 없는 평행이동이므로, 지금 점유한 셀을 그대로 옮기면 그것이 목적지의
	// 점유 셀이다. 그리고 그 목록은 GatherOccupiedCells가 이미 빈 영역과 홈을 빼고 만든
	// 것이라, 파생 클래스마다 다른 모양(L자 벤치의 빈 자리, 회전 장애물의 채널)이 저절로
	// 따라온다.
	//
	// 사각형으로 따지던 시절에는 L자 벤치가 감싼 기둥이 자기 빈 자리에 서 있는데도
	// "무언가 가로막고 있다"가 되어 어느 방향으로도 밀리지 않았다. 점유를 등록할 때와
	// 이동을 판정할 때가 서로 다른 규칙을 쓰고 있었던 것이다.
	TArray<FIntPoint> Cells;
	GatherOccupiedCells(Cells);

	for (FIntPoint& Cell : Cells)
	{
		Cell += Delta;
	}

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

void APuzzleBlock::ResolveHomeRegion()
{
	bHomeRegionResolved = true;

	const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	// 구간이 하나도 없는 레벨(러쉬아워·회전 장애물 프로토타입)은 지금까지처럼 제한 없이
	// 동작해야 하므로, 못 찾은 것은 오류가 아니라 안내다.
	HomeRegion = Subsystem->FindRegionContaining(GetRect());

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: region %s."), *GetName(),
		HomeRegion.IsValid() ? *HomeRegion->GetDisplayName() : TEXT("none (movement unrestricted)"));
}

void APuzzleBlock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bHomeRegionResolved)
	{
		ResolveHomeRegion();
	}

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

// ---------------------------------------------------------------------------- 콘솔
//
// 블록 밀기는 마우스 드래그로만 시작된다. 그래서 "이 조각이 저쪽으로 밀리는가"는 자동으로
// 확인할 수 없는 질문이었다 -- 빈 영역을 감싼 조각처럼 판정이 미묘한 경우일수록 더 그랬다.
// 열차의 ltts.TrainArrive, 엘리베이터의 ltts.ElevatorRide와 같은 자리다.

namespace
{
	bool ParseGridDirection(const FString& Text, EGridDirection& OutDir)
	{
		const FString Upper = Text.ToUpper();

		if (Upper.StartsWith(TEXT("N"))) { OutDir = EGridDirection::North; return true; }
		if (Upper.StartsWith(TEXT("E"))) { OutDir = EGridDirection::East;  return true; }
		if (Upper.StartsWith(TEXT("S"))) { OutDir = EGridDirection::South; return true; }
		if (Upper.StartsWith(TEXT("W"))) { OutDir = EGridDirection::West;  return true; }

		return false;
	}

	void BlockSlideCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.BlockSlide: run this in play mode."));
			return;
		}

		if (Args.Num() < 2)
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("ltts.BlockSlide: usage is ltts.BlockSlide <name substring> <N|E|S|W>"));
			return;
		}

		EGridDirection Dir = EGridDirection::North;
		if (!ParseGridDirection(Args[1], Dir))
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.BlockSlide: '%s' is not N, E, S or W."), *Args[1]);
			return;
		}

		for (TActorIterator<APuzzleBlock> It(World); It; ++It)
		{
			APuzzleBlock* Block = *It;
			if (!Block
				|| (!Block->GetName().Contains(Args[0]) && !Block->GetActorNameOrLabel().Contains(Args[0])))
			{
				continue;
			}

			// 거부 사유를 먼저 물어 두면, 밀리지 않았을 때 왜인지가 로그에 남는다.
			FText Reason;
			const bool bAllowed = Block->CanSlide(Dir, &Reason);
			const bool bMoved = bAllowed && Block->StartSlide(Dir);

			UE_LOG(LogLTTSGrid, Display,
				TEXT("ltts.BlockSlide: %s %s -> %s%s"),
				*Block->GetActorNameOrLabel(),
				*StaticEnum<EGridDirection>()->GetNameStringByValue(static_cast<int64>(Dir)),
				bMoved ? TEXT("moving") : TEXT("refused"),
				bMoved ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Reason.ToString()));
			return;
		}

		UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.BlockSlide: no block matching '%s'."), *Args[0]);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBlockSlideCommand(
	TEXT("ltts.BlockSlide"),
	TEXT("Push a puzzle block one cell as a drag would: ltts.BlockSlide <name substring> <N|E|S|W>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BlockSlideCommand));
