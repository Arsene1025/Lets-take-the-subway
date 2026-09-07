// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridNPCSpawner.generated.h"

class AGridActor;
class AGridNPC;
class UStaticMeshComponent;

/**
 * 정해진 간격으로 행인을 내보내는 지점.
 *
 * 레벨에 놓고 경유 셀 목록만 적으면 된다. 셀과 셀 사이는 기존 A*가 잇는다.
 *
 * 스포너 자신은 셀에 스냅하지 않는다. 계단 입구나 승강장 가장자리처럼 그리드가 깔리지 않은
 * 곳에 두는 것이 오히려 정상적인 사용법이고, 그 경우 행인이 스스로 가장 가까운 셀로 걸어
 * 들어온다.
 *
 * 동시에 살아 있는 수를 MaxAlive로 묶는다. 행인은 목적지에 닿으면 사라지므로 슬롯이 다시
 * 빈다. 상한이 없으면 경로가 붐빌 때 행인이 끝없이 쌓인다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API AGridNPCSpawner : public AActor
{
	GENERATED_BODY()

public:
	AGridNPCSpawner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------------------------------------------------------------- 저작

	/** 만들어 낼 행인 클래스. 블루프린트 자식으로 종류를 늘릴 수 있다. */
	UPROPERTY(EditAnywhere, Category = "NPC Spawner")
	TSubclassOf<AGridNPC> NPCClass;

	/**
	 * 행인이 지나갈 셀들. 마지막 셀이 목적지이며, 거기 닿으면 사라진다.
	 *
	 * 걸음 단위 경로가 아니라 반드시 지나야 하는 지점이다. 사이는 A*가 잇는다.
	 */
	UPROPERTY(EditAnywhere, Category = "NPC Spawner")
	TArray<FIntPoint> Waypoints;

	UPROPERTY(EditAnywhere, Category = "NPC Spawner", meta = (ClampMin = 0.1))
	float SpawnInterval = 4.0f;

	/** 게임 시작 후 첫 행인이 나오기까지의 시간(초). */
	UPROPERTY(EditAnywhere, Category = "NPC Spawner", meta = (ClampMin = 0.0))
	float FirstSpawnDelay = 1.0f;

	/** 이 스포너가 낸 행인 중 동시에 살아 있을 수 있는 최대 수. */
	UPROPERTY(EditAnywhere, Category = "NPC Spawner", meta = (ClampMin = 1, ClampMax = 64))
	int32 MaxAlive = 3;

	UPROPERTY(EditAnywhere, Category = "NPC Spawner")
	bool bEnabled = true;

	/** 스포너가 그리드 밖일 때 행인이 진입 셀을 찾을 최대 반경(셀). */
	UPROPERTY(EditAnywhere, Category = "NPC Spawner", meta = (ClampMin = 0, ClampMax = 64))
	int32 EntrySearchRadius = 8;

	UPROPERTY(EditAnywhere, Category = "NPC Spawner", meta = (ClampMin = 1.0))
	float NPCMoveSpeed = 300.0f;

	/**
	 * 태어나는 행인에게 붙일 태그.
	 *
	 * Conditional 셀 규칙이 폰 태그를 읽으므로, 개찰구를 지나야 하는 경로라면 여기에
	 * HasTicket 같은 태그를 적어 둔다.
	 */
	UPROPERTY(EditAnywhere, Category = "NPC Spawner")
	TArray<FName> NPCTags;

	// ---------------------------------------------------------------- 사용

	/** 지금 하나 내보낸다. bIgnoreCap이면 MaxAlive를 무시한다(콘솔 시험용). */
	AGridNPC* SpawnOne(bool bIgnoreCap = false);

	/** 행인이 경로를 못 찾았다. 저작 오류이므로 스포너를 멈춘다. */
	void ReportRouteFailure(const AGridNPC& NPC, FIntPoint From, FIntPoint To);

	/** 이 스포너가 낸 행인 중 아직 살아 있는 수. */
	int32 GetNumAlive() const;

private:
	void OnSpawnTimer();
	void PruneAlive();

	UPROPERTY(VisibleAnywhere, Category = "NPC Spawner")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 에디터에서 스포너를 찾기 위한 표식. 게임에서는 보이지 않는다. */
	UPROPERTY(VisibleAnywhere, Category = "NPC Spawner")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	/**
	 * 살아 있는 행인들.
	 *
	 * 약참조로 든다. PIE를 끝낼 때 액터는 Destroy되지 않고 EndPlay만 받으므로 OnDestroyed
	 * 콜백은 오지 않는다. 셀 때마다 무효 항목을 걷어 내는 편이 확실하다.
	 */
	TArray<TWeakObjectPtr<AGridNPC>> Alive;

	FTimerHandle SpawnTimer;

	int32 NumSpawned = 0;
	bool bLoggedRouteFailure = false;
};
