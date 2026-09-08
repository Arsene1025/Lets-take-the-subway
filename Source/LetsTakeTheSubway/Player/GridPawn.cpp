// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridPawn.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
#include "Player/GridPlayerController.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
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

void AGridPawn::BoardVehicle(AActor* InVehicle, const FVector& SeatWorld)
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
			WalkOntoGrid(GetActorLocation());
			return;
		}

		// 좌석 오프셋은 붙는 순간에 잰다. 그래야 탈것이 그동안 조금 움직였더라도 폰이
		// 지금 서 있는 자리에 그대로 실린다.
		RideOffset = GetActorLocation() - Ride->GetActorLocation();
		RideState = ERideState::Riding;
		return;
	}

	// Leaving: 여기서부터 다시 그리드 위다.
	CurrentCell = LandingCell;
	GoalCell = LandingCell;
	Vehicle.Reset();
	RideState = ERideState::OnGrid;

	Grid->NotifyPawnEnteredCell(this, CurrentCell);
	RefreshPathDebug();

	ReportFeedback(
		FString::Printf(TEXT("Stepped off at (%d,%d)."), CurrentCell.X, CurrentCell.Y),
		FLinearColor::White);
}

void AGridPawn::TickRide(float DeltaSeconds)
{
	AActor* Ride = Vehicle.Get();
	if (!Ride)
	{
		RideState = ERideState::OnGrid;
		WalkOntoGrid(GetActorLocation());
		return;
	}

	SetActorLocation(Ride->GetActorLocation() + RideOffset);
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

	TickGridStep(DeltaSeconds);
}

void AGridPawn::TickGridStep(float DeltaSeconds)
{
	if (!Grid || Path.IsEmpty())
	{
		return;
	}

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
	const FVector NewLocation = FMath::VInterpConstantTo(OldLocation, TargetLocation, DeltaSeconds, MoveSpeed);
	SetActorLocation(NewLocation);
	RollBody(NewLocation - OldLocation);

	if (!NewLocation.Equals(TargetLocation, 0.5))
	{
		return;
	}

	SetActorLocation(TargetLocation);
	CurrentCell = NextCell;
	Path.RemoveAt(0);

	Grid->NotifyPawnEnteredCell(this, CurrentCell);

	if (PendingGoal.IsSet())
	{
		const FIntPoint Goal = PendingGoal.GetValue();
		PendingGoal.Reset();
		Path.Reset();
		PlanPath(Goal);
		return;
	}

	if (Path.IsEmpty())
	{
		ReportFeedback(
			FString::Printf(TEXT("Arrived at (%d,%d)."), CurrentCell.X, CurrentCell.Y),
			FLinearColor::White);
	}

	RefreshPathDebug();
}
