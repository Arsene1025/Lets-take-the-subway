// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stage/StageSubsystem.h"

#include "LetsTakeTheSubway.h"
#include "Player/GridPawn.h"
#include "Stage/StageInfo.h"
#include "Stage/StageZoneVolume.h"

#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** 약참조 목록에서 대상과 이미 사라진 항목을 함께 지운다. 대상을 지웠는지 돌려준다. */
	template <typename T>
	bool RemoveWeak(TArray<TWeakObjectPtr<T>>& Array, const T* Target)
	{
		bool bRemovedTarget = false;
		Array.RemoveAll([Target, &bRemovedTarget](const TWeakObjectPtr<T>& Entry)
		{
			if (Target && Entry.Get() == Target)
			{
				bRemovedTarget = true;
				return true;
			}
			return !Entry.IsValid();
		});
		return bRemovedTarget;
	}
}

UStageSubsystem* UStageSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UStageSubsystem>() : nullptr;
}

bool UStageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// UPuzzleSubsystem과 같다. 에디터 월드에는 플레이어가 없다.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UStageSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 여기서 곧바로 판정하면 안 된다. UE 5.8의 UWorld::BeginPlay 순서는
	//   1) 월드 서브시스템 OnWorldBeginPlay (지금 여기)
	//   2) GameMode::StartPlay -> 모든 액터 BeginPlay (볼륨·StageInfo 등록, 폰 셀 정렬, 컨트롤러 바인딩)
	//   3) UWorld::OnWorldBeginPlay 방송
	// 이라서, 이 시점에는 등록부가 비어 있다. 액터 BeginPlay가 전부 끝난 3)에 판정을 건다.
	InWorld.OnWorldBeginPlay.AddWeakLambda(this, [this]()
	{
		ResolveInitialZone();
	});
}

void UStageSubsystem::Deinitialize()
{
	// 레벨이 닫힌다. UI가 해제를 잊어도 사라지는 월드의 서브시스템이 UI를 붙들지 않게 한다.
	OnStageZoneChanged.Clear();
	OnStageStateChanged.Clear();

	Zones.Reset();
	Infos.Reset();
	Overlapped.Reset();
	CurrentZone.Reset();
	bResolved = false;

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------- 등록부

void UStageSubsystem::RegisterZone(AStageZoneVolume* Zone)
{
	if (!Zone)
	{
		return;
	}

	Zones.AddUnique(Zone);

	// 판정 뒤에 스폰된 볼륨이면 구역 수가 바뀔 수 있다.
	if (bResolved)
	{
		RefreshState();
	}
}

void UStageSubsystem::UnregisterZone(AStageZoneVolume* Zone)
{
	RemoveWeak(Zones, Zone);
	RemoveWeak(Overlapped, Zone);

	if (!CurrentZone.IsValid() || CurrentZone.Get() == Zone)
	{
		CurrentZone = Overlapped.IsEmpty() ? nullptr : Overlapped.Last().Get();
	}

	if (!IsTearingDown())
	{
		RefreshState();
	}
}

void UStageSubsystem::RegisterInfo(AStageInfo* Info)
{
	if (!Info)
	{
		return;
	}

	Infos.AddUnique(Info);

	if (Infos.Num() > 1)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: this level has %d AStageInfo actors. Only the first one (%s) is used."),
			*Info->GetName(), Infos.Num(), *GetNameSafe(GetStageInfo()));
	}

	if (bResolved)
	{
		RefreshState();
	}
}

void UStageSubsystem::UnregisterInfo(AStageInfo* Info)
{
	RemoveWeak(Infos, Info);

	if (!IsTearingDown())
	{
		RefreshState();
	}
}

// ---------------------------------------------------------------------------- 오버랩

void UStageSubsystem::NotifyZoneEntered(AStageZoneVolume* Zone, AActor* OtherActor)
{
	if (!Zone || !Cast<AGridPawn>(OtherActor) || IsTearingDown())
	{
		return;
	}

	// 다시 들어온 볼륨은 맨 뒤로 보낸다. 마지막으로 들어간 쪽이 이긴다.
	RemoveWeak(Overlapped, Zone);
	Overlapped.Add(Zone);
	CurrentZone = Zone;

	RefreshState();
}

void UStageSubsystem::NotifyZoneLeft(AStageZoneVolume* Zone, AActor* OtherActor)
{
	if (!Zone || !Cast<AGridPawn>(OtherActor) || IsTearingDown())
	{
		return;
	}

	// 등록 해제된 볼륨의 뒤늦은 EndOverlap은 목록에 없으므로 여기서 걸러진다.
	if (!RemoveWeak(Overlapped, Zone))
	{
		return;
	}

	// 아직 다른 볼륨 안이면 그쪽으로 돌아간다. 전부 나갔으면 마지막 구역을 그대로 둔다.
	if (!Overlapped.IsEmpty())
	{
		CurrentZone = Overlapped.Last();
	}

	RefreshState();
}

// ---------------------------------------------------------------------------- 판정

void UStageSubsystem::ResolveInitialZone()
{
	Overlapped.Reset();
	CurrentZone.Reset();

	if (const AGridPawn* Pawn = FindPlayerPawn())
	{
		const FVector Location = Pawn->GetActorLocation();
		for (const TWeakObjectPtr<AStageZoneVolume>& Entry : Zones)
		{
			AStageZoneVolume* Zone = Entry.Get();
			if (Zone && Zone->ContainsPoint(Location))
			{
				Overlapped.Add(Zone);
			}
		}

		if (!Overlapped.IsEmpty())
		{
			CurrentZone = Overlapped.Last();
		}
		else if (!Zones.IsEmpty())
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("StageSubsystem: player pawn %s starts outside every stage zone (at %s). Current zone stays 0 until it enters one."),
				*Pawn->GetName(), *Location.ToCompactString());
		}
	}
	else
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("StageSubsystem: no AGridPawn at world begin play; the starting zone could not be resolved."));
	}

	bResolved = true;
	ValidateLayout();
	RefreshState(/*bInitialBroadcast*/ true);

	const AStageZoneVolume* Zone = CurrentZone.Get();
	UE_LOG(LogLTTSGrid, Display,
		TEXT("StageSubsystem: stage %d '%s', %d zones; player starts in zone %d '%s'."),
		State.StageIndex, *State.StageName.ToString(), State.ZoneCount,
		State.CurrentZoneIndex, Zone ? *Zone->GetDisplayName() : TEXT(""));
}

void UStageSubsystem::ValidateLayout() const
{
	const AStageInfo* Info = GetStageInfo();
	if (!Info)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("StageSubsystem: this level has no AStageInfo. Stage index is 0; place one and set StageIndex. The view follows the pawn."));
	}
	else
	{
		// 구역 카메라. 빠진 칸은 막지 않는다: 그 구역에서는 직전 시점이 유지된다.
		const int32 NumZones = ComputeZoneCount(Info);
		if (Info->ZoneCameras.IsEmpty())
		{
			// --- PAWN CAMERA DISABLED 2026-09-14 ---
			// 저작 실수가 아니라 "고정 카메라를 쓰지 않는 스테이지"라는 뜻이다. 컨트롤러가 폰 카메라를 켠다.
			UE_LOG(LogLTTSGrid, Display,
				TEXT("StageSubsystem: %s has no ZoneCameras; the view follows the pawn's spring-arm camera. Fill ZoneCameras (one per zone) to use fixed cameras."),
				*Info->GetName());
		}
		else
		{
			for (int32 Zone = 1; Zone <= NumZones; ++Zone)
			{
				if (!Info->GetZoneCamera(Zone))
				{
					UE_LOG(LogLTTSGrid, Warning,
						TEXT("StageSubsystem: %s has no camera for zone %d (ZoneCameras[%d]). Entering that zone keeps the previous view."),
						*Info->GetName(), Zone, Zone - 1);
				}
			}

			if (NumZones > 0 && Info->ZoneCameras.Num() > NumZones)
			{
				UE_LOG(LogLTTSGrid, Display,
					TEXT("StageSubsystem: %s lists %d cameras for %d zones; the extra ones are never used."),
					*Info->GetName(), Info->ZoneCameras.Num(), NumZones);
			}
		}

		// PathUI 그림 번호(2026-09-16). 비워 둔 스테이지는 PathUI를 쓰지 않는다는 뜻이라 알리지 않는다.
		if (!Info->ZonePathUIIndices.IsEmpty() && NumZones > 0 && Info->ZonePathUIIndices.Num() != NumZones)
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("StageSubsystem: %s lists %d PathUI indices for %d zones. Zones without an entry keep the previous PathUI image."),
				*Info->GetName(), Info->ZonePathUIIndices.Num(), NumZones);
		}

		for (const TObjectPtr<ACameraActor>& Camera : Info->ZoneCameras)
		{
			if (Camera && Camera->GetAutoActivatePlayerIndex() != INDEX_NONE)
			{
				UE_LOG(LogLTTSGrid, Warning,
					TEXT("StageSubsystem: zone camera %s has AutoActivateForPlayer set. Set it to Disabled, or the engine may grab it as the view at level start."),
					*Camera->GetActorNameOrLabel());
			}
		}
	}

	if (Zones.IsEmpty())
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("StageSubsystem: this level has no AStageZoneVolume. Current zone is always 0."));
		return;
	}

	// 같은 번호를 쓰는 볼륨은 넓은 구역 하나를 박스 여러 개로 덮은 것일 수 있어 막지 않고 알리기만 한다.
	TMap<int32, int32> CountByIndex;
	for (const TWeakObjectPtr<AStageZoneVolume>& Entry : Zones)
	{
		if (const AStageZoneVolume* Zone = Entry.Get())
		{
			++CountByIndex.FindOrAdd(Zone->ZoneIndex);
		}
	}

	for (const TPair<int32, int32>& Pair : CountByIndex)
	{
		if (Pair.Value > 1)
		{
			UE_LOG(LogLTTSGrid, Display,
				TEXT("StageSubsystem: zone index %d is shared by %d volumes (treated as one zone)."),
				Pair.Key, Pair.Value);
		}
	}

	const int32 MaxIndex = GetMaxZoneIndex();
	for (int32 Index = 1; Index <= MaxIndex; ++Index)
	{
		if (!CountByIndex.Contains(Index))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("StageSubsystem: no volume uses zone index %d (highest index is %d)."), Index, MaxIndex);
		}
	}

	if (Info && Info->ZoneCount > 0 && Info->ZoneCount != MaxIndex)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("StageSubsystem: %s says ZoneCount %d but the highest placed zone index is %d. Using %d."),
			*Info->GetName(), Info->ZoneCount, MaxIndex, Info->ZoneCount);
	}
}

void UStageSubsystem::RefreshState(bool bInitialBroadcast)
{
	FStageState Next;
	Next.bResolved = bResolved;

	const AStageInfo* Info = GetStageInfo();
	if (Info)
	{
		Next.StageIndex = Info->StageIndex;
		Next.StageName = Info->StageName;
	}

	Next.ZoneCount = ComputeZoneCount(Info);

	if (const AStageZoneVolume* Zone = CurrentZone.Get())
	{
		Next.CurrentZoneIndex = Zone->ZoneIndex;
		Next.CurrentZoneName = Zone->ZoneName;
	}

	const FStageState Previous = State;
	State = Next;

	// 판정 전에는 조용히 값만 갱신한다. 그때는 UI가 아직 붙지 않았거나, 붙었어도 반쪽 상태다.
	if (!bResolved)
	{
		return;
	}

	const int32 PreviousZone = bInitialBroadcast ? 0 : Previous.CurrentZoneIndex;
	if (State.CurrentZoneIndex != PreviousZone)
	{
		if (!bInitialBroadcast)
		{
			UE_LOG(LogLTTSGrid, Display, TEXT("StageSubsystem: zone %d -> %d '%s'."),
				PreviousZone, State.CurrentZoneIndex, *State.CurrentZoneName.ToString());
		}
		OnStageZoneChanged.Broadcast(PreviousZone, State.CurrentZoneIndex);
	}

	if (bInitialBroadcast || !State.IsSameAs(Previous))
	{
		OnStageStateChanged.Broadcast(State);
	}
}

AStageInfo* UStageSubsystem::GetStageInfo() const
{
	for (const TWeakObjectPtr<AStageInfo>& Entry : Infos)
	{
		if (AStageInfo* Info = Entry.Get())
		{
			return Info;
		}
	}
	return nullptr;
}

int32 UStageSubsystem::ComputeZoneCount(const AStageInfo* Info) const
{
	// 정본은 저작한 값이다. 볼륨에서 세면 실수로 빠진 볼륨 하나에 UI 숫자가 흔들린다.
	if (Info && Info->ZoneCount > 0)
	{
		return Info->ZoneCount;
	}
	return GetMaxZoneIndex();
}

int32 UStageSubsystem::GetMaxZoneIndex() const
{
	int32 MaxIndex = 0;
	for (const TWeakObjectPtr<AStageZoneVolume>& Entry : Zones)
	{
		if (const AStageZoneVolume* Zone = Entry.Get())
		{
			MaxIndex = FMath::Max(MaxIndex, Zone->ZoneIndex);
		}
	}
	return MaxIndex;
}

bool UStageSubsystem::IsTearingDown() const
{
	const UWorld* World = GetWorld();
	return !World || World->bIsTearingDown;
}

AGridPawn* UStageSubsystem::FindPlayerPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// UPuzzleSubsystem::GetGridPawn과 같은 조회. 퍼즐 서브시스템에 의존하지 않으려고 복제했다.
	if (const APlayerController* Controller = World->GetFirstPlayerController())
	{
		if (AGridPawn* Pawn = Cast<AGridPawn>(Controller->GetPawn()))
		{
			return Pawn;
		}
	}

	// 빙의 전이거나 다른 폰을 쓰는 테스트 레벨이면 월드에 있는 첫 그리드 폰을 쓴다.
	for (TActorIterator<AGridPawn> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}
