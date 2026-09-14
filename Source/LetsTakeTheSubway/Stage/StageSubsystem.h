// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stage/StageTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "StageSubsystem.generated.h"

class AGridPawn;
class AStageInfo;
class AStageZoneVolume;

/**
 * 지금 레벨이 몇 스테이지인지, 구역이 몇 개인지, 플레이어가 어느 구역에 있는지를 들고 있다.
 *
 * 월드 서브시스템인 이유는 스테이지가 곧 레벨이기 때문이다. 레벨이 열리면 새로 생기고 레벨이
 * 닫히면 함께 사라지므로 "현재 구역"을 레벨 전환 때 지워 줄 코드가 필요 없다. 게임 인스턴스
 * 쪽에 두면 반대로 레벨마다 초기화를 잊지 않아야 한다. 레벨을 넘어 남아야 하는 진행 상태(클리어한
 * 스테이지 목록 등)가 생기면 그것만 UGameInstanceSubsystem에 둔다.
 *
 * 정보를 쓰는 쪽(UI 매니저, ULocalPlayerSubsystem)은 이 서브시스템의 델리게이트에 바인딩한다.
 * 로컬 플레이어 서브시스템은 레벨을 넘어 살아남고 이 서브시스템은 레벨마다 새로 생기므로,
 * PlayerControllerChanged가 올 때마다 다시 바인딩해야 한다. 자세한 계약은 Docs/StageZone.html.
 *
 * 흐름:
 *   AStageInfo / AStageZoneVolume  --BeginPlay 등록-->  UStageSubsystem
 *   AStageZoneVolume  --Begin/EndOverlap(플레이어 폰)-->  NotifyZoneEntered / NotifyZoneLeft
 *   UStageSubsystem  --OnStageZoneChanged / OnStageStateChanged-->  UI
 */
UCLASS()
class LETSTAKETHESUBWAY_API UStageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 액터가 속한 월드의 서브시스템. 게임 월드 밖에서는 null. */
	static UStageSubsystem* Get(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// ---------------------------------------------------------------- 등록부

	void RegisterZone(AStageZoneVolume* Zone);
	void UnregisterZone(AStageZoneVolume* Zone);

	void RegisterInfo(AStageInfo* Info);
	void UnregisterInfo(AStageInfo* Info);

	// ---------------------------------------------------------------- 볼륨이 부른다

	/**
	 * 무언가가 구역 볼륨에 들어왔다. 플레이어 폰(AGridPawn)이 아니면 무시한다.
	 *
	 * 컨트롤러 유무는 보지 않는다. 폰이 스폰되는 순간의 오버랩은 Possess보다 먼저 온다.
	 */
	void NotifyZoneEntered(AStageZoneVolume* Zone, AActor* OtherActor);

	void NotifyZoneLeft(AStageZoneVolume* Zone, AActor* OtherActor);

	// ---------------------------------------------------------------- 조회

	/** 현재 스테이지·구역 상태 스냅샷. bResolved가 false면 아직 레벨 시작 판정 전이다. */
	UFUNCTION(BlueprintPure, Category = "Stage")
	const FStageState& GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Stage")
	int32 GetStageIndex() const { return State.StageIndex; }

	UFUNCTION(BlueprintPure, Category = "Stage")
	int32 GetZoneCount() const { return State.ZoneCount; }

	UFUNCTION(BlueprintPure, Category = "Stage")
	int32 GetCurrentZoneIndex() const { return State.CurrentZoneIndex; }

	UFUNCTION(BlueprintPure, Category = "Stage")
	bool IsResolved() const { return State.bResolved; }

	/** 플레이어가 지금 속한 구역 볼륨. 없으면 null. */
	UFUNCTION(BlueprintPure, Category = "Stage")
	AStageZoneVolume* GetCurrentZone() const { return CurrentZone.Get(); }

	const TArray<TWeakObjectPtr<AStageZoneVolume>>& GetZones() const { return Zones; }

	// ---------------------------------------------------------------- 이벤트

	/**
	 * 구역 번호가 실제로 바뀌었을 때만 온다.
	 *
	 * 레벨 시작 판정 때는 (0, 시작 구역)으로 한 번 온다. 방송 시점에 GetState()는 이미 새 값이다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Stage")
	FOnStageZoneChanged OnStageZoneChanged;

	/**
	 * 상태의 어느 필드든 바뀌면 온다. 레벨 시작 판정 때 반드시 한 번 온다.
	 *
	 * 같은 변화에 OnStageZoneChanged도 오면 그쪽이 먼저 온다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Stage")
	FOnStageStateChanged OnStageStateChanged;

private:
	/**
	 * 레벨 시작 판정. 이벤트에 기대지 않고 폰 위치로 겹침 목록을 다시 만든다.
	 *
	 * 폰 스폰 순간의 오버랩은 볼륨이 BeginPlay로 등록하기 전에 오므로 여기까지 오지 못했을 수
	 * 있다. 점 판정으로 덮어쓰면 이벤트가 왔든 안 왔든 결과가 같다.
	 */
	void ResolveInitialZone();

	/** 저작 실수를 로그로 알린다. 판정은 막지 않는다. */
	void ValidateLayout() const;

	/** 등록부에서 State를 다시 만들고, 판정이 끝났다면 바뀐 만큼 방송한다. */
	void RefreshState(bool bInitialBroadcast = false);

	AStageInfo* GetInfo() const;
	int32 ComputeZoneCount(const AStageInfo* Info) const;
	int32 GetMaxZoneIndex() const;

	/** 월드를 정리하는 중이면 true. 그 사이 쏟아지는 EndOverlap을 방송하지 않는다. */
	bool IsTearingDown() const;

	AGridPawn* FindPlayerPawn() const;

	TArray<TWeakObjectPtr<AStageZoneVolume>> Zones;
	TArray<TWeakObjectPtr<AStageInfo>> Infos;

	/**
	 * 플레이어 폰과 지금 겹쳐 있는 볼륨들. 들어간 순서대로이고, 마지막 것이 가장 최근이다.
	 *
	 * 볼륨이 겹쳐 있으면 마지막으로 들어간 쪽이 현재 구역이다. 거기서 나가면 남은 것 중 마지막으로
	 * 되돌아간다.
	 */
	TArray<TWeakObjectPtr<AStageZoneVolume>> Overlapped;

	/**
	 * 현재 구역. 모든 볼륨에서 나가도 비우지 않는다.
	 *
	 * 구역 사이에 틈이 있거나 탈것이 볼륨 밖으로 잠깐 나가도 UI가 "구역 없음"으로 깜빡이지 않게
	 * 하려는 것이다. 볼륨이 등록 해제될 때만 비운다.
	 */
	TWeakObjectPtr<AStageZoneVolume> CurrentZone;

	FStageState State;

	bool bResolved = false;
};
