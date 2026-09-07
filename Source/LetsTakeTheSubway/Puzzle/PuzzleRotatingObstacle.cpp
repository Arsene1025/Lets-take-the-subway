// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleRotatingObstacle.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleElevatorBlock.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double LTTSPi = 3.14159265358979323846;
	constexpr double LTTSTwoPi = 2.0 * LTTSPi;

	/** 셀 단위 여유 값. 단순히 맞닿기만 한 도형을 겹친 것으로 취급하지 않기 위한 값이다. */
	constexpr double LTTSTouchEpsilon = 0.02;

	double WrapToPi(double Angle)
	{
		while (Angle > LTTSPi)
		{
			Angle -= LTTSTwoPi;
		}
		while (Angle < -LTTSPi)
		{
			Angle += LTTSTwoPi;
		}
		return Angle;
	}

	double WrapToTwoPi(double Angle)
	{
		while (Angle < 0.0)
		{
			Angle += LTTSTwoPi;
		}
		while (Angle >= LTTSTwoPi)
		{
			Angle -= LTTSTwoPi;
		}
		return Angle;
	}

	/**
	 * 도형을 원점 기준으로 차지하는 반지름 띠와 각도 부채꼴로 환원한 것. 이 표현을 회전시키는
	 * 것은 StartAngle에 더하는 것이고, 한 번의 회전만큼 스윕하는 것은 AngleLength를 늘리는
	 * 것이다. 회전 경로(스윕 영역) 검사가 싸게 끝나는 이유다.
	 */
	struct FPolarSpan
	{
		double MinRadius = 0.0;
		double MaxRadius = 0.0;
		double StartAngle = 0.0;
		double AngleLength = 0.0;

		bool IsFullCircle() const { return AngleLength >= LTTSTwoPi; }
	};

	/** 낮은 쪽 모서리가 (X, Y)에 있는 단위 셀의 극좌표 범위. 원점 기준으로 잰다. */
	FPolarSpan MakeCellSpan(double X, double Y)
	{
		FPolarSpan Span;

		const double X0 = X;
		const double X1 = X + 1.0;
		const double Y0 = Y;
		const double Y1 = Y + 1.0;

		const double Corners[4][2] = { { X0, Y0 }, { X1, Y0 }, { X1, Y1 }, { X0, Y1 } };

		double MaxRadiusSq = 0.0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const double CX = Corners[Index][0];
			const double CY = Corners[Index][1];
			MaxRadiusSq = FMath::Max(MaxRadiusSq, CX * CX + CY * CY);
		}
		Span.MaxRadius = FMath::Sqrt(MaxRadiusSq);

		// 원점이 셀 위나 안에 있으면 셀이 모든 방향을 덮는다는 뜻이고, 아래의 모서리 각도
		// 계산으로는 보고할 연속된 부채꼴이 없다.
		if (X0 <= 0.0 && 0.0 <= X1 && Y0 <= 0.0 && 0.0 <= Y1)
		{
			Span.MinRadius = 0.0;
			Span.StartAngle = 0.0;
			Span.AngleLength = LTTSTwoPi;
			return Span;
		}

		const double NearX = FMath::Clamp(0.0, X0, X1);
		const double NearY = FMath::Clamp(0.0, Y0, Y1);
		Span.MinRadius = FMath::Sqrt(NearX * NearX + NearY * NearY);

		// 첫 번째 모서리 기준 오프셋으로 재므로, -X 축에 걸친 부채꼴이 atan2 불연속점에서
		// 갈라지지 않고 하나의 연속 구간으로 남는다. 원점을 포함하지 않는 셀은 항상 반 바퀴
		// 미만의 각도만 차지하므로 오프셋에 모호함이 없다.
		const double BaseAngle = FMath::Atan2(Corners[0][1], Corners[0][0]);

		double MinDelta = 0.0;
		double MaxDelta = 0.0;
		for (int32 Index = 1; Index < 4; ++Index)
		{
			const double Delta = WrapToPi(FMath::Atan2(Corners[Index][1], Corners[Index][0]) - BaseAngle);
			MinDelta = FMath::Min(MinDelta, Delta);
			MaxDelta = FMath::Max(MaxDelta, Delta);
		}

		Span.StartAngle = BaseAngle + MinDelta;
		Span.AngleLength = MaxDelta - MinDelta;
		return Span;
	}

	bool ArcsOverlap(const FPolarSpan& A, const FPolarSpan& B)
	{
		if (A.IsFullCircle() || B.IsFullCircle())
		{
			return true;
		}

		return WrapToTwoPi(B.StartAngle - A.StartAngle) <= A.AngleLength
			|| WrapToTwoPi(A.StartAngle - B.StartAngle) <= B.AngleLength;
	}

	bool RadiiOverlap(const FPolarSpan& A, const FPolarSpan& B)
	{
		return A.MinRadius <= B.MaxRadius - LTTSTouchEpsilon
			&& B.MinRadius <= A.MaxRadius - LTTSTouchEpsilon;
	}
}

APuzzleRotatingObstacle::APuzzleRotatingObstacle()
{
	FootprintSize = FIntPoint(6, 4);
	MoveAxis = EPuzzleMoveAxis::None;
	Height = 300.0f;

	// 상속받은 단일 박스는 홈을 가로질러 놓이게 된다. 대신 홈을 잘라내고 남은 슬랩들로
	// 본체를 그린다.
	if (BodyMesh)
	{
		BodyMesh->SetVisibility(false);
		BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FaceMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	static const TCHAR* SlabNames[4] = { TEXT("SlabWest"), TEXT("SlabEast"), TEXT("SlabSouth"), TEXT("SlabNorth") };
	static const TCHAR* FaceNames[4] = { TEXT("FaceWest"), TEXT("FaceEast"), TEXT("FaceSouth"), TEXT("FaceNorth") };

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Slab = CreateDefaultSubobject<UStaticMeshComponent>(SlabNames[Index]);
		Slab->SetupAttachment(SceneRoot);

		// 기본 블록과 같은 규칙: 클릭 트레이스에만 보이고 그 외에는 아무것에도 잡히지 않는다.
		Slab->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Slab->SetCollisionResponseToAllChannels(ECR_Ignore);
		Slab->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Slab->SetGenerateOverlapEvents(false);

		if (CubeFinder.Succeeded())
		{
			Slab->SetStaticMesh(CubeFinder.Object);
		}
		if (BodyMaterialFinder.Succeeded())
		{
			Slab->SetMaterial(0, BodyMaterialFinder.Object);
		}
		SlabMeshes.Add(Slab);

		UStaticMeshComponent* Face = CreateDefaultSubobject<UStaticMeshComponent>(FaceNames[Index]);
		Face->SetupAttachment(SceneRoot);
		Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Face->SetCollisionResponseToAllChannels(ECR_Ignore);
		Face->SetGenerateOverlapEvents(false);

		if (CubeFinder.Succeeded())
		{
			Face->SetStaticMesh(CubeFinder.Object);
		}
		if (FaceMaterialFinder.Succeeded())
		{
			Face->SetMaterial(0, FaceMaterialFinder.Object);
		}
		InnerFaceMeshes.Add(Face);
	}
}

// ---------------------------------------------------------------------------- 저작

void APuzzleRotatingObstacle::NormaliseAuthoring()
{
	FootprintSize.X = FMath::Max(FootprintSize.X, 2);
	FootprintSize.Y = FMath::Max(FootprintSize.Y, 2);

	// 두 변의 홀짝이 다른 사각형은 중심이 그리드에서 반 셀 어긋나 있고, 함께 실려 도는
	// 것들도 모두 그 어긋난 자리에 놓이게 된다.
	if ((FootprintSize.X % 2) != (FootprintSize.Y % 2))
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: footprint %dx%d cannot turn about its centre and stay on the grid; ")
			TEXT("widened to %dx%d. Both sides must be odd or both even."),
			*GetName(), FootprintSize.X, FootprintSize.Y, FootprintSize.X, FootprintSize.Y + 1);

		FootprintSize.Y += 1;
	}

	ChannelSize.X = FMath::Clamp(ChannelSize.X, 1, FootprintSize.X);
	ChannelSize.Y = FMath::Clamp(ChannelSize.Y, 1, FootprintSize.Y);
	ChannelOffset.X = FMath::Clamp(ChannelOffset.X, 0, FootprintSize.X - ChannelSize.X);
	ChannelOffset.Y = FMath::Clamp(ChannelOffset.Y, 0, FootprintSize.Y - ChannelSize.Y);

	if (ChannelSize.X == FootprintSize.X && ChannelSize.Y == FootprintSize.Y)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: the channel covers the whole footprint, leaving no solid body."), *GetName());
	}

	// 누군가 미는 피스가 아니다. 다른 값을 저장해 둔 오래된 레벨이 로드될 때 바로잡히도록
	// 생성자뿐 아니라 여기서도 고정한다.
	MoveAxis = EPuzzleMoveAxis::None;
}

// ---------------------------------------------------------------------------- 기하

FIntPoint APuzzleRotatingObstacle::LocalToWorldOffset(FIntPoint Local) const
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

FGridRect APuzzleRotatingObstacle::GetWorldChannelRect() const
{
	const FIntPoint LowLocal = LocalToWorldOffset(ChannelOffset);
	const FIntPoint HighLocal = LocalToWorldOffset(ChannelOffset + ChannelSize - FIntPoint(1, 1));

	// 양 끝 모서리를 모두 변환한 뒤 다시 합친다: 홀수 번 돌면 둘의 역할이 뒤바뀌므로,
	// 성분별 최솟값을 취하면 특수 처리 없이 어느 방향에서도 맞는다.
	const FIntPoint MinLocal(FMath::Min(LowLocal.X, HighLocal.X), FMath::Min(LowLocal.Y, HighLocal.Y));
	const FIntPoint Size = (GetQuarterTurns() % 2 == 0)
		? ChannelSize
		: FIntPoint(ChannelSize.Y, ChannelSize.X);

	return FGridRect(GetRect().Min + MinLocal, Size);
}

FGridRect APuzzleRotatingObstacle::GetRegion() const
{
	const int32 Side = FMath::Max(FootprintSize.X, FootprintSize.Y);
	const FIntPoint WorldFootprint = GetWorldFootprint();

	// 홀짝 규칙 덕분에 두 차이가 모두 짝수이므로, 정사각형이 셀 경계에 맞게 놓이고
	// 풋프린트와 중심을 공유한다.
	const FIntPoint Inset((WorldFootprint.X - Side) / 2, (WorldFootprint.Y - Side) / 2);
	return FGridRect(GetRect().Min + Inset, FIntPoint(Side, Side));
}

FVector APuzzleRotatingObstacle::GetPivotWorld() const
{
	if (!Grid)
	{
		return GetActorLocation();
	}

	const FGridRect Region = GetRegion();
	const FVector Origin = Grid->GetGridOrigin();

	return FVector(
		Origin.X + (Region.Min.X + Region.Size.X * 0.5) * Grid->CellSize,
		Origin.Y + (Region.Min.Y + Region.Size.Y * 0.5) * Grid->CellSize,
		GetFloorZ());
}

bool APuzzleRotatingObstacle::IsSolidCell(FIntPoint Cell) const
{
	return GetRect().Contains(Cell) && !GetWorldChannelRect().Contains(Cell);
}

void APuzzleRotatingObstacle::GatherOccupiedCells(TArray<FIntPoint>& OutCells) const
{
	const FGridRect Footprint = GetRect();
	const FGridRect Channel = GetWorldChannelRect();

	TArray<FIntPoint> All;
	Footprint.GatherCells(All);

	OutCells.Reserve(OutCells.Num() + All.Num());
	for (const FIntPoint& Cell : All)
	{
		// 홈은 일부러 점유하지 않고 비워 둔다: 블록을 안으로 밀어 넣고 폰이 통과해 지나가는
		// 것이 이 피스의 존재 이유다.
		if (!Channel.Contains(Cell))
		{
			OutCells.Add(Cell);
		}
	}
}

// ---------------------------------------------------------------------------- 동승 블록

bool APuzzleRotatingObstacle::IsAttachedToInnerFace(const APuzzleBlock& Block) const
{
	const FGridRect Channel = GetWorldChannelRect();

	TArray<FIntPoint> Cells;
	Block.GatherOccupiedCells(Cells);

	static const EGridDirection Directions[4] = {
		EGridDirection::North, EGridDirection::East, EGridDirection::South, EGridDirection::West };

	for (const FIntPoint& Cell : Cells)
	{
		// 실제로 홈 안에 들어와 있는 블록 부분만 안쪽 면에 닿을 수 있다. 열린 끝 너머로
		// 삐져나온 셀은 어차피 함께 실려 간다.
		if (!Channel.Contains(Cell))
		{
			continue;
		}

		for (const EGridDirection Dir : Directions)
		{
			if (IsSolidCell(Cell + LTTSGrid::DirOffset(Dir)))
			{
				return true;
			}
		}
	}

	return false;
}

void APuzzleRotatingObstacle::GatherRiders(TArray<APuzzleBlock*>& OutRiders) const
{
	const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	for (const TWeakObjectPtr<APuzzleBlock>& Entry : Subsystem->GetBlocks())
	{
		APuzzleBlock* Block = Entry.Get();
		if (!Block || Block == this)
		{
			continue;
		}

		// 디자인에서 엘리베이터를 명시적으로 제외했다. 엘리베이터는 문 방향이 곧 퍼즐의
		// 상태인 목표 피스이므로, 지나가는 구조물이 그 방향을 바꿔 버리면 레벨을 푸는 숨은
		// 두 번째 방법이 생겨 버린다.
		if (Block->IsA<APuzzleElevatorBlock>())
		{
			continue;
		}

		if (IsAttachedToInnerFace(*Block))
		{
			OutRiders.Add(Block);
		}
	}
}

// ---------------------------------------------------------------------------- 스윕

void APuzzleRotatingObstacle::GatherSweptCells(
	const TArray<FIntPoint>& MovingCells, int32 TurnSign, TSet<FIntPoint>& OutCells) const
{
	if (!Grid || MovingCells.IsEmpty())
	{
		return;
	}

	const FGridRect Region = GetRegion();
	const double PivotX = Region.Min.X + Region.Size.X * 0.5;
	const double PivotY = Region.Min.Y + Region.Size.Y * 0.5;

	const double Turn = (TurnSign >= 0) ? (LTTSPi * 0.5) : (-LTTSPi * 0.5);

	TArray<FPolarSpan> Swept;
	Swept.Reserve(MovingCells.Num());

	double MaxRadius = 0.0;

	for (const FIntPoint& Cell : MovingCells)
	{
		FPolarSpan Span = MakeCellSpan(Cell.X - PivotX, Cell.Y - PivotY);

		if (!Span.IsFullCircle())
		{
			// 부채꼴을 회전량만큼 넓힌 것이 정확히 셀이 지나가는 영역이다: 회전은 각도에만
			// 더해지고 반지름은 건드리지 않기 때문이다.
			if (Turn < 0.0)
			{
				Span.StartAngle += Turn;
			}
			Span.AngleLength = FMath::Min(Span.AngleLength + FMath::Abs(Turn), LTTSTwoPi);
		}

		MaxRadius = FMath::Max(MaxRadius, Span.MaxRadius);
		Swept.Add(Span);
	}

	const int32 MinX = FMath::FloorToInt32(PivotX - MaxRadius);
	const int32 MaxX = FMath::CeilToInt32(PivotX + MaxRadius);
	const int32 MinY = FMath::FloorToInt32(PivotY - MaxRadius);
	const int32 MaxY = FMath::CeilToInt32(PivotY + MaxRadius);

	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const FPolarSpan Candidate = MakeCellSpan(X - PivotX, Y - PivotY);

			for (const FPolarSpan& Span : Swept)
			{
				if (RadiiOverlap(Candidate, Span) && ArcsOverlap(Span, Candidate))
				{
					OutCells.Add(FIntPoint(X, Y));
					break;
				}
			}
		}
	}
}

bool APuzzleRotatingObstacle::FindPawnRefuge(
	FIntPoint From, const TSet<FIntPoint>& Swept, const TSet<FIntPoint>& Destinations, FIntPoint& OutCell) const
{
	if (!Grid)
	{
		return false;
	}

	// 벽은 뚫고 지나가지 않고 돌아서 가므로, 폰이 걸어서 갈 수 없었던 곳으로 밀려나는
	// 일은 없다.
	TSet<FIntPoint> Visited;
	TArray<FIntPoint> Queue;
	Visited.Add(From);
	Queue.Add(From);

	static const EGridDirection Directions[4] = {
		EGridDirection::North, EGridDirection::East, EGridDirection::South, EGridDirection::West };

	constexpr int32 MaxVisited = 512;

	for (int32 Head = 0; Head < Queue.Num() && Visited.Num() < MaxVisited; ++Head)
	{
		const FIntPoint Current = Queue[Head];

		for (const EGridDirection Dir : Directions)
		{
			const FIntPoint Next = Current + LTTSGrid::DirOffset(Dir);

			if (Visited.Contains(Next))
			{
				continue;
			}
			Visited.Add(Next);

			if (!Grid->IsValidCell(Next) || !Grid->IsCellWalkableStatic(Next))
			{
				continue;
			}

			// 여기서 셀을 잡고 있는 것은 배경이거나 제자리에 남는 블록이다. 곧 움직일 피스들은
			// 이 코드가 실행되기 전에 자기 셀을 놓았다.
			if (Grid->IsCellOccupied(Next))
			{
				continue;
			}

			if (!Swept.Contains(Next) && !Destinations.Contains(Next))
			{
				OutCell = Next;
				return true;
			}

			Queue.Add(Next);
		}
	}

	return false;
}

// ---------------------------------------------------------------------------- 회전

bool APuzzleRotatingObstacle::TryRotate(int32 TurnSign, FText* OutReason)
{
	if (!Grid || IsAnimating())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleBusy", "The structure is still turning.");
		}
		return false;
	}

	UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
	if (!Subsystem)
	{
		return false;
	}

	const int32 Sign = (TurnSign >= 0) ? 1 : -1;
	const FGridRect Region = GetRegion();

	TArray<APuzzleBlock*> Riders;
	GatherRiders(Riders);

	for (const APuzzleBlock* Rider : Riders)
	{
		if (Rider->IsAnimating())
		{
			if (OutReason)
			{
				*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleRiderMoving", "Wait for the pieces to settle.");
			}
			return false;
		}
	}

	// ---- 첫 번째 패스(검사 패스): 모든 것이 어디에 놓일지 계산하고, 무엇도 건드리기 전에 거부한다.

	TArray<FIntPoint> MovingCells;
	GatherOccupiedCells(MovingCells);

	TArray<FGridRect> RiderDestinations;
	RiderDestinations.Reserve(Riders.Num());

	TSet<const AActor*> Movers;
	Movers.Add(this);

	for (const APuzzleBlock* Rider : Riders)
	{
		Rider->GatherOccupiedCells(MovingCells);
		RiderDestinations.Add(GridFootprint::RotateRect(Rider->GetRect(), Region, Sign));
		Movers.Add(Rider);
	}

	const FGridRect Destination = GridFootprint::RotateRect(GetRect(), Region, Sign);

	// 구조물이나 동승 블록이 멈춰 서는 모든 셀은 바닥이어야 하며 홈도 포함된다: 낭떠러지
	// 위에 걸린 홈은 무엇도 밀어 넣을 수 있는 곳이 아니다.
	TSet<FIntPoint> DestinationCells;
	{
		TArray<FIntPoint> Cells;
		Destination.GatherCells(Cells);
		for (const FGridRect& Rect : RiderDestinations)
		{
			Rect.GatherCells(Cells);
		}

		for (const FIntPoint& Cell : Cells)
		{
			if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
			{
				if (OutReason)
				{
					*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleNoFloor", "Cannot turn: there is no floor to turn into.");
				}
				return false;
			}
			DestinationCells.Add(Cell);
		}
	}

	// 회전 경로는 실제로 이웃 셀을 스치는 것만으로 잰다. 사각 본체는 모서리가 밖으로 나가므로
	// 자기 셀도 넣지만, 제자리에서 도는 기둥 같은 파생 클래스는 동승 블록만 넣는다.
	TArray<FIntPoint> SweepSource;
	if (SweepsOwnCells())
	{
		GatherOccupiedCells(SweepSource);
	}
	for (const APuzzleBlock* Rider : Riders)
	{
		Rider->GatherOccupiedCells(SweepSource);
	}

	TSet<FIntPoint> Swept;
	GatherSweptCells(SweepSource, Sign, Swept);

	const TSet<FIntPoint> MovingSet(MovingCells);

	for (const FIntPoint& Cell : Swept)
	{
		if (MovingSet.Contains(Cell))
		{
			continue;
		}

		// 그리드 바깥과 바닥이 없는 셀은 구조물이 그 위로 지나가는 허공이다.
		if (!Grid->IsValidCell(Cell))
		{
			continue;
		}

		if (const FGridCellData* Data = Grid->GetCell(Cell))
		{
			if (Data->Type == EGridCellType::Blocked)
			{
				if (OutReason)
				{
					*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleHitsWall", "Cannot turn: it would swing into a wall.");
				}
				return false;
			}
		}

		const AActor* Occupant = Grid->GetOccupant(Cell);
		if (Occupant && !Movers.Contains(Occupant))
		{
			if (OutReason)
			{
				*OutReason = FText::Format(
					NSLOCTEXT("LTTSPuzzle", "ObstacleHitsObject", "Cannot turn: {0} is in the way."),
					FText::FromString(Occupant->GetName()));
			}
			return false;
		}
	}

	// 폰도 공간의 일부다. 함께 실려 갈 수는 없으므로 밀어내는데, 밀어낼 빈자리가 없으면
	// 아무것도 돌지 않는다.
	AGridPawn* Pawn = Subsystem->GetGridPawn();
	TOptional<FIntPoint> PawnRefuge;

	if (Pawn)
	{
		if (Pawn->IsMoving())
		{
			Pawn->StopAndSnapToCurrentCell(TEXT("the structure is turning"));
		}

		const FIntPoint PawnCell = Pawn->GetCurrentCell();
		if (Swept.Contains(PawnCell) || DestinationCells.Contains(PawnCell))
		{
			FIntPoint Refuge;
			if (!FindPawnRefuge(PawnCell, Swept, DestinationCells, Refuge))
			{
				if (OutReason)
				{
					*OutReason = NSLOCTEXT("LTTSPuzzle", "ObstacleNoRefuge", "Cannot turn: you have nowhere to be pushed to.");
				}
				return false;
			}
			PawnRefuge = Refuge;
		}
	}

	// ---- 두 번째 패스(커밋 패스).

	// 누군가 셀을 잡기 전에 모든 이동 대상이 먼저 셀을 놓는다. 그래야 둘 사이에서 주인이
	// 바뀌는 셀이, 이전 주인이 아직 안 움직였다는 이유로 거부되지 않는다.
	Grid->ClearAllOccupantsOf(this);
	for (APuzzleBlock* Rider : Riders)
	{
		Grid->ClearAllOccupantsOf(Rider);
	}

	const FVector Pivot = GetPivotWorld();

	BeginRotation(Pivot, Sign, RotateDuration, Destination);
	for (int32 Index = 0; Index < Riders.Num(); ++Index)
	{
		Riders[Index]->BeginRotation(Pivot, Sign, RotateDuration, RiderDestinations[Index]);
	}

	// 회전 타일과 달리 끝이 아니라 시작 시점에 옮긴다: 거기서는 폰이 공간과 함께 실려
	// 돌지만, 여기서는 폰이 서 있는 셀로 벽이 들어온다.
	if (Pawn && PawnRefuge.IsSet())
	{
		Pawn->TeleportToCell(PawnRefuge.GetValue());
		Subsystem->ShowFeedback(TEXT("You are shoved out of the way."), FLinearColor(1.0f, 0.65f, 0.05f));
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: turned %s with %d rider(s); footprint now at cell (%d,%d)."),
		*GetName(), (Sign > 0) ? TEXT("clockwise") : TEXT("counter-clockwise"),
		Riders.Num(), Destination.Min.X, Destination.Min.Y);

	return true;
}

// ---------------------------------------------------------------------------- 비주얼

void APuzzleRotatingObstacle::RefreshVisual()
{
	// 일부러 Super를 호출하지 않는다: 상속받은 단일 박스는 홈을 가로지른다.
	const double CellSize = Grid ? Grid->CellSize : 100.0;

	const double HalfX = FootprintSize.X * 0.5;
	const double HalfY = FootprintSize.Y * 0.5;

	const int32 ChannelMinX = ChannelOffset.X;
	const int32 ChannelMaxX = ChannelOffset.X + ChannelSize.X;
	const int32 ChannelMinY = ChannelOffset.Y;
	const int32 ChannelMaxY = ChannelOffset.Y + ChannelSize.Y;

	// 풋프린트에서 홈을 뺀 부분을 서로 겹치지 않는 네 개의 사각형으로 자른다: 홈 양옆의
	// 긴 벽 둘과 양 끝 마감 둘. 어느 것이든 비어 있을 수 있으며, 그것이 홈의 한쪽 끝을
	// 열어 둔다.
	const int32 Slabs[4][4] = {
		{ 0,			ChannelMinX,	0,				FootprintSize.Y },
		{ ChannelMaxX,	FootprintSize.X, 0,				FootprintSize.Y },
		{ ChannelMinX,	ChannelMaxX,	0,				ChannelMinY },
		{ ChannelMinX,	ChannelMaxX,	ChannelMaxY,	FootprintSize.Y }
	};

	for (int32 Index = 0; Index < SlabMeshes.Num() && Index < 4; ++Index)
	{
		UStaticMeshComponent* Slab = SlabMeshes[Index];
		if (!Slab)
		{
			continue;
		}

		const int32 X0 = Slabs[Index][0];
		const int32 X1 = Slabs[Index][1];
		const int32 Y0 = Slabs[Index][2];
		const int32 Y1 = Slabs[Index][3];

		const bool bUsed = (X1 > X0) && (Y1 > Y0);
		Slab->SetVisibility(bUsed);
		Slab->SetCollisionEnabled(bUsed ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

		if (!bUsed)
		{
			continue;
		}

		Slab->SetRelativeLocation(FVector(
			((X0 + X1) * 0.5 - HalfX) * CellSize,
			((Y0 + Y1) * 0.5 - HalfY) * CellSize,
			Height * 0.5));

		Slab->SetRelativeScale3D(FVector(
			(X1 - X0) * CellSize / 100.0,
			(Y1 - Y0) * CellSize / 100.0,
			Height / 100.0));
	}

	// 디자인의 빗금 친 면: 홈에 접한 벽마다 얇은 패널을 붙인다. "여기가 물건이 붙는 면"이라는
	// 것이 한눈에 읽히도록 엘리베이터 문과 같은 머티리얼을 쓴다.
	constexpr double FaceThickness = 12.0;
	const double FaceHeight = FMath::Max(Height * InnerFaceHeightRatio, 1.0);

	const bool bFaceUsed[4] = {
		ChannelMinX > 0,
		ChannelMaxX < FootprintSize.X,
		ChannelMinY > 0,
		ChannelMaxY < FootprintSize.Y
	};

	const double ChannelCentreX = ((ChannelMinX + ChannelMaxX) * 0.5 - HalfX) * CellSize;
	const double ChannelCentreY = ((ChannelMinY + ChannelMaxY) * 0.5 - HalfY) * CellSize;
	const double ChannelSpanX = ChannelSize.X * CellSize;
	const double ChannelSpanY = ChannelSize.Y * CellSize;

	for (int32 Index = 0; Index < InnerFaceMeshes.Num() && Index < 4; ++Index)
	{
		UStaticMeshComponent* Face = InnerFaceMeshes[Index];
		if (!Face)
		{
			continue;
		}

		Face->SetVisibility(bFaceUsed[Index]);
		if (!bFaceUsed[Index])
		{
			continue;
		}

		FVector Location(0.0, 0.0, FaceHeight * 0.5);
		FVector Scale(1.0, 1.0, FaceHeight / 100.0);

		// 패널을 두께의 절반만큼 열린 쪽으로 밀어 넣어, 벽에 반쯤 파묻혀 이음새처럼 보이는
		// 대신 벽에서 도드라져 보이게 한다.
		switch (Index)
		{
		case 0:		// 서쪽 벽, 홈 안쪽인 동쪽을 향한다
			Location.X = (ChannelMinX - HalfX) * CellSize + FaceThickness * 0.5;
			Location.Y = ChannelCentreY;
			Scale.X = FaceThickness / 100.0;
			Scale.Y = ChannelSpanY / 100.0;
			break;

		case 1:		// 동쪽 벽
			Location.X = (ChannelMaxX - HalfX) * CellSize - FaceThickness * 0.5;
			Location.Y = ChannelCentreY;
			Scale.X = FaceThickness / 100.0;
			Scale.Y = ChannelSpanY / 100.0;
			break;

		case 2:		// 남쪽 벽
			Location.X = ChannelCentreX;
			Location.Y = (ChannelMinY - HalfY) * CellSize + FaceThickness * 0.5;
			Scale.X = ChannelSpanX / 100.0;
			Scale.Y = FaceThickness / 100.0;
			break;

		default:	// 북쪽 벽
			Location.X = ChannelCentreX;
			Location.Y = (ChannelMaxY - HalfY) * CellSize - FaceThickness * 0.5;
			Scale.X = ChannelSpanX / 100.0;
			Scale.Y = FaceThickness / 100.0;
			break;
		}

		Face->SetRelativeLocation(Location);
		Face->SetRelativeScale3D(Scale);
	}
}

// ---------------------------------------------------------------------------- 생명주기

void APuzzleRotatingObstacle::RegisterWithSubsystem(UPuzzleSubsystem& Subsystem)
{
	// 밀 수 있는 블록 목록에는 넣지 않는다: 드래그 코드가 이것을 밀려 들 것이고, 회전
	// 타일은 자기가 돌리기엔 너무 큰 피스를 보게 된다.
	Subsystem.RegisterObstacle(this);
}

void APuzzleRotatingObstacle::UnregisterFromSubsystem(UPuzzleSubsystem& Subsystem)
{
	Subsystem.UnregisterObstacle(this);
}

void APuzzleRotatingObstacle::OnConstruction(const FTransform& Transform)
{
	NormaliseAuthoring();
	Super::OnConstruction(Transform);
}

void APuzzleRotatingObstacle::BeginPlay()
{
	Super::BeginPlay();

	if (!Grid)
	{
		return;
	}

	// 이중 안전장치: OnConstruction에서 이미 강제하지만, 이 피스가 계산하는 모든 목적지는
	// 회전 정사각형을 기준으로 재며, 그리드에서 반 셀 어긋난 정사각형은 블록과 폰을 다른
	// 어떤 시스템도 표현할 수 없는 자리에 놓게 된다.
	ensureMsgf((FootprintSize.X % 2) == (FootprintSize.Y % 2),
		TEXT("%s: footprint %dx%d breaks the parity rule; the turning square is off the grid."),
		*GetName(), FootprintSize.X, FootprintSize.Y);

	const FGridRect Channel = GetWorldChannelRect();
	const FGridRect Region = GetRegion();

	TArray<FIntPoint> Solid;
	GatherOccupiedCells(Solid);

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %d solid cell(s), channel %dx%d at cell (%d,%d), turning region %dx%d at (%d,%d)."),
		*GetName(), Solid.Num(),
		Channel.Size.X, Channel.Size.Y, Channel.Min.X, Channel.Min.Y,
		Region.Size.X, Region.Size.Y, Region.Min.X, Region.Min.Y);

	// 홈은 바닥일 때만 쓸모가 있고, 본체가 플랫폼 밖에 놓이는 회전은 런타임에 거부된다.
	// 플레이어가 시도할 때가 아니라 지금 미리 알려 두는 편이 낫다.
	TArray<FIntPoint> ChannelCells;
	Channel.GatherCells(ChannelCells);
	for (const FIntPoint& Cell : ChannelCells)
	{
		if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: channel cell (%d,%d) is not walkable floor; nothing can stand in the opening there."),
				*GetName(), Cell.X, Cell.Y);
		}
	}
}

#if WITH_EDITOR

bool APuzzleRotatingObstacle::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (!InProperty)
	{
		return true;
	}

	// 여기서는 둘 다 의미가 없다: 구조물은 레버로만 돌리지 절대 밀지 않는다.
	const FName Name = InProperty->GetFName();
	return Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, MoveAxis)
		&& Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, bOneStepPerDrag);
}

#endif

// ---------------------------------------------------------------------------- 콘솔

namespace
{
	/**
	 * 레버 없이 장애물을 돌린다.
	 *
	 * 레버는 옆 셀에서만 조작할 수 있으므로, 디자인이 요구하는 한 가지 경우 -- 구조물이 돌 때
	 * 폰이 홈 안에 서 있는 상황 -- 는 일반 플레이로는 만들 수 없다. 그 경우도 테스트할 수
	 * 있도록 이 명령을 둔다.
	 */
	void RotateObstacleCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.RotateObstacle: run this in play mode."));
			return;
		}

		const FString Filter = Args.IsValidIndex(0) ? Args[0] : FString();
		const int32 Sign = (Args.IsValidIndex(1) && Args[1].StartsWith(TEXT("-"))) ? -1 : 1;

		int32 Matched = 0;
		for (TActorIterator<APuzzleRotatingObstacle> It(World); It; ++It)
		{
			APuzzleRotatingObstacle* Obstacle = *It;
			if (!Obstacle || (!Filter.IsEmpty() && !Obstacle->GetName().Contains(Filter)))
			{
				continue;
			}

			++Matched;

			FText Reason;
			if (!Obstacle->TryRotate(Sign, &Reason))
			{
				UE_LOG(LogLTTSGrid, Warning, TEXT("%s: %s"), *Obstacle->GetName(), *Reason.ToString());
			}
		}

		if (Matched == 0)
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("ltts.RotateObstacle: no rotating obstacle matching '%s'."), *Filter);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRotateObstacleCommand(
	TEXT("ltts.RotateObstacle"),
	TEXT("Turn a rotating obstacle without its lever: ltts.RotateObstacle <name substring> [+1|-1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RotateObstacleCommand));
