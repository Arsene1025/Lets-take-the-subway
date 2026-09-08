// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/GridEscalator.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "Player/GridPawn.h"

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"

AGridEscalator::AGridEscalator()
{
	// 태우고 있을 때와 디버그를 켰을 때만 실제로 돈다. 역에 에스컬레이터가 여럿 놓이므로
	// 아무도 타지 않는 동안 매 프레임 도는 것은 그냥 낭비다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 그리드 생성 트레이스가 이 액터를 바닥으로 굽지 않게 한다. 난간에만 콜리전이 있어서
	// 그냥 두면 난간 윗면이 층 바닥으로 구워진다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

	// 컴포넌트는 여기서 만들지 않는다 -- 헤더의 설명을 볼 것.

	// 기본 경로는 +X로 6.5 m 나아가며 6.1 m 오르는 아트 실측값이다. 배치한 인스턴스마다
	// 발판에 맞춰 손보되, 처음 붙였을 때 값이 없어 길이 0이 되는 일은 없게 한다.
	RidePathLocal.Add(FVector(-325.0, 0.0, 0.0));
	RidePathLocal.Add(FVector(325.0, 0.0, 610.0));
}

// ---------------------------------------------------------------------------- 경로 계산

FVector AGridEscalator::PathPointWorld(int32 Index) const
{
	if (!RidePathLocal.IsValidIndex(Index))
	{
		return GetActorLocation();
	}

	return GetActorTransform().TransformPosition(RidePathLocal[Index]);
}

namespace
{
	/** 수평 성분만 남긴 정규화 방향. 수평 성분이 없으면 Fallback을 쓴다. */
	FVector FlattenDirection(const FVector& Delta, const FVector& Fallback)
	{
		const FVector Flat(Delta.X, Delta.Y, 0.0);
		return Flat.IsNearlyZero() ? Fallback : Flat.GetSafeNormal();
	}
}

FVector AGridEscalator::StartDirectionWorld() const
{
	if (RidePathLocal.Num() < 2)
	{
		return GetActorForwardVector().GetSafeNormal2D();
	}

	return FlattenDirection(PathPointWorld(1) - PathPointWorld(0), GetActorForwardVector().GetSafeNormal2D());
}

FVector AGridEscalator::EndDirectionWorld() const
{
	const int32 Last = RidePathLocal.Num() - 1;
	if (Last < 1)
	{
		return GetActorForwardVector().GetSafeNormal2D();
	}

	return FlattenDirection(PathPointWorld(Last) - PathPointWorld(Last - 1), GetActorForwardVector().GetSafeNormal2D());
}

FVector AGridEscalator::BoardingAnchorWorld() const
{
	return PathPointWorld(0) - StartDirectionWorld() * ApproachDistance;
}

FVector AGridEscalator::LandingAnchorWorld() const
{
	return PathPointWorld(RidePathLocal.Num() - 1) + EndDirectionWorld() * LandingDistance;
}

FVector AGridEscalator::SeatWorldAtDistance(double Distance, const AGridPawn& Pawn) const
{
	FVector Location = RidePath
		? RidePath->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World)
		: PathPointWorld(0);

	Location.Z += Pawn.HeightAboveFloor;
	return Location;
}

void AGridEscalator::ComputeBoardingCells(const AGridActor& InGrid, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();

	if (RidePathLocal.Num() < 2)
	{
		return;
	}

	const FVector Anchor = BoardingAnchorWorld();
	const FVector Forward = StartDirectionWorld();

	// 진행 방향의 좌우로 훑는다. 그리드 축이 아니라 경로 기준이라 액터가 비스듬히 놓여
	// 있어도 발판 폭을 따라간다.
	const FVector Sideways(-Forward.Y, Forward.X, 0.0);
	const int32 Half = FMath::Max(BoardingHalfWidthCells, 0);

	for (int32 Step = -Half; Step <= Half; ++Step)
	{
		const FIntPoint Cell = InGrid.WorldToCell(Anchor + Sideways * (InGrid.CellSize * Step));
		if (InGrid.IsValidCell(Cell))
		{
			OutCells.AddUnique(Cell);
		}
	}
}

// ---------------------------------------------------------------------------- 생명주기

bool AGridEscalator::BuildRuntimeComponents()
{
	USceneComponent* Root = GetRootComponent();
	if (!Root)
	{
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: no root component. AGridEscalator is meant to be a Blueprint parent, not placed directly."),
			*GetName());
		return false;
	}

	if (RidePathLocal.Num() < 2)
	{
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: RidePathLocal needs at least two points; nobody can ride this."), *GetName());
		return false;
	}

	// 붙는 쪽과 모빌리티를 맞춘다. 다르면 붙일 때마다 경고가 뜨는데, 아트 블루프린트의 루트가
	// 무엇으로 설정돼 있을지는 이쪽에서 정할 일이 아니다.
	const EComponentMobility::Type RootMobility = Root->Mobility;

	RidePath = NewObject<USplineComponent>(this, TEXT("RidePath"));
	RidePath->SetMobility(RootMobility);
	RidePath->SetupAttachment(Root);
	RidePath->RegisterComponent();
	RidePath->SetSplinePoints(RidePathLocal, ESplineCoordinateSpace::Local, false);

	// 발판은 곧게 뻗어 있다. 곡선으로 두면 점 사이에서 스플라인이 부풀어 폰이 발판을 벗어난다.
	for (int32 Index = 0; Index < RidePath->GetNumberOfSplinePoints(); ++Index)
	{
		RidePath->SetSplinePointType(Index, ESplinePointType::Linear, false);
	}
	RidePath->UpdateSpline();

	// 좌석은 어디에도 붙이지 않는다. 매 프레임 월드 좌표로 직접 옮기므로 부모가 필요 없고,
	// 붙이지 않으면 모빌리티 조합을 신경 쓸 일도 없다.
	Seat = NewObject<USceneComponent>(this, TEXT("Seat"));
	Seat->SetMobility(EComponentMobility::Movable);
	Seat->RegisterComponent();
	Seat->SetWorldLocation(PathPointWorld(0));

	if (!ClickBoxExtent.IsNearlyZero())
	{
		// 발판 메시에는 콜리전이 없다. 커서 트레이스만 받는 상자를 덮어 두지 않으면 발판
		// 한가운데를 눌러도 클릭이 뒤쪽 바닥으로 빠진다.
		ClickVolume = NewObject<UBoxComponent>(this, TEXT("ClickVolume"));
		ClickVolume->SetMobility(RootMobility);
		ClickVolume->SetupAttachment(Root);
		ClickVolume->SetRelativeLocation(ClickBoxCentreLocal);
		ClickVolume->SetBoxExtent(ClickBoxExtent, false);
		ClickVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ClickVolume->SetCollisionObjectType(ECC_WorldStatic);
		ClickVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
		ClickVolume->SetCollisionResponseToChannel(Grid ? Grid->TraceChannel.GetValue() : ECC_Visibility, ECR_Block);
		ClickVolume->SetGenerateOverlapEvents(false);
		ClickVolume->SetHiddenInGame(true);
		ClickVolume->RegisterComponent();
	}

	return true;
}

void AGridEscalator::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: no grid actor in the level; nobody can board."), *GetName());
		return;
	}

	if (!BuildRuntimeComponents())
	{
		return;
	}

	ComputeBoardingCells(*Grid, BoardingCells);
	if (BoardingCells.IsEmpty())
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: the boarding spot is off the grid; check RidePathLocal and ApproachDistance."), *GetName());
	}

	ApplyAnimationPlayRate();

	if (bDrawDebugPath)
	{
		SetActorTickEnabled(true);
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: ride is %.0f cm long, boarding from %d cell(s), first is (%d,%d)."),
		*GetName(),
		RidePath ? RidePath->GetSplineLength() : 0.0f,
		BoardingCells.Num(),
		BoardingCells.IsEmpty() ? -1 : BoardingCells[0].X,
		BoardingCells.IsEmpty() ? -1 : BoardingCells[0].Y);
}

void AGridEscalator::ApplyAnimationPlayRate()
{
	if (AnimStepSpeedAtRate1 <= 0.0f)
	{
		return;
	}

	const float Desired = RideSpeed / AnimStepSpeedAtRate1;

	TArray<USkeletalMeshComponent*> Meshes;
	GetComponents(Meshes);

	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		// 부호는 그대로 둔다. 내려가는 에스컬레이터는 같은 애니메이션을 거꾸로 돌려 만든다.
		const float Sign = (Mesh->GetPlayRate() < 0.0f) ? -1.0f : 1.0f;
		Mesh->SetPlayRate(Sign * Desired);
	}
}

void AGridEscalator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 태운 채로 사라지면 폰이 허공에 남는다. 발밑에서 그리드를 다시 찾게 해 준다.
	if (AGridPawn* Pawn = Rider.Get())
	{
		if (!Pawn->IsOnGrid())
		{
			Pawn->WalkOntoGrid(GetActorLocation());
		}
	}

	Rider.Reset();
	Phase = ERidePhase::Idle;

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- 탑승

bool AGridEscalator::CanBoard(const AGridPawn* Pawn, FText* OutReason) const
{
	if (!Pawn || !Grid || !RidePath)
	{
		return false;
	}

	if (Rider.IsValid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSVehicle", "EscalatorBusy", "Someone is already riding.");
		}
		return false;
	}

	if (!Pawn->IsOnGrid())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSVehicle", "EscalatorNotOnGrid", "Wait until you are back on the ground.");
		}
		return false;
	}

	if (Pawn->IsMoving())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSVehicle", "EscalatorPawnMoving", "Stop walking first, then step on.");
		}
		return false;
	}

	if (!BoardingCells.Contains(Pawn->GetCurrentCell()))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSVehicle", "EscalatorWrongEnd",
				"Stand at the near end to ride. This escalator runs one way.");
		}
		return false;
	}

	return true;
}

bool AGridEscalator::TryBoard(AGridPawn* Pawn, FText* OutReason)
{
	if (!CanBoard(Pawn, OutReason))
	{
		return false;
	}

	// 좌석을 첫 발판에 놓고 그 자리로 걸어오게 한다. 걸어 들어오는 동안에는 좌석을 움직이지
	// 않는다 -- 목표점이 달아나면 폰이 영영 따라잡지 못한다.
	RideDistance = 0.0;
	const FVector StartSeat = SeatWorldAtDistance(0.0, *Pawn);
	Seat->SetWorldLocation(StartSeat);

	Pawn->BoardVehicle(this, StartSeat, Seat);

	Rider = Pawn;
	Phase = ERidePhase::WaitingForRider;
	SetActorTickEnabled(true);

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %s boarded from cell (%d,%d)."),
		*GetName(), *GetNameSafe(Pawn), Pawn->GetCurrentCell().X, Pawn->GetCurrentCell().Y);

	return true;
}

void AGridEscalator::FinishRide()
{
	Rider.Reset();
	Phase = ERidePhase::Idle;
	RideDistance = 0.0;

	// 디버그를 켜 두었으면 계속 그려야 하므로 그때만 틱을 남긴다.
	SetActorTickEnabled(bDrawDebugPath);
}

void AGridEscalator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDrawDebugPath)
	{
		DrawDebugOverlay();
	}

	if (Phase == ERidePhase::Idle)
	{
		return;
	}

	AGridPawn* Pawn = Rider.Get();
	if (!Pawn)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: the rider vanished mid-ride."), *GetName());
		FinishRide();
		return;
	}

	switch (Phase)
	{
	case ERidePhase::WaitingForRider:
		// 폰이 첫 발판에 완전히 올라설 때까지 기다린다.
		if (Pawn->IsRiding())
		{
			Phase = ERidePhase::Riding;
		}
		break;

	case ERidePhase::Riding:
	{
		const double Length = RidePath->GetSplineLength();
		RideDistance = FMath::Min(RideDistance + RideSpeed * DeltaSeconds, Length);
		Seat->SetWorldLocation(SeatWorldAtDistance(RideDistance, *Pawn));

		if (RideDistance >= Length - UE_KINDA_SMALL_NUMBER)
		{
			Pawn->WalkOntoGrid(LandingAnchorWorld());
			Phase = ERidePhase::Unloading;

			UE_LOG(LogLTTSGrid, Display,
				TEXT("%s: %s reached the far end; stepping off."), *GetName(), *GetNameSafe(Pawn));
		}
		break;
	}

	case ERidePhase::Unloading:
		// 폰이 그리드 위에 다시 서면 조작이 돌아온다.
		if (Pawn->IsOnGrid())
		{
			FinishRide();
		}
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------- 확인용

void AGridEscalator::DrawDebugOverlay() const
{
	const UWorld* World = GetWorld();
	if (!World || RidePathLocal.Num() < 2)
	{
		return;
	}

	for (int32 Index = 0; Index < RidePathLocal.Num() - 1; ++Index)
	{
		DrawDebugLine(World, PathPointWorld(Index), PathPointWorld(Index + 1), FColor::Cyan, false, -1.0f, 0, 4.0f);
	}

	DrawDebugSphere(World, BoardingAnchorWorld(), 40.0f, 12, FColor::Green, false, -1.0f, 0, 3.0f);
	DrawDebugSphere(World, LandingAnchorWorld(), 40.0f, 12, FColor::Yellow, false, -1.0f, 0, 3.0f);

	if (Grid)
	{
		for (const FIntPoint& Cell : BoardingCells)
		{
			DrawDebugBox(World, Grid->CellToWorld(Cell) + FVector(0.0, 0.0, 10.0),
				FVector(Grid->CellSize * 0.45, Grid->CellSize * 0.45, 5.0),
				FColor::Green, false, -1.0f, 0, 2.0f);
		}
	}
}

void AGridEscalator::LogBoardingCells()
{
	const AGridActor* FoundGrid = AGridActor::FindGrid(GetWorld());
	if (!FoundGrid)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: no grid actor in this world."), *GetName());
		return;
	}

	TArray<FIntPoint> Cells;
	ComputeBoardingCells(*FoundGrid, Cells);

	FString Text;
	for (const FIntPoint& Cell : Cells)
	{
		Text += FString::Printf(TEXT("(%d,%d) "), Cell.X, Cell.Y);
	}

	const FVector Landing = LandingAnchorWorld();
	const FIntPoint LandingCell = FoundGrid->WorldToCell(Landing);

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: boarding cells %s| landing near cell (%d,%d) at Z %.0f | path start Z %.0f, end Z %.0f."),
		*GetName(),
		Text.IsEmpty() ? TEXT("(none) ") : *Text,
		LandingCell.X, LandingCell.Y, Landing.Z,
		PathPointWorld(0).Z, PathPointWorld(RidePathLocal.Num() - 1).Z);
}
