// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/GridTrain.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "NPC/GridNPCSpawner.h"
#include "Player/GridPawn.h"
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
	/** 문 표시 슬랩의 두께와 높이 비율. */
	constexpr double TrainDoorThickness = 24.0;
	constexpr double TrainDoorHeightRatio = 0.7;
	constexpr int32 NumTrainDoors = 2;
}

AGridTrain::AGridTrain()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ClosedFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_B2.MI_GreyBox_B2"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpenFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);

	// 퍼즐 조각과 같은 규칙이다: Visibility만 막아 커서에는 잡히고, 물리와 폰은 관여하지
	// 않는다. 폰에는 콜리전이 아예 없다.
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BodyMesh->SetGenerateOverlapEvents(false);

	if (CubeFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeFinder.Object);
	}
	if (BodyFinder.Succeeded())
	{
		BodyMesh->SetMaterial(0, BodyFinder.Object);
	}

	if (ClosedFinder.Succeeded())
	{
		DoorClosedMaterial = ClosedFinder.Object;
	}
	if (OpenFinder.Succeeded())
	{
		DoorOpenMaterial = OpenFinder.Object;
	}

	DoorMeshes.Reserve(NumTrainDoors);
	for (int32 Index = 0; Index < NumTrainDoors; ++Index)
	{
		UStaticMeshComponent* Door = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("DoorMesh%d"), Index));
		Door->SetupAttachment(SceneRoot);
		Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Door->SetCollisionResponseToAllChannels(ECR_Ignore);
		Door->SetGenerateOverlapEvents(false);

		if (CubeFinder.Succeeded())
		{
			Door->SetStaticMesh(CubeFinder.Object);
		}
		if (ClosedFinder.Succeeded())
		{
			Door->SetMaterial(0, ClosedFinder.Object);
		}

		DoorMeshes.Add(Door);
	}

	// 열차는 선로 위를 지나다닌다. 그리드가 이것을 바닥으로 구우면 지나간 자리가 통째로
	// 걸을 수 있는 셀이 된다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

// ---------------------------------------------------------------------------- 비주얼

void AGridTrain::RefreshVisual()
{
	if (BodyMesh)
	{
		// 액터 원점이 객차 바닥이다. 폰의 좌석 높이를 그 위로 잡을 수 있어 계산이 단순해진다.
		BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, BodyHeight * 0.5));
		BodyMesh->SetRelativeScale3D(FVector(BodyLength / 100.0, BodyWidth / 100.0, BodyHeight / 100.0));
	}

	// 문은 승강장 쪽(+Y) 면에 둘. 열차는 X축을 따라 달린다는 전제다.
	const double DoorSpacing = BodyLength * 0.25;
	for (int32 Index = 0; Index < DoorMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Door = DoorMeshes[Index];
		if (!Door)
		{
			continue;
		}

		const double OffsetX = (Index == 0) ? -DoorSpacing : DoorSpacing;
		Door->SetRelativeLocation(FVector(
			OffsetX,
			BodyWidth * 0.5 + TrainDoorThickness * 0.5,
			BodyHeight * TrainDoorHeightRatio * 0.5));
		Door->SetRelativeScale3D(FVector(
			1.2,
			TrainDoorThickness / 100.0,
			BodyHeight * TrainDoorHeightRatio / 100.0));
	}
}

void AGridTrain::RefreshDoorLook()
{
	const bool bOpen = AreDoorsOpen();
	if (bOpen == bDoorsLookOpen)
	{
		return;
	}

	bDoorsLookOpen = bOpen;

	if (UMaterialInterface* Material = bOpen ? DoorOpenMaterial : DoorClosedMaterial)
	{
		for (UStaticMeshComponent* Door : DoorMeshes)
		{
			if (Door)
			{
				Door->SetMaterial(0, Material);
			}
		}
	}
}

void AGridTrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
}

// ---------------------------------------------------------------------------- 생명주기

void AGridTrain::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; the train is idle."), *GetName());
		return;
	}

	// 높이는 배치된 그대로 쓴다. 선로 셀은 걸을 수 없어 그리드에 바닥 높이가 없으므로
	// 셀에서 Z를 얻을 수 없다.
	TrackZ = GetActorLocation().Z;

	if (Stops.Num() < 2)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: needs at least two stops to run; it will stand still."), *GetName());
		RefreshVisual();
		return;
	}

	for (int32 Index = 0; Index < Stops.Num(); ++Index)
	{
		const FGridTrainStop& Stop = Stops[Index];
		if (!Grid->IsValidCell(Stop.StopCell))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: stop %d '%s' cell (%d,%d) is outside the grid."),
				*GetName(), Index, *Stop.StopName.ToString(), Stop.StopCell.X, Stop.StopCell.Y);
		}

		if (Stop.BoardingCells.IsEmpty())
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: stop %d '%s' has no boarding cells; the player cannot board there."),
				*GetName(), Index, *Stop.StopName.ToString());
		}
	}

	CurrentStop = 0;
	SetActorLocation(GetStopLocation(0));
	RefreshVisual();

	// 첫 역에는 조금 뜸을 들이고 들어온다. 레벨이 시작하자마자 문이 열려 있으면 열차가
	// 원래 거기 서 있었던 것처럼 보인다.
	Phase = ETrainPhase::Idle;
	PhaseTimer = StartDelaySeconds;

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->RegisterVehicle(this);
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %d stops, %.0f cm/s, doors open %.1f s, loop %s."),
		*GetName(), Stops.Num(), Speed, DoorOpenSeconds, bLoop ? TEXT("on") : TEXT("off"));
}

void AGridTrain::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->UnregisterVehicle(this);
	}

	Rider.Reset();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- 정차역

FVector AGridTrain::GetStopLocation(int32 StopIndex) const
{
	if (!Grid || !Stops.IsValidIndex(StopIndex))
	{
		return GetActorLocation();
	}

	const FVector Origin = Grid->GetGridOrigin();
	const FIntPoint Cell = Stops[StopIndex].StopCell;

	// 셀 중심의 XY만 쓰고 높이는 선로 높이를 유지한다. CellToWorld는 걸을 수 있는 바닥의
	// Z를 돌려주는데, 선로 셀에는 그런 바닥이 없다.
	return FVector(
		Origin.X + (Cell.X + 0.5) * Grid->CellSize,
		Origin.Y + (Cell.Y + 0.5) * Grid->CellSize,
		TrackZ);
}

int32 AGridTrain::GetNextStopIndex() const
{
	const int32 Next = CurrentStop + 1;

	if (Next < Stops.Num())
	{
		return Next;
	}

	return bLoop ? 0 : INDEX_NONE;
}

// ---------------------------------------------------------------------------- 상태 전이

void AGridTrain::EnterMoving()
{
	const int32 Next = GetNextStopIndex();
	if (Next == INDEX_NONE)
	{
		// 마지막 역이고 순환하지 않는다. 문을 닫은 채 여기 선다.
		Phase = ETrainPhase::Idle;
		PhaseTimer = 0.0f;
		return;
	}

	CurrentStop = Next;
	Phase = ETrainPhase::Moving;

	UE_LOG(LogLTTSGrid, Verbose,
		TEXT("%s: departing for stop %d '%s'."),
		*GetName(), CurrentStop, *Stops[CurrentStop].StopName.ToString());
}

void AGridTrain::EnterDoorsOpen()
{
	Phase = ETrainPhase::DoorsOpen;
	PhaseTimer = DoorOpenSeconds;
	RefreshDoorLook();

	const FGridTrainStop& Stop = Stops[CurrentStop];

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: doors open at stop %d '%s'."), *GetName(), CurrentStop, *Stop.StopName.ToString());

	OnDoorsOpened.Broadcast(this, Stop.StopName);

	// 행인이 내린다. 플레이어가 없는 승강장에서도 일어나는 배경 연출이다.
	if (AGridNPCSpawner* Spawner = Stop.DisembarkSpawner)
	{
		if (Stop.DisembarkCount > 0)
		{
			Spawner->SpawnBurst(Stop.DisembarkCount);
		}
	}

	// 타고 있던 폰을 내려 준다. 카메라가 열차 안을 들여다보지 않으므로, 문이 열리는 것을
	// 기준으로 자동 하차시킨다(기획).
	if (AGridPawn* Pawn = Rider.Get())
	{
		FVector ExitWorld = GetActorLocation();

		if (Grid && Grid->IsValidCell(Stop.ExitCell))
		{
			ExitWorld = Grid->CellToWorld(Stop.ExitCell);
		}
		else if (Grid && !Stop.BoardingCells.IsEmpty() && Grid->IsValidCell(Stop.BoardingCells[0]))
		{
			ExitWorld = Grid->CellToWorld(Stop.BoardingCells[0]);
		}

		Pawn->WalkOntoGrid(ExitWorld);
		Rider.Reset();
		UnloadedPawn = Pawn;
	}
}

void AGridTrain::EnterDoorsClosing()
{
	Phase = ETrainPhase::DoorsClosing;
	PhaseTimer = DoorCloseSeconds;
	RefreshDoorLook();

	UE_LOG(LogLTTSGrid, Verbose, TEXT("%s: doors closing."), *GetName());
}

void AGridTrain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Grid || Stops.Num() < 2)
	{
		return;
	}

	// 하차한 폰이 그리드에 다시 서면 지정된 자리까지 마저 걸어가게 한다. 문 앞에서 바로
	// 멈추면 플레이어가 자기가 어디에 내렸는지 알아차리기 전에 열차가 떠난다.
	if (AGridPawn* Unloaded = UnloadedPawn.Get())
	{
		if (Unloaded->IsOnGrid())
		{
			const FIntPoint PostExit = Stops[CurrentStop].PostExitCell;
			if (Grid->IsValidCell(PostExit))
			{
				Unloaded->RequestMoveToCell(PostExit);
			}

			UnloadedPawn.Reset();
		}
	}

	switch (Phase)
	{
	case ETrainPhase::Idle:
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.0f && GetNextStopIndex() != INDEX_NONE)
		{
			// 첫 역에는 이미 서 있으므로, 대기가 끝나면 문부터 연다.
			EnterDoorsOpen();
		}
		break;

	case ETrainPhase::Moving:
	{
		const FVector Target = GetStopLocation(CurrentStop);
		const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), Target, DeltaSeconds, Speed);
		SetActorLocation(NewLocation);

		if (NewLocation.Equals(Target, 1.0))
		{
			SetActorLocation(Target);
			EnterDoorsOpen();
		}
		break;
	}

	case ETrainPhase::DoorsOpen:
		PhaseTimer -= DeltaSeconds;

		// 타는 중인 폰이 아직 차체 안에 들어오지 못했으면 문을 닫지 않는다. 그대로 출발하면
		// 폰이 문 앞에 서 있던 자리로 걸어가는 동안 열차가 빠져나가 뒤늦게 허공에서 붙는다.
		if (PhaseTimer <= 0.0f && !(Rider.IsValid() && !Rider->IsRiding()))
		{
			EnterDoorsClosing();
		}
		break;

	case ETrainPhase::DoorsClosing:
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.0f)
		{
			EnterMoving();
		}
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------- 탑승

bool AGridTrain::CanBoard(const AGridPawn* Pawn, FText* OutReason) const
{
	if (!Pawn || !Stops.IsValidIndex(CurrentStop))
	{
		return false;
	}

	if (!AreDoorsOpen())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSTrain", "TrainDoorsClosed", "The doors are closed.");
		}
		return false;
	}

	if (!Pawn->IsOnGrid() || Pawn->IsMoving())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSTrain", "TrainPawnMoving", "Wait until you have stopped walking.");
		}
		return false;
	}

	if (!Stops[CurrentStop].BoardingCells.Contains(Pawn->GetCurrentCell()))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSTrain", "TrainNotAtDoor", "Stand at a door to board.");
		}
		return false;
	}

	return true;
}

bool AGridTrain::TryBoard(AGridPawn* Pawn, FText* OutReason)
{
	if (!CanBoard(Pawn, OutReason))
	{
		return false;
	}

	// 좌석은 객차 바닥 위 차체 중심이다. 폰이 문 앞 셀에서 여기까지 직선으로 걸어 들어온다.
	const FVector Seat(
		GetActorLocation().X,
		GetActorLocation().Y,
		GetActorLocation().Z + Pawn->HeightAboveFloor);

	Pawn->BoardVehicle(this, Seat);
	Rider = Pawn;

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %s boarded at stop %d '%s'."),
		*GetName(), *Pawn->GetName(), CurrentStop, *Stops[CurrentStop].StopName.ToString());

	// 플레이어가 탔으니 더 기다릴 이유가 없다. 남은 대기 시간을 버리고 문을 닫는다.
	if (bDepartAfterBoarding)
	{
		PhaseTimer = FMath::Min(PhaseTimer, 1.5f);
	}

	return true;
}

// ---------------------------------------------------------------------------- 콘솔용

void AGridTrain::ForceArriveAtNextStop()
{
	if (Stops.Num() < 2)
	{
		return;
	}

	if (Phase != ETrainPhase::Moving)
	{
		EnterMoving();
	}

	if (Phase == ETrainPhase::Moving)
	{
		SetActorLocation(GetStopLocation(CurrentStop));
		EnterDoorsOpen();
	}
}

void AGridTrain::ForceDoors(bool bOpen)
{
	if (!Stops.IsValidIndex(CurrentStop))
	{
		return;
	}

	if (bOpen)
	{
		EnterDoorsOpen();
	}
	else
	{
		EnterDoorsClosing();
	}
}

// ---------------------------------------------------------------------------- 콘솔

namespace
{
	AGridTrain* FindTrain(UWorld* World, const FString& Filter)
	{
		for (TActorIterator<AGridTrain> It(World); It; ++It)
		{
			AGridTrain* Train = *It;
			if (Train && (Filter.IsEmpty() || Train->GetName().Contains(Filter)))
			{
				return Train;
			}
		}

		return nullptr;
	}

	void TrainArriveCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainArrive: run this in play mode."));
			return;
		}

		const FString Filter = Args.IsValidIndex(0) ? Args[0] : FString();
		if (AGridTrain* Train = FindTrain(World, Filter))
		{
			Train->ForceArriveAtNextStop();
		}
		else
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainArrive: no train matching '%s'."), *Filter);
		}
	}

	void TrainDoorsCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainDoors: run this in play mode."));
			return;
		}

		const FString Filter = Args.IsValidIndex(0) ? Args[0] : FString();
		const bool bOpen = Args.IsValidIndex(1) ? (FCString::Atoi(*Args[1]) != 0) : true;

		if (AGridTrain* Train = FindTrain(World, Filter))
		{
			Train->ForceDoors(bOpen);
		}
		else
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainDoors: no train matching '%s'."), *Filter);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GTrainArriveCommand(
	TEXT("ltts.TrainArrive"),
	TEXT("Bring a train into its next stop right now: ltts.TrainArrive [name substring]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TrainArriveCommand));

static FAutoConsoleCommandWithWorldAndArgs GTrainDoorsCommand(
	TEXT("ltts.TrainDoors"),
	TEXT("Open or close a train's doors: ltts.TrainDoors [name substring] [0|1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TrainDoorsCommand));
