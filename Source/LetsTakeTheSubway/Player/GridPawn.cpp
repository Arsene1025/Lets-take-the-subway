// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridPawn.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
#include "Player/GridPlayerController.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AGridPawn::AGridPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->InitSphereRadius(BallRadius);
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetGenerateOverlapEvents(false);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Sphere);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyMesh->SetGenerateOverlapEvents(false);
	// 엔진 구체는 지름 100 cm이므로 스케일 값이 곧 미터 단위 반지름이다.
	BodyMesh->SetRelativeScale3D(FVector(BallRadius / 50.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (MaterialFinder.Succeeded())
	{
		BodyMesh->SetMaterial(0, MaterialFinder.Object);
	}

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Sphere);
	SpringArm->TargetArmLength = CameraArmLength;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = false;
	SpringArm->bInheritRoll = false;
	// 절대 회전을 쓰면 폰이 어느 쪽으로 돌든 아이소메트릭 구도가 고정된다.
	SpringArm->SetUsingAbsoluteRotation(true);
	SpringArm->SetRelativeRotation(CameraRotation);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
}

void AGridPawn::BeginPlay()
{
	Super::BeginPlay();

	if (SpringArm)
	{
		SpringArm->TargetArmLength = CameraArmLength;
		SpringArm->SetWorldRotation(CameraRotation);
	}

	// 디자이너가 인스턴스에서 반지름을 조정할 수 있으니, 콜리전 없는 바운드와 보이는
	// 공을 그 값에 맞춰 둔다.
	if (Sphere)
	{
		Sphere->SetSphereRadius(BallRadius);
	}
	if (BodyMesh)
	{
		BodyMesh->SetRelativeScale3D(FVector(BallRadius / 50.0f));
	}

	if (!EnsureGrid())
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; movement is disabled."), *GetName());
		return;
	}

	const FIntPoint SpawnCell = Grid->WorldToCell(GetActorLocation());
	FIntPoint StartCell = SpawnCell;

	if (!Grid->CanPawnEnter(SpawnCell, this))
	{
		// 행인·하차와 같은 규칙으로 고른다. 여기서는 걸어 들어가지 않고 그 셀에 그대로
		// 세운다 -- 레벨이 시작되기도 전에 폰이 어딘가에서 걸어 나오면 이상하다.
		if (!Grid->FindEntryCell(GetActorLocation(), 8, this, TOptional<FIntPoint>(), StartCell))
		{
			UE_LOG(LogLTTSGrid, Error,
				TEXT("%s: spawned at cell (%d,%d) with no walkable cell within 8 cells."),
				*GetName(), SpawnCell.X, SpawnCell.Y);
			return;
		}

		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: spawn cell (%d,%d) is not walkable; moved to (%d,%d)."),
			*GetName(), SpawnCell.X, SpawnCell.Y, StartCell.X, StartCell.Y);
	}

	CurrentCell = StartCell;
	GoalCell = StartCell;
	SetActorLocation(CellStandLocation(StartCell));

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: standing on cell (%d,%d)."), *GetName(), CurrentCell.X, CurrentCell.Y);
}

bool AGridPawn::EnsureGrid()
{
	if (!IsValid(Grid))
	{
		Grid = AGridActor::FindGrid(GetWorld());
	}
	return Grid != nullptr;
}

FVector AGridPawn::CellStandLocation(FIntPoint Cell) const
{
	return Grid ? Grid->CellToWorld(Cell) + FVector(0.0, 0.0, HeightAboveFloor) : GetActorLocation();
}

void AGridPawn::ReportFeedback(const FString& Message, const FLinearColor& Color) const
{
	if (AGridPlayerController* GridController = Cast<AGridPlayerController>(GetController()))
	{
		GridController->ShowFeedback(Message, Color);
	}
}

void AGridPawn::RollBody(const FVector& Delta)
{
	if (!bRollWhileMoving || !BodyMesh || BallRadius <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 수평 이동만 공을 굴린다; 계단 스텝의 수직 성분은 굴리지 않는다.
	const FVector Flat(Delta.X, Delta.Y, 0.0);
	const double Distance = Flat.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// D 방향으로 구르는 공은 Up x D 축을 중심으로 호 길이 / 반지름만큼 회전한다.
	const FVector Axis = FVector::CrossProduct(FVector::UpVector, Flat / Distance);
	const double AngleRad = Distance / BallRadius;
	BodyMesh->AddWorldRotation(FQuat(Axis, AngleRad));
}

void AGridPawn::RefreshPathDebug() const
{
	if (!Grid)
	{
		return;
	}

	if (LTTSGridDebug::ShouldDrawWorld())
	{
		FGridRuntimeDebugDrawer::DrawPath(GetWorld(), *Grid, CurrentCell, Path);
	}
	else
	{
		FGridRuntimeDebugDrawer::ClearPath(GetWorld());
	}
}

void AGridPawn::RequestMoveToCell(FIntPoint Goal)
{
	if (!EnsureGrid())
	{
		ReportFeedback(TEXT("No grid in this level."), FLinearColor::Red);
		return;
	}

	// 탈것 위에서는 셀 경로가 없다. 컨트롤러도 이때는 클릭을 넘기지 않지만, 여기서 한 번
	// 더 막아 두면 어느 경로로 들어오든 같은 답이 된다.
	if (RideState != ERideState::OnGrid)
	{
		return;
	}

	if (IsMoving())
	{
		// 두 셀 사이 위치에서 재계획하면 모호하므로, 클릭을 기억해 뒀다가 폰이 다시
		// 셀에 정렬되는 즉시 처리한다.
		PendingGoal = Goal;
		return;
	}

	PlanPath(Goal);
}

void AGridPawn::SetHeldDirection(TOptional<EGridDirection> Dir)
{
	// TOptional의 비교 연산자에 기대지 않고 직접 본다. 값이 있고 없고와 값 자체를 나눠 보는
	// 편이 무엇을 비교하는지도 분명하다.
	const bool bSame = (HeldDirection.IsSet() == Dir.IsSet())
		&& (!Dir.IsSet() || HeldDirection.GetValue() == Dir.GetValue());

	if (!bSame)
	{
		// 방향이 바뀌었으면 막혔다는 안내를 새 방향에 대해 다시 낼 수 있어야 한다.
		bHeldDirectionBlockedReported = false;
	}

	HeldDirection = Dir;

	if (!HeldDirection.IsSet())
	{
		return;
	}

	// 클릭이나 탑승 예약으로 세워 둔 긴 경로는 버린다. 플레이어가 폰을 직접 밀기 시작했으므로
	// 지금 들어가고 있는 칸까지만 마저 가고, 그다음부터는 키가 정한다. 지금 칸을 버리지 않는
	// 이유는 두 셀 사이에서 폰을 멈춰 세울 수 없기 때문이다.
	if (Path.Num() > 1)
	{
		Path.SetNum(1);
		GoalCell = Path[0];
	}

	PendingGoal.Reset();
}

bool AGridPawn::StepOnce(EGridDirection Dir)
{
	SetHeldDirection(Dir);
	const bool bStepped = TryStepHeldDirection();

	// 이어 걷지 않는다. 이미 세워 둔 한 칸짜리 경로는 그대로 걸어간다.
	SetHeldDirection(TOptional<EGridDirection>());

	return bStepped;
}

bool AGridPawn::TryStepHeldDirection()
{
	if (!Grid || !HeldDirection.IsSet() || RideState != ERideState::OnGrid)
	{
		return false;
	}

	const FIntPoint NextCell = CurrentCell + LTTSGrid::DirOffset(HeldDirection.GetValue());

	FText DeniedMessage;
	if (!Grid->CanPawnEnter(NextCell, this, &DeniedMessage))
	{
		// 막힌 쪽으로 한 번 부딪히고 선다. 키를 누르고 있는 동안 매 틱 다시 흔들거나 같은
		// 줄을 다시 쓰지 않도록, 방향이 바뀌기 전까지는 한 번만 알린다.
		if (!bHeldDirectionBlockedReported)
		{
			bHeldDirectionBlockedReported = true;
			Bump(NextCell);
			ReportFeedback(
				FString::Printf(TEXT("Cell (%d,%d): %s"), NextCell.X, NextCell.Y, *DeniedMessage.ToString()),
				FLinearColor(1.0f, 0.65f, 0.05f));
		}

		return false;
	}

	bHeldDirectionBlockedReported = false;

	Path.Reset();
	Path.Add(NextCell);
	GoalCell = NextCell;
	PendingGoal.Reset();

	RefreshPathDebug();
	return true;
}

void AGridPawn::StopAndSnapToCurrentCell(const FString& Reason, const FLinearColor& Color)
{
	Path.Reset();
	GoalCell = CurrentCell;
	PendingGoal.Reset();

	if (Grid)
	{
		SetActorLocation(CellStandLocation(CurrentCell));
	}

	ReportFeedback(
		FString::Printf(TEXT("Stopped at (%d,%d): %s"), CurrentCell.X, CurrentCell.Y, *Reason),
		Color);

	RefreshPathDebug();
}

void AGridPawn::TeleportToCell(FIntPoint Cell)
{
	Path.Reset();
	PendingGoal.Reset();
	bBumping = false;

	// 셀로 직접 옮기는 것은 언제나 그리드 위에 선다는 뜻이다. 탈것에 붙은 채로 옮겨지면
	// 다음 틱에 탈것이 위치를 도로 가져간다.
	Vehicle.Reset();
	RideAnchor.Reset();
	RideState = ERideState::OnGrid;

	CurrentCell = Cell;
	GoalCell = Cell;

	if (Grid)
	{
		SetActorLocation(CellStandLocation(Cell));
	}

	RefreshPathDebug();
}

bool AGridPawn::PlanPath(FIntPoint Goal)
{
	if (Goal == CurrentCell)
	{
		ReportFeedback(FString::Printf(TEXT("Already on cell (%d,%d)."), Goal.X, Goal.Y), FLinearColor::White);
		return false;
	}

	FText DeniedMessage;
	if (!Grid->CanPawnEnter(Goal, this, &DeniedMessage))
	{
		ReportFeedback(
			FString::Printf(TEXT("Cell (%d,%d): %s"), Goal.X, Goal.Y, *DeniedMessage.ToString()),
			FLinearColor::Red);
		return false;
	}

	TArray<FIntPoint> NewPath;
	if (!Grid->FindPath(CurrentCell, Goal, this, NewPath) || NewPath.IsEmpty())
	{
		ReportFeedback(
			FString::Printf(TEXT("No route to cell (%d,%d)."), Goal.X, Goal.Y),
			FLinearColor::Red);
		return false;
	}

	Path = MoveTemp(NewPath);
	GoalCell = Goal;

	ReportFeedback(
		FString::Printf(TEXT("Moving to (%d,%d): %d step(s)."), Goal.X, Goal.Y, Path.Num()),
		FLinearColor(0.45f, 0.85f, 1.0f));

	RefreshPathDebug();
	return true;
}

// ---------------------------------------------------------------------------- 탑승과 하차

void AGridPawn::BoardVehicle(AActor* InVehicle, const FVector& SeatWorld, USceneComponent* FollowComponent)
{
	if (!InVehicle)
	{
		return;
	}

	// 그리드 위에서 하던 일은 전부 버린다. 탈것 위에서는 셀 경로가 의미를 잃는다.
	Path.Reset();
	PendingGoal.Reset();
	bBumping = false;
	GoalCell = CurrentCell;
	RefreshPathDebug();

	Vehicle = InVehicle;
	RideAnchor = FollowComponent;
	WalkTarget = SeatWorld;
	RideState = ERideState::Entering;

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: boarding %s from cell (%d,%d)."),
		*GetName(), *InVehicle->GetName(), CurrentCell.X, CurrentCell.Y);
}

void AGridPawn::WalkOntoGrid(const FVector& NearWorld, int32 SearchRadius)
{
	if (!EnsureGrid())
	{
		return;
	}

	FIntPoint Entry = FIntPoint::ZeroValue;
	if (!Grid->FindEntryCell(NearWorld, SearchRadius, this, TOptional<FIntPoint>(), Entry))
	{
		// 내릴 곳이 없다면 마지막으로 알던 셀로 되돌린다. 그리드 밖에 폰을 버려 두는 것보다
		// 낫다 -- 그 상태에서는 클릭도 길찾기도 되지 않는다.
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: no walkable cell within %d cells of the exit; returning to cell (%d,%d)."),
			*GetName(), SearchRadius, CurrentCell.X, CurrentCell.Y);

		Vehicle.Reset();
		RideAnchor.Reset();
		RideState = ERideState::OnGrid;
		TeleportToCell(CurrentCell);
		return;
	}

	LandingCell = Entry;
	WalkTarget = CellStandLocation(Entry);
	RideState = ERideState::Leaving;

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: leaving %s towards cell (%d,%d)."),
		*GetName(), *GetNameSafe(Vehicle.Get()), Entry.X, Entry.Y);
}

void AGridPawn::Bump(FIntPoint TowardCell)
{
	// 서 있을 때만 흔든다. 걷는 중에 위치를 덧씌우면 스텝 보간과 서로 싸운다.
	if (!Grid || RideState != ERideState::OnGrid || !Path.IsEmpty() || bBumping)
	{
		return;
	}

	const FVector Here = CellStandLocation(CurrentCell);
	const FVector There = Grid->CellToWorld(TowardCell);
	const FVector Delta = FVector(There.X - Here.X, There.Y - Here.Y, 0.0);

	if (Delta.IsNearlyZero())
	{
		return;
	}

	BumpDirection = Delta.GetSafeNormal();
	BumpElapsed = 0.0f;
	bBumping = true;
}

void AGridPawn::TickStraightWalk(float DeltaSeconds)
{
	const FVector OldLocation = GetActorLocation();
	const FVector NewLocation = FMath::VInterpConstantTo(OldLocation, WalkTarget, DeltaSeconds, MoveSpeed);
	SetActorLocation(NewLocation);
	RollBody(NewLocation - OldLocation);

	if (!NewLocation.Equals(WalkTarget, 0.5))
	{
		return;
	}

	SetActorLocation(WalkTarget);

	if (RideState == ERideState::Entering)
	{
		AActor* Ride = Vehicle.Get();
		if (!Ride)
		{
			// 걸어 들어가는 사이에 탈것이 사라졌다. 발밑에서 다시 그리드를 찾는다.
			RideState = ERideState::OnGrid;
			RideAnchor.Reset();
			WalkOntoGrid(GetActorLocation());
			return;
		}

		// 좌석 오프셋은 붙는 순간에 잰다. 그래야 탈것이 그동안 조금 움직였더라도 폰이
		// 지금 서 있는 자리에 그대로 실린다.
		RideOffset = GetActorLocation() - RideBaseLocation();
		RideState = ERideState::Riding;
		return;
	}

	// Leaving: 여기서부터 다시 그리드 위다.
	CurrentCell = LandingCell;
	GoalCell = LandingCell;
	Vehicle.Reset();
	RideAnchor.Reset();
	RideState = ERideState::OnGrid;

	Grid->NotifyPawnEnteredCell(this, CurrentCell);
	RefreshPathDebug();

	ReportFeedback(
		FString::Printf(TEXT("Stepped off at (%d,%d)."), CurrentCell.X, CurrentCell.Y),
		FLinearColor::White);
}

void AGridPawn::TickRide(float DeltaSeconds)
{
	if (!Vehicle.IsValid())
	{
		RideState = ERideState::OnGrid;
		RideAnchor.Reset();
		WalkOntoGrid(GetActorLocation());
		return;
	}

	SetActorLocation(RideBaseLocation() + RideOffset);
}

FVector AGridPawn::RideBaseLocation() const
{
	// 앵커가 있으면 그 컴포넌트를 따른다. 에스컬레이터는 액터가 가만히 서 있고 좌석 컴포넌트만
	// 발판을 따라 움직이므로, 액터 원점을 기준으로 삼으면 폰이 제자리에 머문다.
	if (const USceneComponent* Anchor = RideAnchor.Get())
	{
		return Anchor->GetComponentLocation();
	}

	const AActor* Ride = Vehicle.Get();
	return Ride ? Ride->GetActorLocation() : GetActorLocation();
}

void AGridPawn::TickBump(float DeltaSeconds)
{
	BumpElapsed += DeltaSeconds;

	const float Alpha = FMath::Clamp(BumpElapsed / FMath::Max(BumpDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);

	// 반주기 사인이면 나갔다가 정확히 제자리로 돌아온다. 끝에서 위치를 따로 복구할 필요가 없다.
	const double Offset = BumpDistance * FMath::Sin(PI * Alpha);
	SetActorLocation(CellStandLocation(CurrentCell) + BumpDirection * Offset);

	if (Alpha >= 1.0f)
	{
		bBumping = false;
		SetActorLocation(CellStandLocation(CurrentCell));
	}
}

void AGridPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (RideState)
	{
	case ERideState::Entering:
	case ERideState::Leaving:
		TickStraightWalk(DeltaSeconds);
		return;

	case ERideState::Riding:
		TickRide(DeltaSeconds);
		return;

	default:
		break;
	}

	if (bBumping)
	{
		TickBump(DeltaSeconds);
		return;
	}

	// 서 있는데 이동 키가 눌려 있으면 첫 걸음을 세운다. 이어지는 걸음은 TickGridStep이
	// 도착할 때마다 스스로 잇는다.
	if (Path.IsEmpty() && HeldDirection.IsSet())
	{
		TryStepHeldDirection();
	}

	TickGridStep(DeltaSeconds);
}

void AGridPawn::TickGridStep(float DeltaSeconds)
{
	if (!Grid)
	{
		return;
	}

	// 이번 프레임에 걸을 수 있는 거리를 예산으로 두고, 셀에 도착하면 남은 예산을 그대로 다음
	// 셀로 이월한다. 도착한 프레임을 그냥 끝내면 셀마다 한 프레임씩 서게 되어, 키를 누르고
	// 있는 동안의 걸음이 셀 경계마다 눈에 띄게 끊긴다.
	double Budget = static_cast<double>(MoveSpeed) * DeltaSeconds;

	while (Budget > 0.0 && !Path.IsEmpty())
	{
		const FIntPoint NextCell = Path[0];

		// Conditional 룰은 계획 시점과 도착 시점 사이에 바뀔 수 있으므로, 폰이 다음 셀에
		// 들어가기 직전마다 다시 검사한다.
		FText DeniedMessage;
		if (!Grid->CanPawnEnter(NextCell, this, &DeniedMessage))
		{
			StopAndSnapToCurrentCell(DeniedMessage.ToString());
			return;
		}

		const FVector TargetLocation = CellStandLocation(NextCell);
		const FVector OldLocation = GetActorLocation();
		const FVector ToTarget = TargetLocation - OldLocation;
		const double Remaining = ToTarget.Size();

		// 0.5는 예전 VInterpConstantTo + Equals(0.5) 시절의 도착 허용 오차 그대로다.
		if (Remaining > Budget + 0.5)
		{
			const FVector NewLocation = OldLocation + ToTarget * (Budget / Remaining);
			SetActorLocation(NewLocation);
			RollBody(NewLocation - OldLocation);
			return;
		}

		SetActorLocation(TargetLocation);
		RollBody(TargetLocation - OldLocation);
		Budget -= Remaining;

		CurrentCell = NextCell;
		Path.RemoveAt(0);

		Grid->NotifyPawnEnteredCell(this, CurrentCell);

		if (PendingGoal.IsSet())
		{
			const FIntPoint Goal = PendingGoal.GetValue();
			PendingGoal.Reset();
			Path.Reset();
			PlanPath(Goal);
			continue;
		}

		// 키를 계속 누르고 있으면 남은 예산 그대로 다음 셀을 이어 붙인다. 막혔으면
		// TryStepHeldDirection이 그 자리에서 부딪히는 연출과 사유를 낸다.
		if (Path.IsEmpty() && HeldDirection.IsSet())
		{
			TryStepHeldDirection();
			continue;
		}

		if (Path.IsEmpty())
		{
			ReportFeedback(
				FString::Printf(TEXT("Arrived at (%d,%d)."), CurrentCell.X, CurrentCell.Y),
				FLinearColor::White);
		}
	}

	RefreshPathDebug();
}

// ---------------------------------------------------------------------------- 콘솔
//
// 걷기는 이제 이동 키로만 시작된다. 그래서 "폰이 이쪽으로 한 칸 갈 수 있는가"는 키보드
// 없이는 물을 수 없는 질문이 됐다. 블록의 ltts.BlockSlide와 같은 자리다.

namespace
{
	bool ParsePawnDirection(const FString& Text, EGridDirection& OutDir)
	{
		const FString Upper = Text.ToUpper();

		if (Upper.StartsWith(TEXT("N"))) { OutDir = EGridDirection::North; return true; }
		if (Upper.StartsWith(TEXT("E"))) { OutDir = EGridDirection::East;  return true; }
		if (Upper.StartsWith(TEXT("S"))) { OutDir = EGridDirection::South; return true; }
		if (Upper.StartsWith(TEXT("W"))) { OutDir = EGridDirection::West;  return true; }

		return false;
	}

	void PawnStepCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.PawnStep: run this in play mode."));
			return;
		}

		if (Args.Num() < 1)
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.PawnStep: usage is ltts.PawnStep <N|E|S|W>"));
			return;
		}

		EGridDirection Dir = EGridDirection::North;
		if (!ParsePawnDirection(Args[0], Dir))
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.PawnStep: '%s' is not N, E, S or W."), *Args[0]);
			return;
		}

		const APlayerController* Controller = World->GetFirstPlayerController();
		AGridPawn* Pawn = Controller ? Cast<AGridPawn>(Controller->GetPawn()) : nullptr;

		if (!Pawn)
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.PawnStep: no grid pawn is possessed."));
			return;
		}

		const FIntPoint From = Pawn->GetCurrentCell();
		const bool bStepped = Pawn->StepOnce(Dir);

		UE_LOG(LogLTTSGrid, Display,
			TEXT("ltts.PawnStep: (%d,%d) %s -> %s"),
			From.X, From.Y,
			*StaticEnum<EGridDirection>()->GetNameStringByValue(static_cast<int64>(Dir)),
			bStepped ? TEXT("walking") : TEXT("refused"));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GPawnStepCommand(
	TEXT("ltts.PawnStep"),
	TEXT("Walk the player pawn one cell as a movement key would: ltts.PawnStep <N|E|S|W>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PawnStepCommand));
