// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridNPCSpawner.generated.h"

class AGridActor;
class AGridNPC;
class UStaticMeshComponent;

/** 스포너가 언제 행인을 내보내는지. */
UENUM(BlueprintType)
enum class ENPCSpawnMode : uint8
{
	/** 일정 간격으로 계속. 배경을 채우는 통행량이다. */
	Interval	UMETA(DisplayName = "Interval"),

	/**
	 * 부르면 그때만. 열차가 문을 열 때처럼 사건에 맞춰 쏟아져 나와야 하는 경우다.
	 *
	 * 타이머를 걸지 않으므로 SpawnBurst나 콘솔 명령이 없으면 아무 일도 일어나지 않는다.
	 */
	OnDemand	UMETA(DisplayName = "On demand")
};

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

	/**
	 * 간격 스폰인지, 불렀을 때만 나오는지.
	 *
	 * 열차 하차용 스포너는 OnDemand로 두고 열차의 정차역이 이 스포너를 가리킨다. 스포너를
	 * 열차가 아니라 승강장에 두는 이유는 경유 셀이 역마다 다르기 때문이다.
	 */
	UPROPERTY(EditAnywhere, Category = "NPC Spawner")
	ENPCSpawnMode SpawnMode = ENPCSpawnMode::Interval;

	UPROPERTY(EditAnywhere, Category = "NPC Spawner",
		meta = (ClampMin = 0.1, EditCondition = "SpawnMode == ENPCSpawnMode::Interval", EditConditionHides))
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

	/**
	 * Count명을 Spacing초 간격으로 줄지어 내보낸다. MaxAlive는 무시한다.
	 *
	 * 열차에서 내리는 무리가 이것이다. 상한을 무시하는 이유는 하차 인원이 승강장을 배회하는
	 * 배경 통행량과는 다른 사건이기 때문이다 -- 상한에 걸려 두 명만 내리면 열차가 비어
	 * 보인다. 한 명씩 간격을 두는 것은 문 하나에서 세 명이 같은 프레임에 겹쳐 나오지 않게
	 * 하기 위해서다.
	 */
	void SpawnBurst(int32 Count, float Spacing = 0.6f);

	/** 행인이 경로를 못 찾았다. 저작 오류이므로 스포너를 멈춘다. */
	void ReportRouteFailure(const AGridNPC& NPC, FIntPoint From, FIntPoint To);

	/** 이 스포너가 낸 행인 중 아직 살아 있는 수. */
	int32 GetNumAlive() const;

private:
	void OnSpawnTimer();

	/** 버스트 타이머 콜백. 남은 인원을 하나씩 줄여 가며 내보낸다. */
	void OnBurstTimer();

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

	/** 버스트는 자기 타이머를 쓴다. 간격 스폰과 섞이면 둘 중 하나가 상대를 끊는다. */
	FTimerHandle BurstTimer;

	int32 BurstRemaining = 0;

	int32 NumSpawned = 0;
	bool bLoggedRouteFailure = false;
};
