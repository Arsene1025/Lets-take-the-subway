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

AGridActor::AGridActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DebugDrawComponent = CreateDefaultSubobject<UGridDebugDrawComponent>(TEXT("GridDebugDraw"));
	DebugDrawComponent->SetupAttachment(SceneRoot);
}

// ---------------------------------------------------------------------------- Conversions

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

// ---------------------------------------------------------------------------- Walkability

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

// ---------------------------------------------------------------------------- Occupancy

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
		// A stale entry (the holder was destroyed) is cleared by anyone, so a block that
		// leaves the level cannot leave cells permanently unwalkable.
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

	// Checked before the Conditional rule so a rule never runs for a cell that a block is
	// standing on anyway -- rules are called many times per path search.
	if (IsCellOccupied(Cell))
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

	// Reported from the enum rather than from the cell: the reason is a runtime occupant,
	// so it is never stored in BlockReason.
	if (IsCellOccupied(Cell))
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

	// Expanding square rings. Not a true nearest-by-distance search, but on a uniform grid
	// the difference never exceeds one cell and this needs no sorting.
	for (int32 Radius = 1; Radius <= MaxRadius; ++Radius)
	{
		for (int32 OffsetY = -Radius; OffsetY <= Radius; ++OffsetY)
		{
			for (int32 OffsetX = -Radius; OffsetX <= Radius; ++OffsetX)
			{
				if (FMath::Max(FMath::Abs(OffsetX), FMath::Abs(OffsetY)) != Radius)
				{
					continue;	// interior of the ring was covered by a smaller radius
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

// ---------------------------------------------------------------------------- Generation

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

	// Markers carry no collision, but ignoring every editor-only actor also covers any
	// visualiser someone adds later. In a game world (bRegenerateOnPlay) ignore pawns
	// instead, so a pawn standing on the floor cannot mask the floor under itself.
	//
	// Tagged actors are movable props -- puzzle blocks. They stand on the floor and would
	// otherwise bake themselves in as Blocked cells at whatever position they were authored
	// at, so the floor beneath them must be traced as if they were not there.
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

			// The trace started inside geometry -- a wall or column that passes through the
			// top of the region. There is no headroom here at all, and the reported impact
			// point and normal are the trace start, not a surface, so they cannot be used.
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
				// Start the box above MaxStepHeight so a ramp or a small lip cannot block itself.
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

	// Only +X and +Y are examined; each connection is written to both endpoints, so the
	// mask is symmetric by construction and no one-way links can appear.
	//
	// A flat step and a ramp both show up as a height difference, but only the step is a
	// discontinuity you have to climb. On a ramp the rise is the surface itself: at
	// MaxSlopeAngle 35 degrees a 1 m cell already rises 70 cm, which the step rule alone
	// would sever, cutting every ramp into disconnected strips. So the allowance grows with
	// the *shallower* of the two cells' slopes -- two ramp cells get the ramp's own rise plus
	// the step budget, while a ramp next to flat ground still only gets the step budget and
	// cannot bridge a drop.
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

	// Start from what tracing found, so removing a marker reverts its cells exactly.
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

		// A cell with no floor has no Z to stand on, so making it walkable would teleport
		// the pawn to the grid origin height. Refuse it and say so.
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

// ---------------------------------------------------------------------------- Editor actions

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
		// Nothing to stamp onto -- fall back to a full generate.
		GenerateGrid();
		return;
	}

	Modify();

	// No traces here: ApplyStoredOverrides rebuilds every cell from its generated state,
	// so this is just the bake-and-stamp half of GenerateGrid.
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

	// Rules are evaluated with a null pawn here, so a Conditional cell reports whatever its
	// rule says about "nobody" -- enough to prove the path search reaches it.
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

// ---------------------------------------------------------------------------- Lifecycle

void AGridActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Runs inside SpawnActor / InitializeActorsForPlay, so the grid is queryable before any
	// BeginPlay -- the pawn reads it the moment it spawns.
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
		// Editing an inspection field is a request to see the answer, so skip the button.
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

	// The origin moved, so every cell's world position changed. Only regenerate once the
	// drag is over -- 6400 traces per mouse-move frame would be unusable.
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
		return;		// nothing generated yet; the designer has to press Generate Grid first
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

	// TActorIterator order is not stable between sessions, and overlapping markers resolve
	// by "later wins", so the order has to be pinned down or the baked result would differ
	// run to run. Priority first, then single-cell markers over box markers (a small fix on
	// top of a broad region is the common intent), then name as a final tiebreak.
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

		// Markers are editor-only and vanish on cook, so the rule object has to be copied
		// onto the grid. One copy per marker, shared by all the cells it covers.
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
