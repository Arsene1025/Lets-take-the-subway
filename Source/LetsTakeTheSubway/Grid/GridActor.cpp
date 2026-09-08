// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/GridActor.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridCellRule.h"
#include "Grid/GridDebugDrawComponent.h"
#include "Grid/GridPathfinder.h"
#include "Authoring/GridCellMarker.h"

#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

namespace
{
	/**
	 * 이 폰이 셀 점유자를 통과하는가.
	 *
	 * 태그 하나로 판정하므로 그리드는 어떤 NPC 클래스도 알 필요가 없고, CanPawnEnter를
	 * 거치는 길찾기 · 매 걸음 재검사 · 진입 셀 탐색이 저절로 같은 답을 낸다.
	 */
	bool PawnPassesThroughOccupants(const APawn* Pawn)
	{
		return Pawn != nullptr && Pawn->ActorHasTag(LTTSGrid::PassThroughOccupantsTag());
	}
}

AGridActor::AGridActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DebugDrawComponent = CreateDefaultSubobject<UGridDebugDrawComponent>(TEXT("GridDebugDraw"));
	DebugDrawComponent->SetupAttachment(SceneRoot);
}

// ---------------------------------------------------------------------------- 변환

FIntPoint AGridActor::WorldToCell(const FVector& World) const
{
	const FVector Local = World - GetGridOrigin();
	return FIntPoint(FMath::FloorToInt32(Local.X / CellSize), FMath::FloorToInt32(Local.Y / CellSize));
}

FVector AGridActor::CellToWorld(FIntPoint Cell) const
{
	const FVector Origin = GetGridOrigin();
	const FGridCellData* Data = GetCell(Cell);
	const double Z = (Data && Data->Type != EGridCellType::NoFloor) ? Data->FloorZ : Origin.Z;

	return FVector(
		Origin.X + (Cell.X + 0.5) * CellSize,
		Origin.Y + (Cell.Y + 0.5) * CellSize,
		Z);
}

// ---------------------------------------------------------------------------- 걸을 수 있음 판정

bool AGridActor::IsCellWalkableStatic(FIntPoint Cell) const
{
	const FGridCellData* Data = GetCell(Cell);
	if (!Data)
	{
		return false;
	}

	return Data->Type == EGridCellType::Walkable
		|| Data->Type == EGridCellType::StageClear
		|| Data->Type == EGridCellType::Conditional;
}

// ---------------------------------------------------------------------------- 점유

bool AGridActor::SetOccupant(FIntPoint Cell, AActor* Occupant)
{
	if (!Occupant || !IsValidCell(Cell))
	{
		return false;
	}

	const int32 Index = CellToIndex(Cell);
	if (const TWeakObjectPtr<AActor>* Existing = Occupants.Find(Index))
	{
		const AActor* Holder = Existing->Get();
		if (Holder && Holder != Occupant)
		{
			return false;
		}
	}

	Occupants.Add(Index, Occupant);
	return true;
}

void AGridActor::ClearOccupant(FIntPoint Cell, const AActor* Expected)
{
	if (!IsValidCell(Cell))
	{
		return;
	}

	const int32 Index = CellToIndex(Cell);
	if (const TWeakObjectPtr<AActor>* Existing = Occupants.Find(Index))
	{
		// 오래된 항목(점유자가 파괴됨)은 누구든 지울 수 있다. 그래야 레벨을 떠난 블록이
		// 셀을 영구히 걸을 수 없는 상태로 남겨 두지 못한다.
		const AActor* Holder = Existing->Get();
		if (!Holder || Holder == Expected)
		{
			Occupants.Remove(Index);
		}
	}
}

void AGridActor::ClearAllOccupantsOf(const AActor* Occupant)
{
	for (auto It = Occupants.CreateIterator(); It; ++It)
	{
		const AActor* Holder = It.Value().Get();
		if (!Holder || Holder == Occupant)
		{
			It.RemoveCurrent();
		}
	}
}

AActor* AGridActor::GetOccupant(FIntPoint Cell) const
{
	if (!IsValidCell(Cell))
	{
		return nullptr;
	}

	const TWeakObjectPtr<AActor>* Existing = Occupants.Find(CellToIndex(Cell));
	return Existing ? Existing->Get() : nullptr;
}

bool AGridActor::IsCellOccupied(FIntPoint Cell, const AActor* Ignore) const
{
	const AActor* Holder = GetOccupant(Cell);
	return Holder != nullptr && Holder != Ignore;
}

bool AGridActor::CanPawnEnter(FIntPoint Cell, const APawn* Pawn, FText* OutDeniedMessage) const
{
	const FGridCellData* Data = GetCell(Cell);
	if (!Data || !IsCellWalkableStatic(Cell))
	{
		if (OutDeniedMessage)
		{
			*OutDeniedMessage = DescribeCell(Cell, Pawn);
		}
		return false;
	}

	// Conditional 규칙보다 먼저 검사한다. 어차피 블록이 서 있는 셀에 대해서는 규칙이 절대
	// 실행되지 않도록 -- 규칙은 경로 탐색 한 번에 여러 번 호출된다.
	// 통과 태그가 붙은 폰(행인 NPC)에게는 점유자가 없는 것과 같다.
	if (!PawnPassesThroughOccupants(Pawn) && IsCellOccupied(Cell))
	{
		if (OutDeniedMessage)
		{
			*OutDeniedMessage = DescribeCell(Cell, Pawn);
		}
		return false;
	}

	if (Data->Type == EGridCellType::Conditional && ConditionalRules.IsValidIndex(Data->RuleIndex))
	{
		const UGridCellRule* Rule = ConditionalRules[Data->RuleIndex];
		if (Rule && !Rule->CanEnter(Pawn))
		{
			if (OutDeniedMessage)
			{
				*OutDeniedMessage = Rule->GetDeniedMessage();
			}
			return false;
		}
	}

	return true;
}

FText AGridActor::DescribeCell(FIntPoint Cell, const APawn* Pawn) const
{
	const FGridCellData* Data = GetCell(Cell);
	if (!Data)
	{
		return NSLOCTEXT("LTTSGrid", "CellOffGrid", "Outside the grid.");
	}

	if (!IsCellWalkableStatic(Cell))
	{
		const UEnum* ReasonEnum = StaticEnum<EGridBlockReason>();
		return ReasonEnum
			? ReasonEnum->GetDisplayNameTextByValue(static_cast<int64>(Data->BlockReason))
			: NSLOCTEXT("LTTSGrid", "CellBlocked", "Blocked.");
	}

	// 셀이 아니라 enum에서 직접 가져와 보고한다: 이유가 런타임 점유자이므로 BlockReason에는
	// 절대 저장되지 않는다. CanPawnEnter와 같은 조건이어야 거부 문구가 실제 거부 사유와 맞는다.
	if (!PawnPassesThroughOccupants(Pawn) && IsCellOccupied(Cell))
	{
		const UEnum* ReasonEnum = StaticEnum<EGridBlockReason>();
		return ReasonEnum
			? ReasonEnum->GetDisplayNameTextByValue(static_cast<int64>(EGridBlockReason::Object))
			: NSLOCTEXT("LTTSGrid", "CellOccupied", "Blocked by object.");
	}

	if (Data->Type == EGridCellType::Conditional && ConditionalRules.IsValidIndex(Data->RuleIndex))
	{
		if (const UGridCellRule* Rule = ConditionalRules[Data->RuleIndex])
		{
			if (!Rule->CanEnter(Pawn))
			{
				return Rule->GetDeniedMessage();
			}
		}
	}

	return FText::GetEmpty();
}

bool AGridActor::FindNearestWalkableCell(FIntPoint From, int32 MaxRadius, const APawn* Pawn, FIntPoint& OutCell) const
{
	if (CanPawnEnter(From, Pawn))
	{
		OutCell = From;
		return true;
	}

	// 정사각형 링을 넓혀 간다. 거리 기준의 엄밀한 최근접 탐색은 아니지만, 균일한 그리드에서는
	// 차이가 한 셀을 넘지 않고 정렬도 필요 없다.
	for (int32 Radius = 1; Radius <= MaxRadius; ++Radius)
	{
		for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
		{
			for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
			{
				if (FMath::Max(FMath::Abs(OffsetX), FMath::Abs(OffsetY)) != Radius)
				{
					continue;	// 링 안쪽은 더 작은 반경에서 이미 다뤘다
				}

				const FIntPoint Candidate(From.X + OffsetX, From.Y + OffsetY);
				if (CanPawnEnter(Candidate, Pawn))
				{
					OutCell = Candidate;
					return true;
				}
			}
		}
	}

	return false;
}

bool AGridActor::FindEntryCell(const FVector& FromWorld, int32 MaxRadius, const APawn* Pawn,
	const TOptional<FIntPoint>& Goal, FIntPoint& OutCell) const
{
	const FIntPoint FromCell = WorldToCell(FromWorld);
	if (CanPawnEnter(FromCell, Pawn))
	{
		OutCell = FromCell;
		return true;
	}

	for (int32 Radius = 1; Radius <= MaxRadius; ++Radius)
	{
		FIntPoint Best = FIntPoint::ZeroValue;
		double BestDistanceSq = TNumericLimits<double>::Max();
		bool bFound = false;

		for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
		{
			for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
			{
				if (FMath::Max(FMath::Abs(OffsetX), FMath::Abs(OffsetY)) != Radius)
				{
					continue;	// 링 안쪽은 더 작은 반경에서 이미 다뤘다
				}

				const FIntPoint Candidate(FromCell.X + OffsetX, FromCell.Y + OffsetY);
				if (!CanPawnEnter(Candidate, Pawn))
				{
					continue;
				}

				// 목적지가 있으면 거기까지 이어지는 셀만 받는다. 그러지 않으면 선로 건너편
				// 같은 고립된 섬으로 걸어 들어가 영영 못 나온다.
				if (Goal.IsSet() && Candidate != Goal.GetValue())
				{
					TArray<FIntPoint> Probe;
					if (!FindPath(Candidate, Goal.GetValue(), Pawn, Probe))
					{
						continue;
					}
				}

				const double DistanceSq = FVector::DistSquared2D(CellToWorld(Candidate), FromWorld);
				if (DistanceSq < BestDistanceSq)
				{
					BestDistanceSq = DistanceSq;
					Best = Candidate;
					bFound = true;
				}
			}
		}

		// 한 반경 안에서 최선을 고르고 끝낸다. 더 넓은 링에는 더 가까운 셀이 있을 수 없다.
		if (bFound)
		{
			OutCell = Best;
			return true;
		}
	}

	return false;
}

bool AGridActor::FindPath(FIntPoint Start, FIntPoint Goal, const APawn* Pawn, TArray<FIntPoint>& OutPath) const
{
	return FGridPathfinder::FindPath(*this, Start, Goal, Pawn, OutPath);
}

void AGridActor::NotifyPawnEnteredCell(APawn* Pawn, FIntPoint Cell)
{
	const FGridCellData* Data = GetCell(Cell);
	if (Data && Data->Type == EGridCellType::StageClear)
	{
		UE_LOG(LogLTTSGrid, Display, TEXT("Stage clear cell (%d,%d) reached by %s."),
			Cell.X, Cell.Y, *GetNameSafe(Pawn));
		OnStageClear.Broadcast(Pawn, Cell);
	}
}

AGridActor* AGridActor::FindGrid(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGridActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

// ---------------------------------------------------------------------------- 생성

void AGridActor::GenerateFromTraces()
{
	const int32 Width = SizeInCells.X;
	const int32 Height = SizeInCells.Y;
	const int32 NumCells = Width * Height;

	Cells.Reset();
	Cells.SetNum(NumCells);

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no world, cannot generate."), *GetName());
		return;
	}

	const FVector Origin = GetGridOrigin();
	const double TraceTop = Origin.Z + RegionHeight;
	const double TraceBottom = Origin.Z;
	const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxSlopeAngle));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LTTSGridGen), /*bTraceComplex*/ false, this);

	// 마커에는 콜리전이 없지만, 에디터 전용 액터를 전부 무시해 두면 나중에 누군가 추가할
	// 시각화 액터까지 함께 걸러진다. 게임 월드(bRegenerateOnPlay)에서는 대신 폰을 무시해,
	// 바닥에 서 있는 폰이 자기 발밑 바닥을 가리지 못하게 한다.
	//
	// 태그가 붙은 액터는 움직이는 소품 -- 퍼즐 블록이다. 바닥 위에 서 있으므로 그냥 두면
	// 배치된 그 위치에서 자기 자신을 Blocked 셀로 구워 넣게 된다. 그래서 그 아래 바닥은
	// 블록이 없는 것처럼 트레이스해야 한다.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		if (Actor->IsEditorOnly() || Actor->IsA<APawn>() || Actor->ActorHasTag(LTTSGrid::GenerationIgnoreTag()))
		{
			Params.AddIgnoredActor(Actor);
		}
	}

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			FGridCellData& Cell = Cells[Y * Width + X];

			const double CentreX = Origin.X + (X + 0.5) * CellSize;
			const double CentreY = Origin.Y + (Y + 0.5) * CellSize;

			FHitResult Hit;
			const bool bHit = World->LineTraceSingleByChannel(
				Hit,
				FVector(CentreX, CentreY, TraceTop),
				FVector(CentreX, CentreY, TraceBottom),
				TraceChannel,
				Params);

			if (!bHit)
			{
				Cell.GeneratedType = EGridCellType::NoFloor;
				Cell.GeneratedReason = EGridBlockReason::NoFloorHit;
				continue;
			}

			// 트레이스가 지오메트리 안에서 시작했다 -- 영역 꼭대기를 관통하는 벽이나 기둥이다.
			// 여기에는 머리 위 공간이 전혀 없고, 보고된 충돌 지점과 노멀은 표면이 아니라
			// 트레이스 시작점이므로 쓸 수 없다.
			if (Hit.bStartPenetrating)
			{
				Cell.FloorZ = static_cast<float>(TraceTop);
				Cell.GeneratedType = EGridCellType::Blocked;
				Cell.GeneratedReason = EGridBlockReason::Clearance;
				continue;
			}

			Cell.FloorZ = static_cast<float>(Hit.ImpactPoint.Z);
			Cell.SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Hit.ImpactNormal.Z, -1.0, 1.0)));

			if (Hit.ImpactNormal.Z < MinNormalZ)
			{
				Cell.GeneratedType = EGridCellType::Blocked;
				Cell.GeneratedReason = EGridBlockReason::Slope;
				continue;
			}

			Cell.GeneratedType = EGridCellType::Walkable;
			Cell.GeneratedReason = EGridBlockReason::None;

			if (bClearanceTest && ClearanceHeight > MaxStepHeight)
			{
				// 박스를 MaxStepHeight 위에서 시작해, 경사로나 작은 턱이 자기 자신을 막지 못하게 한다.
				const double BoxHalfHeight = (ClearanceHeight - MaxStepHeight) * 0.5;
				const FVector BoxCentre(CentreX, CentreY, Cell.FloorZ + MaxStepHeight + BoxHalfHeight);
				const FCollisionShape Box = FCollisionShape::MakeBox(
					FVector(ClearanceHalfWidth, ClearanceHalfWidth, BoxHalfHeight));

				if (World->OverlapBlockingTestByChannel(BoxCentre, FQuat::Identity, TraceChannel, Box, Params))
				{
					Cell.GeneratedType = EGridCellType::Blocked;
					Cell.GeneratedReason = EGridBlockReason::Clearance;
				}
			}
		}
	}

	BuildAdjacency();
}

void AGridActor::BuildAdjacency()
{
	const int32 Width = SizeInCells.X;
	const int32 Height = SizeInCells.Y;

	NumStepBreaks = 0;

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			FGridCellData& Cell = Cells[Y * Width + X];
			Cell.NeighborMask = 0;
		}
	}

	// +X와 +Y만 검사한다. 연결 하나를 양쪽 끝점 모두에 기록하므로 마스크는 구조적으로
	// 대칭이고 단방향 링크는 생길 수 없다.
	//
	// 평평한 단차와 경사로는 둘 다 높이 차이로 나타나지만, 올라서야 하는
	// 불연속은 단차뿐이다. 경사로에서는 상승분이 표면 그 자체다: MaxSlopeAngle
	// 35도에서 1 m 셀은 이미 70 cm 올라가는데, 단차 규칙만으로는 이것이 끊겨
	// 모든 경사로가 서로 떨어진 띠로 잘린다. 그래서 허용치는 두 셀 중 *더 완만한*
	// 쪽의 경사에 따라 커진다 -- 경사로 셀 두 개는 경사로 자체의 상승분에 단차
	// 여유를 더해 받지만, 평지 옆의 경사로는 여전히 단차 여유만 받으므로 낙차를
	// 건널 수 없다.
	const auto MaxNeighborDelta = [this](const FGridCellData& A, const FGridCellData& B)
	{
		const float SharedSlopeDeg = FMath::Min(A.SlopeDeg, B.SlopeDeg);
		const float SlopeRise = CellSize * FMath::Tan(FMath::DegreesToRadians(SharedSlopeDeg));
		return MaxStepHeight + SlopeRise;
	};

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 Index = Y * Width + X;
			if (Cells[Index].GeneratedType == EGridCellType::NoFloor)
			{
				continue;
			}

			if (X + 1 < Width)
			{
				const int32 EastIndex = Index + 1;
				if (Cells[EastIndex].GeneratedType != EGridCellType::NoFloor)
				{
					if (FMath::Abs(Cells[Index].FloorZ - Cells[EastIndex].FloorZ) <= MaxNeighborDelta(Cells[Index], Cells[EastIndex]))
					{
						Cells[Index].NeighborMask |= EGridDir::East;
						Cells[EastIndex].NeighborMask |= EGridDir::West;
					}
					else
					{
						++NumStepBreaks;
					}
				}
			}

			if (Y + 1 < Height)
			{
				const int32 NorthIndex = Index + Width;
				if (Cells[NorthIndex].GeneratedType != EGridCellType::NoFloor)
				{
					if (FMath::Abs(Cells[Index].FloorZ - Cells[NorthIndex].FloorZ) <= MaxNeighborDelta(Cells[Index], Cells[NorthIndex]))
					{
						Cells[Index].NeighborMask |= EGridDir::North;
						Cells[NorthIndex].NeighborMask |= EGridDir::South;
					}
					else
					{
						++NumStepBreaks;
					}
				}
			}
		}
	}
}

void AGridActor::ApplyStoredOverrides()
{
	NumOverridesApplied = 0;

	// 트레이스 결과에서 다시 시작한다. 그래야 마커를 지우면 그 셀들이 정확히 원래대로 돌아간다.
	for (FGridCellData& Cell : Cells)
	{
		Cell.Type = Cell.GeneratedType;
		Cell.BlockReason = Cell.GeneratedReason;
		Cell.RuleIndex = INDEX_NONE;
	}

	for (const FGridCellOverride& Override : Overrides)
	{
		const int32 Index = CellToIndex(Override.Cell);
		if (!IsValidCell(Override.Cell) || !Cells.IsValidIndex(Index))
		{
			continue;
		}

		FGridCellData& Cell = Cells[Index];

		// 바닥이 없는 셀에는 설 수 있는 Z가 없으므로, 걸을 수 있게 만들면 폰이 그리드 원점
		// 높이로 순간이동한다. 거부하고 로그로 알린다.
		if (Cell.GeneratedType == EGridCellType::NoFloor && Override.Type != EGridCellType::Blocked)
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: override on cell (%d,%d) ignored -- that cell has no floor."),
				*GetName(), Override.Cell.X, Override.Cell.Y);
			continue;
		}

		Cell.Type = Override.Type;
		Cell.RuleIndex = Override.RuleIndex;
		Cell.BlockReason = (Override.Type == EGridCellType::Blocked)
			? EGridBlockReason::Marker
			: EGridBlockReason::None;

		++NumOverridesApplied;
	}
}

void AGridActor::RecomputeStats()
{
	NumWalkable = 0;
	NumBlocked = 0;
	NumNoFloor = 0;
	NumStageClear = 0;
	NumConditional = 0;
	NumBlockedBySlope = 0;
	NumBlockedByClearance = 0;

	for (const FGridCellData& Cell : Cells)
	{
		switch (Cell.Type)
		{
		case EGridCellType::Walkable:		++NumWalkable; break;
		case EGridCellType::Blocked:		++NumBlocked; break;
		case EGridCellType::NoFloor:		++NumNoFloor; break;
		case EGridCellType::StageClear:		++NumStageClear; break;
		case EGridCellType::Conditional:	++NumConditional; break;
		default: break;
		}

		if (Cell.BlockReason == EGridBlockReason::Slope)		{ ++NumBlockedBySlope; }
		if (Cell.BlockReason == EGridBlockReason::Clearance)	{ ++NumBlockedByClearance; }
	}
}

void AGridActor::RefreshDebugDraw()
{
	if (DebugDrawComponent)
	{
		DebugDrawComponent->MarkRenderStateDirty();
	}
}

// ---------------------------------------------------------------------------- 에디터 동작

void AGridActor::GenerateGrid()
{
	const double StartTime = FPlatformTime::Seconds();

	Modify();
	GenerateFromTraces();

#if WITH_EDITOR
	BakeOverridesFromMarkers();
#endif

	ApplyStoredOverrides();
	RecomputeStats();

	LastGenerated = FDateTime::Now().ToString();
	RefreshDebugDraw();
	MarkPackageDirty();

	const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: generated %dx%d = %d cells in %.1f ms. walkable=%d blocked=%d (slope=%d clearance=%d) noFloor=%d stageClear=%d conditional=%d overrides=%d stepBreaks=%d"),
		*GetName(), SizeInCells.X, SizeInCells.Y, Cells.Num(), ElapsedMs,
		NumWalkable, NumBlocked, NumBlockedBySlope, NumBlockedByClearance,
		NumNoFloor, NumStageClear, NumConditional, NumOverridesApplied, NumStepBreaks);
}

void AGridActor::ApplyOverrides()
{
	if (Cells.Num() != SizeInCells.X * SizeInCells.Y)
	{
		// 찍어 넣을 대상이 없다 -- 전체 생성으로 대체한다.
		GenerateGrid();
		return;
	}

	Modify();

	// 여기서는 트레이스하지 않는다: ApplyStoredOverrides가 모든 셀을 생성 상태에서 다시
	// 만들므로, 이것은 GenerateGrid의 굽고-찍는 절반에 해당한다.
#if WITH_EDITOR
	BakeOverridesFromMarkers();
#endif

	ApplyStoredOverrides();
	RecomputeStats();
	RefreshDebugDraw();
	MarkPackageDirty();

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: applied %d marker override(s)."), *GetName(), NumOverridesApplied);
}

void AGridActor::ClearGrid()
{
	Modify();
	Cells.Reset();
	Overrides.Reset();
	ConditionalRules.Reset();
	RecomputeStats();
	NumOverridesApplied = 0;
	NumStepBreaks = 0;
	LastGenerated.Reset();
	RefreshDebugDraw();
	MarkPackageDirty();

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: grid cleared."), *GetName());
}

void AGridActor::LogDebugReport()
{
	const UEnum* TypeEnum = StaticEnum<EGridCellType>();
	const UEnum* ReasonEnum = StaticEnum<EGridBlockReason>();

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s report: %dx%d cells. walkable=%d blocked=%d (slope=%d clearance=%d) noFloor=%d stageClear=%d conditional=%d overrides=%d rules=%d stepBreaks=%d generated=%s"),
		*GetName(), SizeInCells.X, SizeInCells.Y,
		NumWalkable, NumBlocked, NumBlockedBySlope, NumBlockedByClearance,
		NumNoFloor, NumStageClear, NumConditional,
		NumOverridesApplied, ConditionalRules.Num(), NumStepBreaks,
		LastGenerated.IsEmpty() ? TEXT("never") : *LastGenerated);

	if (const FGridCellData* Cell = GetCell(DebugInspectCell))
	{
		const FVector World = CellToWorld(DebugInspectCell);
		UE_LOG(LogLTTSGrid, Display,
			TEXT("  cell (%d,%d): type=%s reason=%s generated=%s floorZ=%.1f slope=%.1f neighbours=%s%s%s%s world=(%.0f,%.0f,%.0f)"),
			DebugInspectCell.X, DebugInspectCell.Y,
			*TypeEnum->GetNameStringByValue(static_cast<int64>(Cell->Type)),
			*ReasonEnum->GetNameStringByValue(static_cast<int64>(Cell->BlockReason)),
			*TypeEnum->GetNameStringByValue(static_cast<int64>(Cell->GeneratedType)),
			Cell->FloorZ, Cell->SlopeDeg,
			(Cell->NeighborMask & EGridDir::North) ? TEXT("N") : TEXT("-"),
			(Cell->NeighborMask & EGridDir::East) ? TEXT("E") : TEXT("-"),
			(Cell->NeighborMask & EGridDir::South) ? TEXT("S") : TEXT("-"),
			(Cell->NeighborMask & EGridDir::West) ? TEXT("W") : TEXT("-"),
			World.X, World.Y, World.Z);
	}
	else
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("  cell (%d,%d) is outside the grid."),
			DebugInspectCell.X, DebugInspectCell.Y);
	}

	// 여기서는 규칙을 null 폰으로 평가하므로, Conditional 셀은 규칙이 "아무도 아님"에 대해
	// 답하는 대로 보고한다 -- 경로 탐색이 거기까지 닿는지 확인하기에는 충분하다.
	TArray<FIntPoint> TestPath;
	const double StartTime = FPlatformTime::Seconds();
	const bool bFound = FindPath(DebugPathStart, DebugPathGoal, nullptr, TestPath);
	const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	if (bFound)
	{
		UE_LOG(LogLTTSGrid, Display, TEXT("  path (%d,%d) -> (%d,%d): %d step(s) in %.2f ms."),
			DebugPathStart.X, DebugPathStart.Y, DebugPathGoal.X, DebugPathGoal.Y, TestPath.Num(), ElapsedMs);
	}
	else
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("  path (%d,%d) -> (%d,%d): unreachable (%.2f ms)."),
			DebugPathStart.X, DebugPathStart.Y, DebugPathGoal.X, DebugPathGoal.Y, ElapsedMs);
	}
}

// ---------------------------------------------------------------------------- 라이프사이클

void AGridActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// SpawnActor / InitializeActorsForPlay 안에서 실행되므로, 어떤 BeginPlay보다 먼저
	// 그리드에 질의할 수 있다 -- 폰은 스폰되는 순간 그리드를 읽는다.
	if (GetWorld() && GetWorld()->IsGameWorld() && bRegenerateOnPlay)
	{
		GenerateFromTraces();
		ApplyStoredOverrides();
		RecomputeStats();

		UE_LOG(LogLTTSGrid, Display,
			TEXT("%s: regenerated at play. walkable=%d blocked=%d noFloor=%d overrides=%d"),
			*GetName(), NumWalkable, NumBlocked, NumNoFloor, NumOverridesApplied);
	}
	else if (Cells.Num() != SizeInCells.X * SizeInCells.Y)
	{
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: serialized grid is %d cells but %dx%d was expected. Press Generate Grid and save the level."),
			*GetName(), Cells.Num(), SizeInCells.X, SizeInCells.Y);
	}
}

void AGridActor::PostLoad()
{
	Super::PostLoad();
	RecomputeStats();
}

#if WITH_EDITOR

void AGridActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const TSet<FName> GenerationProperties = {
		GET_MEMBER_NAME_CHECKED(AGridActor, SizeInCells),
		GET_MEMBER_NAME_CHECKED(AGridActor, RegionHeight),
		GET_MEMBER_NAME_CHECKED(AGridActor, MaxStepHeight),
		GET_MEMBER_NAME_CHECKED(AGridActor, MaxSlopeAngle),
		GET_MEMBER_NAME_CHECKED(AGridActor, bClearanceTest),
		GET_MEMBER_NAME_CHECKED(AGridActor, ClearanceHeight),
		GET_MEMBER_NAME_CHECKED(AGridActor, ClearanceHalfWidth),
		GET_MEMBER_NAME_CHECKED(AGridActor, TraceChannel)
	};

	static const TSet<FName> ReportProperties = {
		GET_MEMBER_NAME_CHECKED(AGridActor, DebugInspectCell),
		GET_MEMBER_NAME_CHECKED(AGridActor, DebugPathStart),
		GET_MEMBER_NAME_CHECKED(AGridActor, DebugPathGoal)
	};

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	if (bAutoRegenerateOnEdit && GenerationProperties.Contains(PropertyName))
	{
		GenerateGrid();
	}
	else if (ReportProperties.Contains(MemberName))
	{
		// 검사용 필드를 편집했다는 것은 답을 보고 싶다는 뜻이므로 버튼을 거치지 않는다.
		LogDebugReport();
	}
	else
	{
		RefreshDebugDraw();
	}
}

void AGridActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// 원점이 움직였으니 모든 셀의 월드 위치가 바뀌었다. 드래그가 끝난 뒤에만 다시 생성한다
	// -- 마우스 이동 프레임마다 트레이스 6400번은 쓸 수 없는 수준이다.
	if (bFinished && bAutoRegenerateOnEdit)
	{
		GenerateGrid();
	}
}

void AGridActor::PostEditUndo()
{
	Super::PostEditUndo();
	RecomputeStats();
	RefreshDebugDraw();
}

void AGridActor::OnMarkerChanged()
{
	if (bIsApplyingOverrides || !bLiveApplyMarkerOverrides)
	{
		return;
	}

	if (Cells.Num() != SizeInCells.X * SizeInCells.Y)
	{
		return;		// 아직 생성된 것이 없다. 디자이너가 먼저 Generate Grid를 눌러야 한다
	}

	TGuardValue<bool> Guard(bIsApplyingOverrides, true);
	ApplyOverrides();
}

void AGridActor::BakeOverridesFromMarkers()
{
	Overrides.Reset();
	ConditionalRules.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AGridCellMarkerBase*> Markers;
	for (TActorIterator<AGridCellMarkerBase> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			Markers.Add(*It);
		}
	}

	// TActorIterator 순서는 세션마다 달라질 수 있고, 겹치는 마커는 "뒤쪽이 이긴다"로
	// 해결되므로, 순서를 고정하지 않으면 구운 결과가 실행할 때마다 달라진다. Priority가
	// 먼저, 그다음 단일 셀 마커가 박스 마커보다 우선(넓은 영역 위에 작은 수정을 얹는 것이
	// 흔한 의도), 마지막으로 이름으로 순서를 정한다.
	Markers.Sort([](const AGridCellMarkerBase& A, const AGridCellMarkerBase& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority < B.Priority;
		}
		if (A.IsSingleCellMarker() != B.IsSingleCellMarker())
		{
			return B.IsSingleCellMarker();
		}
		return A.GetFName().LexicalLess(B.GetFName());
	});

	TArray<FIntPoint> MarkerCells;
	for (AGridCellMarkerBase* Marker : Markers)
	{
		MarkerCells.Reset();
		Marker->GatherCells(*this, MarkerCells);
		if (MarkerCells.IsEmpty())
		{
			continue;
		}

		// 마커는 에디터 전용이라 쿡하면 사라지므로, 규칙 오브젝트를 그리드로 복사해 둬야 한다.
		// 마커당 복사본 하나를 만들고, 마커가 덮는 모든 셀이 공유한다.
		int32 RuleIndex = INDEX_NONE;
		if (Marker->CellType == EGridCellType::Conditional)
		{
			if (Marker->Rule)
			{
				RuleIndex = ConditionalRules.Add(DuplicateObject<UGridCellRule>(Marker->Rule, this));
			}
			else
			{
				UE_LOG(LogLTTSGrid, Warning,
					TEXT("%s: marker '%s' is Conditional but has no rule set; its cells will always be enterable."),
					*GetName(), *Marker->GetActorNameOrLabel());
			}
		}

		for (const FIntPoint& Cell : MarkerCells)
		{
			if (IsValidCell(Cell))
			{
				Overrides.Add(FGridCellOverride{ Cell, Marker->CellType, RuleIndex });
			}
		}
	}
}

#endif	// WITH_EDITOR
