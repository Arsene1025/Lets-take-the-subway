// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/GridNPC.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
#include "Grid/GridTypes.h"
#include "NPC/GridNPCSpawner.h"
#include "Player/GridPawn.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 엔진 기본 도형은 모두 한 변 100 cm이고 원점이 중심이다. */
	constexpr double BasicShapeSize = 100.0;

	/** 셀 중심에 도착했다고 볼 오차(cm). 플레이어 폰과 같은 값이다. */
	constexpr double ArrivalEpsilon = 0.5;
}

AGridNPC::AGridNPC()
{
	PrimaryActorTick.bCanEverTick = true;

	// 컨트롤러를 붙이지 않는다. 이 폰은 자기 경로만 따라가며 입력도 AI도 받지 않는다.
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	// APawn의 기본값은 겹치면 스폰을 포기하는 것이다. 스포너는 그리드 밖 아무 데나 놓일 수
	// 있으므로 언제나 스폰돼야 한다.
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(SceneRoot);

	NoseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NoseMesh"));
	NoseMesh->SetupAttachment(SceneRoot);

	// 콜리전을 완전히 끈다. 퍼즐 조각처럼 Visibility만 막으면 행인이 커서와 바닥 사이에 끼어
	// 플레이어가 그 뒤 셀을 클릭할 수 없게 된다. 위치는 전적으로 그리드가 정하므로 물리도
	// 필요 없다.
	for (UStaticMeshComponent* Mesh : { BodyMesh.Get(), HeadMesh.Get(), NoseMesh.Get() })
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetGenerateOverlapEvents(false);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

	if (CylinderFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderFinder.Object);
	}
	if (SphereFinder.Succeeded())
	{
		HeadMesh->SetStaticMesh(SphereFinder.Object);
	}
	if (CubeFinder.Succeeded())
	{
		NoseMesh->SetStaticMesh(CubeFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HeadMaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	if (BodyMaterialFinder.Succeeded())
	{
		BodyMesh->SetMaterial(0, BodyMaterialFinder.Object);
	}
	if (HeadMaterialFinder.Succeeded())
	{
		HeadMesh->SetMaterial(0, HeadMaterialFinder.Object);
		NoseMesh->SetMaterial(0, HeadMaterialFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GhostMaterialFinder(
		TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent"));
	if (GhostMaterialFinder.Succeeded())
	{
		GhostMaterial = GhostMaterialFinder.Object;
	}

	// 그리드는 행인이 아니라 행인이 밟고 선 바닥을 트레이스해야 한다. 생성 트레이스는 폰을
	// 이미 건너뛰지만, 태그를 붙여 두면 이 클래스가 나중에 폰이 아니게 돼도 안전하다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());
}

// ---------------------------------------------------------------------------- 비주얼

void AGridNPC::GatherMeshes(TArray<UStaticMeshComponent*>& OutMeshes) const
{
	OutMeshes.Reset();
	for (UStaticMeshComponent* Mesh : { BodyMesh.Get(), HeadMesh.Get(), NoseMesh.Get() })
	{
		if (Mesh)
		{
			OutMeshes.Add(Mesh);
		}
	}
}

void AGridNPC::RefreshVisual()
{
	const double HeadDiameter = BodyDiameter * 0.65;
	constexpr double NoseSize = 12.0;

	if (BodyMesh)
	{
		BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, BodyHeight * 0.5));
		BodyMesh->SetRelativeScale3D(FVector(
			BodyDiameter / BasicShapeSize, BodyDiameter / BasicShapeSize, BodyHeight / BasicShapeSize));
	}

	if (HeadMesh)
	{
		HeadMesh->SetRelativeLocation(FVector(0.0, 0.0, BodyHeight + HeadDiameter * 0.5));
		HeadMesh->SetRelativeScale3D(FVector(HeadDiameter / BasicShapeSize));
	}

	if (NoseMesh)
	{
		// 머리 앞면 바로 바깥. +X가 액터의 정면이므로 액터를 돌리면 코가 따라 돈다.
		NoseMesh->SetRelativeLocation(FVector(
			HeadDiameter * 0.5, 0.0, BodyHeight + HeadDiameter * 0.5));
		NoseMesh->SetRelativeScale3D(FVector(NoseSize / BasicShapeSize));
	}
}

void AGridNPC::FaceMovement(const FVector& Delta)
{
	const FVector Flat(Delta.X, Delta.Y, 0.0);
	if (Flat.SizeSquared() <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	SetActorRotation(FRotator(0.0, Flat.Rotation().Yaw, 0.0));
}

void AGridNPC::ApplyGhostMaterials(bool bGhost)
{
	TArray<UStaticMeshComponent*> Meshes;
	GatherMeshes(Meshes);

	for (int32 Index = 0; Index < Meshes.Num(); ++Index)
	{
		if (bGhost)
		{
			if (GhostMID)
			{
				Meshes[Index]->SetMaterial(0, GhostMID);
			}
		}
		else if (NormalMaterials.IsValidIndex(Index))
		{
			Meshes[Index]->SetMaterial(0, NormalMaterials[Index]);
		}
	}
}

bool AGridNPC::IsOverlappingPlayer()
{
	if (!PlayerPawn.IsValid())
	{
		// APawn::Controller와 이름이 겹치지 않게 둔다. 이 폰에게는 컨트롤러가 없고, 여기서
		// 찾는 것은 플레이어의 것이다.
		const APlayerController* LocalPlayerController =
			GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (LocalPlayerController)
		{
			PlayerPawn = Cast<AGridPawn>(LocalPlayerController->GetPawn());
		}
	}

	const AGridPawn* Player = PlayerPawn.Get();
	if (!Player)
	{
		return false;
	}

	// 현재 셀과 들어가고 있는 셀을 합친 것이 다음 순간의 발자국이다. 마주 걸어오는 둘은 셀이
	// 완전히 같아지기 전부터 시각적으로 겹치므로, 진입 중인 셀까지 봐야 한 프레임 늦지 않는다.
	TArray<FIntPoint, TInlineAllocator<2>> MyCells;
	if (CurrentCell.IsSet())
	{
		MyCells.Add(CurrentCell.GetValue());
	}
	if (!Path.IsEmpty())
	{
		MyCells.AddUnique(Path[0]);
	}

	if (MyCells.IsEmpty())
	{
		return false;
	}

	if (MyCells.Contains(Player->GetCurrentCell()))
	{
		return true;
	}

	const TOptional<FIntPoint> PlayerNext = Player->GetNextCell();
	return PlayerNext.IsSet() && MyCells.Contains(PlayerNext.GetValue());
}

void AGridNPC::UpdateGhost()
{
	const bool bShouldGhost = IsOverlappingPlayer();
	if (bShouldGhost == bGhosted)
	{
		return;
	}

	if (bShouldGhost && !GhostMID)
	{
		// 불투명한 채로 지나간다. 메시를 숨기면 행인이 통째로 사라져 더 나쁘다.
		if (!bLoggedMissingGhost)
		{
			bLoggedMissingGhost = true;
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: no ghost material; staying opaque while overlapping the player."), *GetName());
		}
		return;
	}

	bGhosted = bShouldGhost;
	ApplyGhostMaterials(bGhosted);
}

void AGridNPC::DrawDebugRoute() const
{
#if ENABLE_DRAW_DEBUG
	if (!Grid || !LTTSGridDebug::ShouldDrawWorld())
	{
		return;
	}

	// 플레이어 경로가 쓰는 영구 라인 배처는 배치 ID가 하나뿐이라, 여기서 같이 쓰면 서로의
	// 선을 지운다. 행인은 매 프레임 수명 없는 선으로 그린다.
	const FVector Lift(0.0, 0.0, 20.0);
	FVector Previous = GetActorLocation();

	for (const FIntPoint& Cell : Path)
	{
		const FVector To = Grid->CellToWorld(Cell) + Lift;
		DrawDebugLine(GetWorld(), Previous, To, FColor(120, 200, 255), false, -1.0f, 0, 3.0f);
		Previous = To;
	}

	if (!Path.IsEmpty())
	{
		DrawDebugBox(GetWorld(), Previous, FVector(30.0, 30.0, 5.0), FColor(120, 200, 255), false, -1.0f, 0, 3.0f);
	}
#endif
}

// ---------------------------------------------------------------------------- 경로

FVector AGridNPC::CellStandLocation(FIntPoint Cell) const
{
	return Grid ? Grid->CellToWorld(Cell) + FVector(0.0, 0.0, HeightAboveFloor) : GetActorLocation();
}

bool AGridNPC::FindEntryCell(const FVector& FromWorld, FIntPoint& OutCell) const
{
	if (!Grid)
	{
		return false;
	}

	const FIntPoint SpawnCell = Grid->WorldToCell(FromWorld);
	if (Grid->CanPawnEnter(SpawnCell, this))
	{
		OutCell = SpawnCell;
		return true;
	}

	const bool bHasGoal = !Waypoints.IsEmpty();
	const FIntPoint FirstGoal = bHasGoal ? Waypoints[0] : FIntPoint::ZeroValue;

	// AGridActor::FindNearestWalkableCell을 쓰지 않는 이유가 둘 있다. 그 함수는 링 스캔에서
	// 처음 걸린 셀을 돌려주므로 실제로 가장 가까운 셀이 아닐 수 있고, 경로가 없는 섬으로
	// 들어가 버릴 수도 있다. 여기서는 후보가 처음 나온 반경 안에서 월드 거리로 가장 가깝고
	// 첫 경유 셀까지 길이 있는 셀을 고른다.
	for (int32 Radius = 1; Radius <= EntrySearchRadius; ++Radius)
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

				const FIntPoint Candidate(SpawnCell.X + OffsetX, SpawnCell.Y + OffsetY);
				if (!Grid->CanPawnEnter(Candidate, this))
				{
					continue;
				}

				if (bHasGoal && Candidate != FirstGoal)
				{
					TArray<FIntPoint> Probe;
					if (!Grid->FindPath(Candidate, FirstGoal, this, Probe))
					{
						continue;
					}
				}

				const double DistanceSq = FVector::DistSquared2D(Grid->CellToWorld(Candidate), FromWorld);
				if (DistanceSq < BestDistanceSq)
				{
					BestDistanceSq = DistanceSq;
					Best = Candidate;
					bFound = true;
				}
			}
		}

		if (bFound)
		{
			OutCell = Best;
			return true;
		}
	}

	return false;
}

bool AGridNPC::PlanRoute(FIntPoint FromCell, int32 FirstWaypoint, TArray<FIntPoint>& OutPath) const
{
	OutPath.Reset();

	if (!Grid)
	{
		return false;
	}

	FIntPoint From = FromCell;

	for (int32 Index = FirstWaypoint; Index < Waypoints.Num(); ++Index)
	{
		const FIntPoint Goal = Waypoints[Index];
		if (Goal == From)
		{
			continue;	// 중복 경유 셀이거나 이미 그 자리다
		}

		TArray<FIntPoint> Leg;
		if (!Grid->FindPath(From, Goal, this, Leg))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: no route from cell (%d,%d) to waypoint %d (%d,%d)."),
				*GetName(), From.X, From.Y, Index, Goal.X, Goal.Y);

			if (AGridNPCSpawner* Spawner = OwnerSpawner.Get())
			{
				Spawner->ReportRouteFailure(*this, From, Goal);
			}
			return false;
		}

		OutPath.Append(Leg);
		From = Goal;
	}

	return true;
}

// ---------------------------------------------------------------------------- 생명주기

void AGridNPC::Initialize(AGridNPCSpawner* Spawner, const TArray<FIntPoint>& InWaypoints, int32 InEntrySearchRadius)
{
	OwnerSpawner = Spawner;
	Waypoints = InWaypoints;
	EntrySearchRadius = InEntrySearchRadius;
}

void AGridNPC::BeginPlay()
{
	Super::BeginPlay();

	RefreshVisual();

	// 통과 여부는 이동체의 성질이므로 태그로 들고 다닌다. 이렇게 두면 길찾기, 매 걸음 재검사,
	// 진입 셀 탐색이 저절로 같은 답을 낸다.
	if (bPassThroughOccupants)
	{
		Tags.AddUnique(LTTSGrid::PassThroughOccupantsTag());
	}
	else
	{
		Tags.Remove(LTTSGrid::PassThroughOccupantsTag());
	}

	TArray<UStaticMeshComponent*> Meshes;
	GatherMeshes(Meshes);
	NormalMaterials.Reset();
	for (const UStaticMeshComponent* Mesh : Meshes)
	{
		NormalMaterials.Add(Mesh->GetMaterial(0));
	}

	if (GhostMaterial)
	{
		GhostMID = UMaterialInstanceDynamic::Create(GhostMaterial, this);
		if (GhostMID)
		{
			// 이 머티리얼은 Color의 알파를 불투명도로 쓴다.
			GhostMID->SetVectorParameterValue(TEXT("Color"), GhostColor);
		}
	}

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; despawning."), *GetName());
		Destroy();
		return;
	}

	const FVector SpawnLocation = GetActorLocation();
	FIntPoint EntryCell = FIntPoint::ZeroValue;
	if (!FindEntryCell(SpawnLocation, EntryCell))
	{
		const FIntPoint SpawnCell = Grid->WorldToCell(SpawnLocation);
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: spawned at cell (%d,%d) with no reachable grid cell within %d cells; despawning."),
			*GetName(), SpawnCell.X, SpawnCell.Y, EntrySearchRadius);
		Destroy();
		return;
	}

	TArray<FIntPoint> Route;
	if (!PlanRoute(EntryCell, 0, Route))
	{
		Destroy();
		return;
	}

	// 진입 셀을 첫 스텝으로 넣고 위치는 스냅하지 않는다. 그러면 평소의 스텝 루프가 스폰
	// 지점에서 그리드까지 걸어 들어가는 동작을 그대로 해 준다. 순간이동이 아니다.
	Path.Reset();
	Path.Add(EntryCell);
	Path.Append(Route);

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: entering the grid at cell (%d,%d); %d waypoint(s), %d step(s)."),
		*GetName(), EntryCell.X, EntryCell.Y, Waypoints.Num(), Path.Num());

	if (Waypoints.IsEmpty())
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: no waypoints; it will despawn as soon as it reaches the grid."), *GetName());
	}
}

void AGridNPC::HandleBlocked(float DeltaSeconds, FIntPoint BlockedCell, const FText& Reason)
{
	if (!bLoggedBlocked)
	{
		bLoggedBlocked = true;
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: cell (%d,%d) is blocked (%s); waiting for a way around."),
			*GetName(), BlockedCell.X, BlockedCell.Y, *Reason.ToString());
	}

	// 셀 사이에 떠 있으면 마지막으로 완전히 들어갔던 셀로 되돌아간다.
	if (CurrentCell.IsSet())
	{
		SetActorLocation(CellStandLocation(CurrentCell.GetValue()));
	}

	RetryTimer += DeltaSeconds;
	if (RetryTimer < BlockedRetrySeconds)
	{
		return;
	}
	RetryTimer = 0.0f;

	if (!CurrentCell.IsSet())
	{
		// 아직 그리드 밖이다. 노렸던 진입 셀이 막혔으니 다른 셀을 찾는다.
		FIntPoint EntryCell = FIntPoint::ZeroValue;
		if (!FindEntryCell(GetActorLocation(), EntryCell))
		{
			return;
		}

		TArray<FIntPoint> Route;
		if (!PlanRoute(EntryCell, NextWaypointIndex, Route))
		{
			return;
		}

		Path.Reset();
		Path.Add(EntryCell);
		Path.Append(Route);
		bLoggedBlocked = false;
		return;
	}

	TArray<FIntPoint> Route;
	if (!PlanRoute(CurrentCell.GetValue(), NextWaypointIndex, Route))
	{
		return;
	}

	Path = MoveTemp(Route);
	bLoggedBlocked = false;
}

void AGridNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Grid)
	{
		return;
	}

	if (Path.IsEmpty())
	{
		UE_LOG(LogLTTSGrid, Display, TEXT("%s: reached its destination; despawning."), *GetName());
		Destroy();
		return;
	}

	const FIntPoint NextCell = Path[0];

	// Conditional 규칙과 (통과하지 않는 설정이면) 점유는 계획 뒤에 바뀔 수 있으므로, 다음 셀에
	// 들어가기 직전마다 다시 검사한다. 플레이어 폰과 같은 규칙이다.
	FText DeniedMessage;
	if (!Grid->CanPawnEnter(NextCell, this, &DeniedMessage))
	{
		HandleBlocked(DeltaSeconds, NextCell, DeniedMessage);
		UpdateGhost();
		DrawDebugRoute();
		return;
	}

	RetryTimer = 0.0f;

	const FVector TargetLocation = CellStandLocation(NextCell);
	const FVector OldLocation = GetActorLocation();
	const FVector NewLocation = FMath::VInterpConstantTo(OldLocation, TargetLocation, DeltaSeconds, MoveSpeed);
	SetActorLocation(NewLocation);
	FaceMovement(NewLocation - OldLocation);

	if (NewLocation.Equals(TargetLocation, ArrivalEpsilon))
	{
		SetActorLocation(TargetLocation);
		CurrentCell = NextCell;
		Path.RemoveAt(0);

		if (Waypoints.IsValidIndex(NextWaypointIndex) && Waypoints[NextWaypointIndex] == NextCell)
		{
			++NextWaypointIndex;
		}
	}

	UpdateGhost();
	DrawDebugRoute();
}
