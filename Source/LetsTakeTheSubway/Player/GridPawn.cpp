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
		if (!Grid->FindNearestWalkableCell(SpawnCell, 8, this, StartCell))
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

void AGridPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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
