// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleRegion.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 테두리 막대의 굵기와 높이(cm). 순전히 에디터에서 눈에 띄기 위한 값이다. */
	constexpr double BorderThickness = 12.0;
	constexpr double BorderHeight = 60.0;
	constexpr int32 NumBorders = 4;
}

APuzzleRegion::APuzzleRegion()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	BorderMeshes.Reserve(NumBorders);
	for (int32 Index = 0; Index < NumBorders; ++Index)
	{
		UStaticMeshComponent* Border = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("Border%d"), Index));
		Border->SetupAttachment(SceneRoot);

		// 구간은 저작 보조물이다. 콜리전이 있으면 커서 트레이스가 테두리를 짚어 그 뒤 바닥
		// 셀을 클릭할 수 없게 되고, 그리드 생성도 테두리를 지오메트리로 굽는다.
		Border->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Border->SetCollisionResponseToAllChannels(ECR_Ignore);
		Border->SetGenerateOverlapEvents(false);
		Border->SetHiddenInGame(true);

		if (CubeFinder.Succeeded())
		{
			Border->SetStaticMesh(CubeFinder.Object);
		}
		if (MaterialFinder.Succeeded())
		{
			Border->SetMaterial(0, MaterialFinder.Object);
		}

		BorderMeshes.Add(Border);
	}

	Tags.Add(LTTSGrid::GenerationIgnoreTag());

	// 퍼즐 조각과 같은 이유다: 구간이 언로드되면 남은 블록의 이동 제한이 조용히 사라진다.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

FString APuzzleRegion::GetDisplayName() const
{
	return RegionName.IsNone() ? GetName() : RegionName.ToString();
}

void APuzzleRegion::RefreshVisual()
{
	const double CellSize = Grid ? Grid->CellSize : 100.0;
	const double SpanX = SizeInCells.X * CellSize;
	const double SpanY = SizeInCells.Y * CellSize;

	// 순서: 남(-Y), 북(+Y), 서(-X), 동(+X).
	const FVector Locations[NumBorders] = {
		FVector(0.0, -SpanY * 0.5, BorderHeight * 0.5),
		FVector(0.0, SpanY * 0.5, BorderHeight * 0.5),
		FVector(-SpanX * 0.5, 0.0, BorderHeight * 0.5),
		FVector(SpanX * 0.5, 0.0, BorderHeight * 0.5)
	};

	const FVector Scales[NumBorders] = {
		FVector(SpanX / 100.0, BorderThickness / 100.0, BorderHeight / 100.0),
		FVector(SpanX / 100.0, BorderThickness / 100.0, BorderHeight / 100.0),
		FVector(BorderThickness / 100.0, SpanY / 100.0, BorderHeight / 100.0),
		FVector(BorderThickness / 100.0, SpanY / 100.0, BorderHeight / 100.0)
	};

	for (int32 Index = 0; Index < BorderMeshes.Num() && Index < NumBorders; ++Index)
	{
		if (UStaticMeshComponent* Border = BorderMeshes[Index])
		{
			Border->SetRelativeLocation(Locations[Index]);
			Border->SetRelativeScale3D(Scales[Index]);
		}
	}
}

void APuzzleRegion::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
}

void APuzzleRegion::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: no AGridActor in the level; the puzzle region is disabled."), *GetName());
		bDisabled = true;
		return;
	}

	Region = FGridRect(
		GridFootprint::MinCellFromCentre(*Grid, GetActorLocation(), SizeInCells), SizeInCells);

	// 구간은 걸을 수 있는 셀만 담을 필요가 없다 -- 벽이나 기둥을 안에 두는 것이 오히려
	// 정상적인 퍼즐이다. 다만 그리드 밖으로 나가면 소속 판정이 무의미해지므로 그때는 끈다.
	if (!Grid->IsValidCell(Region.Min) || !Grid->IsValidCell(Region.MaxInclusive()))
	{
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: rect (%d,%d)-(%d,%d) leaves the grid; the puzzle region is disabled."),
			*GetName(), Region.Min.X, Region.Min.Y, Region.MaxInclusive().X, Region.MaxInclusive().Y);
		bDisabled = true;
		return;
	}

	SetActorLocation(GridFootprint::CentreFromMinCell(
		*Grid, Region.Min, SizeInCells, Grid->CellToWorld(Region.Min).Z));
	RefreshVisual();

	// 겹치는 구간은 블록이 어느 쪽에 속하는지를 등록 순서에 맡기게 된다.
	for (TActorIterator<APuzzleRegion> It(GetWorld()); It; ++It)
	{
		const APuzzleRegion* Other = *It;
		if (Other && Other != this && !Other->bDisabled && Other->Region.Overlaps(Region))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: overlaps puzzle region %s. Regions must be disjoint."),
				*GetName(), *Other->GetName());
		}
	}

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->RegisterRegion(this);
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: puzzle region '%s' %dx%d at cell (%d,%d), clamping %s."),
		*GetName(), *GetDisplayName(), SizeInCells.X, SizeInCells.Y, Region.Min.X, Region.Min.Y,
		bClampBlocks ? TEXT("on") : TEXT("off"));
}

void APuzzleRegion::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->UnregisterRegion(this);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- 에디터

#if WITH_EDITOR

void APuzzleRegion::PostEditMove(bool bFinished)
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

	const FIntPoint SnappedMin = GridFootprint::MinCellFromCentre(*FoundGrid, GetActorLocation(), SizeInCells);
	if (!FoundGrid->IsValidCell(SnappedMin))
	{
		return;
	}

	// 회전은 무의미하다: 구간은 축에 정렬된 셀 사각형이고, 돌려 놓으면 테두리 미리보기만
	// 실제 영역과 어긋난다.
	Modify();
	SetActorRotation(FRotator::ZeroRotator);
	SetActorLocation(GridFootprint::CentreFromMinCell(
		*FoundGrid, SnappedMin, SizeInCells, FoundGrid->CellToWorld(SnappedMin).Z));
}

void APuzzleRegion::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
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
