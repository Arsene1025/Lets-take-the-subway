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
	// The engine sphere is 100 cm across, so the scale is the radius in metres.
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
	// Absolute rotation keeps the isometric framing fixed no matter how the pawn is turned.
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

	// Designers may tune the radius on an instance; keep the collision-less bounds and the
	// visible ball in step with it.
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

	// Only the horizontal travel rolls the ball; the vertical part of a stair step does not.
	const FVector Flat(Delta.X, Delta.Y, 0.0);
	const double Distance = Flat.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// A ball rolling along D spins about the axis Up x D, by arc length / radius.
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
		// Re-planning from a position between two cells would be ambiguous, so remember the
		// click and act on it as soon as the pawn is cell-aligned again.
		PendingGoal = Goal;
		return;
	}

	PlanPath(Goal);
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

	// A Conditional rule can change between planning and arriving, so the next cell is
	// re-checked every time the pawn is about to enter it.
	FText DeniedMessage;
	if (!Grid->CanPawnEnter(NextCell, this, &DeniedMessage))
	{
		Path.Reset();
		GoalCell = CurrentCell;
		PendingGoal.Reset();
		SetActorLocation(CellStandLocation(CurrentCell));
		ReportFeedback(
			FString::Printf(TEXT("Stopped at (%d,%d): %s"), CurrentCell.X, CurrentCell.Y, *DeniedMessage.ToString()),
			FLinearColor(1.0f, 0.65f, 0.05f));
		RefreshPathDebug();
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
