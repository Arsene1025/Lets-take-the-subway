// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/GridNPCSpawner.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"
#include "NPC/GridNPC.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AGridNPCSpawner::AGridNPCSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	NPCClass = AGridNPC::StaticClass();

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MarkerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarkerMesh"));
	MarkerMesh->SetupAttachment(SceneRoot);
	MarkerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	MarkerMesh->SetGenerateOverlapEvents(false);
	MarkerMesh->SetRelativeLocation(FVector(0.0, 0.0, 50.0));
	MarkerMesh->SetRelativeScale3D(FVector(0.6, 0.6, 1.0));
	// 저작용 표식일 뿐이다. 플레이 중에 보이면 그리드 밖 허공에 뜬 콘이 된다.
	MarkerMesh->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeFinder.Succeeded())
	{
		MarkerMesh->SetStaticMesh(ConeFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	if (MaterialFinder.Succeeded())
	{
		MarkerMesh->SetMaterial(0, MaterialFinder.Object);
	}

	// 그리드는 스포너 표식이 아니라 그 아래 바닥을 트레이스해야 한다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

	// 스포너가 언로드되면 그 경로의 행인이 조용히 끊긴다. 퍼즐 조각과 같은 이유로 항상 로드해 둔다.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

// ---------------------------------------------------------------------------- 조회

int32 AGridNPCSpawner::GetNumAlive() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AGridNPC>& NPC : Alive)
	{
		if (NPC.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

void AGridNPCSpawner::PruneAlive()
{
	Alive.RemoveAll([](const TWeakObjectPtr<AGridNPC>& NPC) { return !NPC.IsValid(); });
}

// ---------------------------------------------------------------------------- 스폰

AGridNPC* AGridNPCSpawner::SpawnOne(bool bIgnoreCap)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	PruneAlive();
	if (!bIgnoreCap && Alive.Num() >= MaxAlive)
	{
		return nullptr;
	}

	TSubclassOf<AGridNPC> ClassToSpawn = NPCClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AGridNPC::StaticClass();
	}

	// 스포너를 기울여 놓았더라도 행인은 똑바로 선다. 방향은 첫 걸음이 정한다.
	const FTransform SpawnTransform(FRotator::ZeroRotator, GetActorLocation());

	// 지연 스폰이라야 BeginPlay가 경로를 이미 쥔 채로 돌아간다. 충돌 처리를 AlwaysSpawn으로
	// 못박는 이유는 APawn의 기본값이 겹치면 스폰을 포기하는 것이기 때문이다. 스포너는 벽
	// 안이나 허공에도 놓일 수 있다.
	AGridNPC* NPC = World->SpawnActorDeferred<AGridNPC>(
		ClassToSpawn,
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!NPC)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: failed to spawn an NPC."), *GetName());
		return nullptr;
	}

	NPC->MoveSpeed = NPCMoveSpeed;
	for (const FName& Tag : NPCTags)
	{
		NPC->Tags.AddUnique(Tag);
	}
	NPC->Initialize(this, Waypoints, EntrySearchRadius);

	NPC->FinishSpawning(SpawnTransform);

	// BeginPlay에서 경로를 못 찾으면 스스로 사라진다. 그 경우 목록에 넣지 않는다.
	if (!IsValid(NPC))
	{
		return nullptr;
	}

	Alive.Add(NPC);
	++NumSpawned;

	UE_LOG(LogLTTSGrid, Verbose,
		TEXT("%s: spawned %s (%d alive, %d total)."), *GetName(), *NPC->GetName(), Alive.Num(), NumSpawned);

	return NPC;
}

void AGridNPCSpawner::OnSpawnTimer()
{
	PruneAlive();
	if (Alive.Num() >= MaxAlive)
	{
		return;
	}

	SpawnOne();
}

void AGridNPCSpawner::ReportRouteFailure(const AGridNPC& NPC, FIntPoint From, FIntPoint To)
{
	if (bLoggedRouteFailure)
	{
		return;
	}
	bLoggedRouteFailure = true;

	// 행인은 점유자를 통과하므로 경로가 없다는 것은 저작 오류다. 간격마다 태어났다 사라지기를
	// 반복하는 대신 스포너를 세우고 한 번만 알린다.
	UE_LOG(LogLTTSGrid, Error,
		TEXT("%s: %s could not route from cell (%d,%d) to (%d,%d); disabling this spawner. Check the waypoints."),
		*GetName(), *NPC.GetName(), From.X, From.Y, To.X, To.Y);

	bEnabled = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimer);
	}
}

// ---------------------------------------------------------------------------- 생명주기

void AGridNPCSpawner::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; the spawner is idle."), *GetName());
		return;
	}

	if (Waypoints.IsEmpty())
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: no waypoints; the spawner is disabled. Add at least one destination cell."), *GetName());
		bEnabled = false;
		return;
	}

	// 잘못 적힌 셀은 첫 행인이 태어난 뒤가 아니라 지금 알린다. 배치 실수는 로그 한 줄로
	// 잡히는 편이 낫다.
	int32 NumBad = 0;
	for (int32 Index = 0; Index < Waypoints.Num(); ++Index)
	{
		const FIntPoint Cell = Waypoints[Index];
		if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
		{
			++NumBad;
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: waypoint %d (%d,%d) is not a walkable cell."), *GetName(), Index, Cell.X, Cell.Y);
		}
	}

	if (NumBad > 0)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: %d of %d waypoints are unusable; the spawner is disabled."),
			*GetName(), NumBad, Waypoints.Num());
		bEnabled = false;
		return;
	}

	if (!bEnabled)
	{
		UE_LOG(LogLTTSGrid, Display, TEXT("%s: disabled by the designer; no NPCs will spawn."), *GetName());
		return;
	}

	const FIntPoint SpawnCell = Grid->WorldToCell(GetActorLocation());
	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: at cell (%d,%d), %d waypoints valid, every %.1f s, up to %d alive; timer started."),
		*GetName(), SpawnCell.X, SpawnCell.Y, Waypoints.Num(), SpawnInterval, MaxAlive);

	// 타이머는 BeginPlay에서만 건다. OnConstruction은 에디터 월드에서도 돌기 때문이다.
	GetWorldTimerManager().SetTimer(
		SpawnTimer, this, &AGridNPCSpawner::OnSpawnTimer, SpawnInterval, true, FirstSpawnDelay);
}

void AGridNPCSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);

	// 스포너만 지워진 경우에만 자기 행인을 거둔다. 월드 자체가 끝나는 중이라면 엔진이 이미
	// 모두에게 EndPlay를 돌리고 있으므로 여기서 손대면 순서만 어지럽힌다.
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		for (const TWeakObjectPtr<AGridNPC>& NPC : Alive)
		{
			if (AGridNPC* Alive_NPC = NPC.Get())
			{
				Alive_NPC->Destroy();
			}
		}
	}

	Alive.Reset();

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- 콘솔

namespace
{
	/**
	 * 간격을 기다리지 않고 행인을 하나 내보낸다.
	 *
	 * 동시 수 상한도 무시한다. 경로를 눈으로 확인하거나 붐빌 때의 모습을 보려면 간격 설정을
	 * 바꿔 PIE를 다시 켜는 것보다 이쪽이 빠르다.
	 */
	void SpawnNPCCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.SpawnNPC: run this in play mode."));
			return;
		}

		const FString Filter = Args.IsValidIndex(0) ? Args[0] : FString();

		int32 Matched = 0;
		for (TActorIterator<AGridNPCSpawner> It(World); It; ++It)
		{
			AGridNPCSpawner* Spawner = *It;
			if (!Spawner || (!Filter.IsEmpty() && !Spawner->GetName().Contains(Filter)))
			{
				continue;
			}

			++Matched;
			if (!Spawner->SpawnOne(true))
			{
				UE_LOG(LogLTTSGrid, Warning,
					TEXT("ltts.SpawnNPC: %s could not spawn an NPC."), *Spawner->GetName());
			}
		}

		if (Matched == 0)
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.SpawnNPC: no NPC spawner matching '%s'."), *Filter);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSpawnNPCCommand(
	TEXT("ltts.SpawnNPC"),
	TEXT("Spawn one NPC now, ignoring the interval and the alive cap: ltts.SpawnNPC [name substring]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnNPCCommand));
