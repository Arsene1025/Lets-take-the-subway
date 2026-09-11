// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/GridTrain.h"

#include "LetsTakeTheSubway.h"
#include "Art/ArtMaterialUtil.h"
#include "Grid/GridActor.h"
#include "Curves/CurveFloat.h"
#include "Grid/GridTypes.h"
#include "NPC/GridNPCSpawner.h"
#include "Player/GridPawn.h"
#include "Vehicle/VehicleSeat.h"
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

// ---------------------------------------------------------------------------- 문과 탑승 셀

void AGridTrain::GetDoorOffsets(TArray<float>& OutOffsets) const
{
	OutOffsets.Reset();

	if (DoorOffsetsLocal.Num() > 0)
	{
		OutOffsets = DoorOffsetsLocal;
		return;
	}

	// 저작 값이 없으면 그레이박스 프록시 문 두 짝이 곧 문이다. RefreshVisual이 그 자리에
	// 슬랩을 그리므로 보이는 것과 탈 수 있는 자리가 어긋나지 않는다.
	const double DoorSpacing = BodyLength * 0.25;
	OutOffsets.Add(static_cast<float>(-DoorSpacing));
	OutOffsets.Add(static_cast<float>(DoorSpacing));
}

void AGridTrain::ComputeBoardingCells(int32 StopIndex, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();

	if (!Grid || !Stops.IsValidIndex(StopIndex))
	{
		return;
	}

	TArray<float> Offsets;
	GetDoorOffsets(Offsets);
	if (Offsets.IsEmpty())
	{
		return;
	}

	// 정차 위치에서 잰다. 열차가 지금 어디 있든 그 역의 문 앞은 같은 자리다.
	const FVector StopLocation = GetStopLocation(StopIndex);
	const FQuat Rotation = GetActorQuat();
	const double CellSize = Grid->CellSize;

	// 문은 승강장 쪽(로컬 +Y) 면에 있다. 차체 반폭 바깥으로 한 칸 나가야 승강장 셀이다.
	const double SideOffset = BodyWidth * 0.5 + CellSize * 0.75;

	for (const float Offset : Offsets)
	{
		// 문 폭에 걸치는 셀을 모두 넣는다. 문 하나가 셀 두 칸에 걸쳐 있으면 어느 쪽에
		// 서 있든 탈 수 있어야 한다.
		const double Half = FMath::Max(DoorWidth, 1.0f) * 0.5;
		const int32 Samples = FMath::Max(2, FMath::CeilToInt((Half * 2.0) / (CellSize * 0.5)) + 1);

		for (int32 Sample = 0; Sample < Samples; ++Sample)
		{
			const double Along = Offset - Half + (Half * 2.0) * Sample / (Samples - 1);

			// 승강장이 차체에서 한 칸 더 떨어져 있을 수도 있다. 걸을 수 있는 셀이 나올
			// 때까지 바깥으로 한 칸씩 두 번까지 더 본다.
			for (int32 Step = 0; Step < 3; ++Step)
			{
				const FVector Local(Along, SideOffset + Step * CellSize, 0.0);
				const FIntPoint Cell = Grid->WorldToCell(StopLocation + Rotation.RotateVector(Local));

				if (!Grid->IsValidCell(Cell))
				{
					continue;
				}

				if (Grid->IsCellWalkableStatic(Cell))
				{
					OutCells.AddUnique(Cell);
					break;
				}
			}
		}
	}
}

void AGridTrain::GetBoardingCells(int32 StopIndex, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();

	if (!Stops.IsValidIndex(StopIndex))
	{
		return;
	}

	// 손으로 적은 목록이 있으면 그쪽이 이긴다. 문 위치로는 표현할 수 없는 배치
	// (그레이박스 테스트 맵)가 이미 있고, 그것을 계산 값으로 덮어써서는 안 된다.
	if (Stops[StopIndex].BoardingCells.Num() > 0)
	{
		OutCells = Stops[StopIndex].BoardingCells;
		return;
	}

	ComputeBoardingCells(StopIndex, OutCells);
}

void AGridTrain::GetApproachCells(TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();

	TArray<FIntPoint> Cells;
	for (int32 Index = 0; Index < Stops.Num(); ++Index)
	{
		GetBoardingCells(Index, Cells);
		for (const FIntPoint& Cell : Cells)
		{
			OutCells.AddUnique(Cell);
		}
	}
}

// ---------------------------------------------------------------------------- 가감속

float AGridTrain::GetSpeedFactor(float Alpha) const
{
	const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);

	float Factor = 1.0f;

	if (SpeedCurve)
	{
		Factor = SpeedCurve->GetFloatValue(Clamped);
	}
	else
	{
		// 기본 곡선: 앞쪽 AccelFraction 동안 올리고 뒤쪽 DecelFraction 동안 내린다.
		// 둘 중 작은 쪽이 이기므로 구간이 짧아 두 구간이 겹쳐도 값이 1을 넘지 않는다.
		const float Rise = (AccelFraction > KINDA_SMALL_NUMBER) ? (Clamped / AccelFraction) : 1.0f;
		const float Fall = (DecelFraction > KINDA_SMALL_NUMBER) ? ((1.0f - Clamped) / DecelFraction) : 1.0f;
		const float Linear = FMath::Clamp(FMath::Min(Rise, Fall), 0.0f, 1.0f);

		// 지수를 씌워 시작과 끝의 꺾임을 부드럽게 한다. PuzzleBlock의 회전 이징과 같은 생각이다.
		Factor = FMath::Pow(Linear, EaseExponent);
	}

	// 0이 되면 영영 도착하지 못한다.
	return FMath::Clamp(Factor, MinSpeedFactor, 1.0f);
}

// ---------------------------------------------------------------------------- 아트

void AGridTrain::ApplyArtFallbackMaterial()
{
	if (!ArtFallbackMaterial)
	{
		return;
	}

	// 블루프린트가 붙인 메시만 손본다. 코드가 만든 프록시(차체와 문 슬랩)는 자기
	// 그레이박스 머티리얼을 갖고 있고, 문 슬랩은 열림·닫힘에 따라 색을 바꾼다.
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents(Meshes);

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || Mesh == BodyMesh || DoorMeshes.Contains(Mesh))
		{
			continue;
		}

		LTTSArt::ReplaceDefaultMaterials(*Mesh, ArtFallbackMaterial);
	}
}

void AGridTrain::RefreshVisual()
{
	if (BodyMesh)
	{
		// 아트 메시를 쓰는 블루프린트에서는 프록시 상자를 숨기되 콜리전은 남긴다. 눈에
		// 보이는 것은 아트이고, 커서가 잡는 것은 언제나 이 상자다.
		BodyMesh->SetVisibility(bShowProxyBody);

		// 액터 원점이 객차 바닥이다. 폰의 좌석 높이를 그 위로 잡을 수 있어 계산이 단순해진다.
		BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, BodyHeight * 0.5));
		BodyMesh->SetRelativeScale3D(FVector(BodyLength / 100.0, BodyWidth / 100.0, BodyHeight / 100.0));
	}

	// 문은 승강장 쪽(+Y) 면에 둘. 열차는 X축을 따라 달린다는 전제다.
	//
	// 자리는 DoorOffsetsLocal을 따른다. 프록시 슬랩과 탑승 셀이 같은 값에서 나와야
	// 그레이박스 맵에서 보이는 문과 실제로 탈 수 있는 자리가 어긋나지 않는다.
	TArray<float> DoorOffsets;
	GetDoorOffsets(DoorOffsets);
	for (int32 Index = 0; Index < DoorMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Door = DoorMeshes[Index];
		if (!Door)
		{
			continue;
		}

		Door->SetVisibility(bShowProxyBody);

		const double OffsetX = DoorOffsets.IsValidIndex(Index) ? DoorOffsets[Index] : 0.0;
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

// ---------------------------------------------------------------------------- 연출 훅

void AGridTrain::AnimateDoorsOpening_Implementation(float /*Alpha*/)
{
	// 아직 비어 있다. 문 애니메이션이 준비되면 여기서 재생한다.
	//
	// 이 훅이 비어 있어도 규칙은 지켜진다: 문이 열리는 동안 타고 내리지 못하게 막는 것은
	// DoorsOpening 단계이지 이 함수가 아니다.
}

void AGridTrain::AnimateDoorsClosing_Implementation(float /*Alpha*/)
{
	// 아직 비어 있다. 열리는 쪽과 같다.
}

void AGridTrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
	ApplyArtFallbackMaterial();
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

		// 탑승 셀은 저작 값이 있으면 그것, 없으면 문 위치에서 계산한 것이다. 여기서 한 번
		// 찍어 두면 아트 문이 승강장과 어긋났을 때 로그만 보고 알 수 있다.
		TArray<FIntPoint> Boarding;
		GetBoardingCells(Index, Boarding);

		if (Boarding.IsEmpty())
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: stop %d '%s' has no boarding cells; the player cannot board there."),
				*GetName(), Index, *Stop.StopName.ToString());
		}
		else
		{
			FString CellList;
			for (const FIntPoint& Cell : Boarding)
			{
				CellList += FString::Printf(TEXT("%s(%d,%d)"), CellList.IsEmpty() ? TEXT("") : TEXT(" "), Cell.X, Cell.Y);
			}

			UE_LOG(LogLTTSGrid, Display,
				TEXT("%s: stop %d '%s': %d boarding cell(s) %s -- %s."),
				*GetName(), Index, *Stop.StopName.ToString(), Boarding.Num(),
				Stop.BoardingCells.Num() > 0 ? TEXT("authored") : TEXT("from door offsets"),
				*CellList);
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
		TEXT("%s: %d stops, %.0f cm/s, doors opening %.1f s / open %.1f s / closing %.1f s, loop %s."),
		*GetName(), Stops.Num(), Speed,
		DoorOpeningSeconds, DoorOpenSeconds, DoorCloseSeconds,
		bLoop ? TEXT("on") : TEXT("off"));
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

	// 구간을 통째로 기억한다. 가감속은 "지금 몇 퍼센트를 왔는가"를 알아야 하는데,
	// 매 프레임 현재 위치에서 목표까지의 남은 거리만 보면 그 답이 나오지 않는다.
	MoveFrom = GetActorLocation();
	MoveTo = GetStopLocation(CurrentStop);
	MoveLength = FVector::Distance(MoveFrom, MoveTo);
	MoveDistance = 0.0;

	UE_LOG(LogLTTSGrid, Verbose,
		TEXT("%s: departing for stop %d '%s'."),
		*GetName(), CurrentStop, *Stops[CurrentStop].StopName.ToString());
}

float AGridTrain::GetPhaseAlpha(float Duration) const
{
	// 시간이 0이면 그 단계는 한 틱 만에 지나간다. 훅에는 끝난 모습(1)을 넘겨야 문이 반쯤
	// 열린 채로 멈춘 것처럼 보이지 않는다.
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}

	return FMath::Clamp(1.0f - PhaseTimer / Duration, 0.0f, 1.0f);
}

void AGridTrain::EnterDoorsOpening()
{
	Phase = ETrainPhase::DoorsOpening;
	PhaseTimer = DoorOpeningSeconds;
	RefreshDoorLook();

	UE_LOG(LogLTTSGrid, Verbose,
		TEXT("%s: doors opening at stop %d '%s'."),
		*GetName(), CurrentStop, *Stops[CurrentStop].StopName.ToString());

	// 첫 프레임을 훅에 알려 둔다. 다음 틱까지 문이 이전 자세로 남아 있지 않게 한다.
	AnimateDoorsOpening(0.0f);
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
		else if (Grid)
		{
			// 저작된 하차 셀이 없으면 이 역의 문 앞 셀 가운데 폰에게 가장 가까운 칸으로
			// 내린다. 목록의 첫 칸을 쓰면 객차 어디에 앉아 있었든 언제나 같은 문으로 나오고,
			// 그 문이 반대쪽 끝이면 폰이 차체를 가로질러 비스듬히 걸어 나온다.
			TArray<FIntPoint> Boarding;
			GetBoardingCells(CurrentStop, Boarding);

			double BestDistanceSq = TNumericLimits<double>::Max();
			for (const FIntPoint& Cell : Boarding)
			{
				if (!Grid->IsValidCell(Cell))
				{
					continue;
				}

				const FVector World = Grid->CellToWorld(Cell);
				const double DistanceSq = FVector::DistSquaredXY(World, Pawn->GetActorLocation());
				if (DistanceSq < BestDistanceSq)
				{
					BestDistanceSq = DistanceSq;
					ExitWorld = World;
				}
			}
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

	AnimateDoorsClosing(0.0f);
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
			EnterDoorsOpening();
		}
		break;

	case ETrainPhase::Moving:
	{
		// 구간이 없다시피 하면(같은 자리) 곧바로 도착으로 친다.
		if (MoveLength <= UE_KINDA_SMALL_NUMBER)
		{
			SetActorLocation(MoveTo);
			EnterDoorsOpening();
			break;
		}

		const float Alpha = static_cast<float>(FMath::Clamp(MoveDistance / MoveLength, 0.0, 1.0));
		MoveDistance += Speed * GetSpeedFactor(Alpha) * DeltaSeconds;

		if (MoveDistance >= MoveLength)
		{
			SetActorLocation(MoveTo);
			EnterDoorsOpening();
			break;
		}

		SetActorLocation(FMath::Lerp(MoveFrom, MoveTo, MoveDistance / MoveLength));
		break;
	}

	case ETrainPhase::DoorsOpening:
		PhaseTimer -= DeltaSeconds;
		AnimateDoorsOpening(GetPhaseAlpha(DoorOpeningSeconds));

		// 문이 다 열려야 행인이 쏟아지고 탑승자가 내린다. 그 전까지는 아무도 드나들지 않는다.
		if (PhaseTimer <= 0.0f)
		{
			EnterDoorsOpen();
		}
		break;

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
		AnimateDoorsClosing(GetPhaseAlpha(DoorCloseSeconds));

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

	// 문이 완전히 열려 있을 때만 탄다. 열리는 중과 닫히는 중은 연출 시간이고, 그 사이에
	// 타면 문이 움직이는 내내 폰이 문틀을 통과해 들어가는 그림이 된다.
	if (!AreDoorsOpen())
	{
		if (OutReason)
		{
			if (!AreDoorsMoving())
			{
				*OutReason = NSLOCTEXT("LTTSTrain", "TrainDoorsClosed", "The doors are closed.");
			}
			else if (Phase == ETrainPhase::DoorsOpening)
			{
				*OutReason = NSLOCTEXT("LTTSTrain", "TrainDoorsOpening", "The doors are still opening.");
			}
			else
			{
				*OutReason = NSLOCTEXT("LTTSTrain", "TrainDoorsClosing", "The doors are closing.");
			}
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

	TArray<FIntPoint> Boarding;
	GetBoardingCells(CurrentStop, Boarding);
	if (!Boarding.Contains(Pawn->GetCurrentCell()))
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

	// 좌석은 차체 중심이 아니라 **폰을 마주 보는 자리**다. 진행축(로컬 X) 위의 자리는 폰의
	// 것을 그대로 쓰고 폭 방향만 중심선으로 당기므로, 폰은 자기가 선 문으로 똑바로 걸어
	// 들어간다. 중심으로 잡으면 45 m짜리 객차 끝에서 탄 폰이 차체를 따라 비스듬히 미끄러져
	// 들어가 어느 문으로 탔는지도, 탄 것인지도 읽히지 않는다.
	const FVector Seat = LTTSVehicle::SeatFacingRider(
		Pawn->GetActorLocation(),
		GetActorLocation(),
		GetActorForwardVector(),
		FMath::Max(BodyLength * 0.5 - 100.0, 0.0),
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
		EnterDoorsOpening();
	}
}

void AGridTrain::ForceDoors(bool bOpen)
{
	if (!Stops.IsValidIndex(CurrentStop))
	{
		return;
	}

	// 열 때도 연출 단계를 건너뛰지 않는다. 콘솔로 연 문만 애니메이션 없이 벌컥 열리면
	// 시험한 것과 실제로 도는 것이 달라진다.
	if (bOpen)
	{
		EnterDoorsOpening();
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
