// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GridNPC.generated.h"

class AGridActor;
class AGridNPCSpawner;
class AGridPawn;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * 그리드를 한 칸씩 걷는 행인.
 *
 * 스포너가 만들어 경로를 쥐여 주고, 마지막 경유 셀에 닿으면 스스로 사라진다. 이동 방식은
 * 플레이어 폰과 같다: 콜리전도 CharacterMovement도 없고, 위치는 셀 중심 사이의 정속 보간으로만
 * 정해지므로 계단과 경사로가 별도 코드 없이 동작한다.
 *
 * 폰인 이유는 그리드가 폰으로 말하기 때문이다. CanPawnEnter와 FindPath가 const APawn*를 받고,
 * Conditional 셀 규칙은 폰의 태그를 읽는다. 컨트롤러는 붙지 않는다.
 *
 * 기본값은 셀 점유자를 통과한다(LTTSGrid::PassThroughOccupantsTag). 행인은 퍼즐의 일부가
 * 아니므로 플레이어의 길을 막아서도, 블록에 갇혀서도 안 된다. 대신 플레이어와 겹치는 동안
 * 반투명해져서 뒤에 무엇이 있는지 가리지 않는다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API AGridNPC : public APawn
{
	GENERATED_BODY()

public:
	AGridNPC();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---------------------------------------------------------------- 저작

	/**
	 * 지나갈 셀들. 마지막 셀이 목적지이며, 거기 닿으면 사라진다.
	 *
	 * 셀과 셀 사이는 기존 A*가 잇는다. 즉 이 목록은 "반드시 지나야 하는 지점"이지 걸음 단위
	 * 경로가 아니다. 스포너가 만든 행인은 스포너의 목록으로 덮어쓴다.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid NPC")
	TArray<FIntPoint> Waypoints;

	UPROPERTY(EditAnywhere, Category = "Grid NPC", meta = (ClampMin = 1.0))
	float MoveSpeed = 300.0f;

	/** 셀 바닥에서 액터 원점까지의 거리. 0이면 발이 바닥에 닿는다. */
	UPROPERTY(EditAnywhere, Category = "Grid NPC")
	float HeightAboveFloor = 0.0f;

	/**
	 * 스폰 지점이 그리드 밖일 때 진입 셀을 찾을 최대 반경(셀).
	 *
	 * 이 안에서 못 찾으면 경고를 남기고 사라진다. 걸어서 들어가는 거리이므로 크게 잡을수록
	 * 벽을 통과해 들어올 여지도 커진다.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid NPC", meta = (ClampMin = 0, ClampMax = 64))
	int32 EntrySearchRadius = 8;

	/** 켜져 있으면 셀 점유자(플레이어, 퍼즐 블록)를 통과한다. */
	UPROPERTY(EditAnywhere, Category = "Grid NPC")
	bool bPassThroughOccupants = true;

	/** 다음 셀이 막혔을 때 경로를 다시 계산하기까지 기다리는 시간(초). */
	UPROPERTY(EditAnywhere, Category = "Grid NPC", meta = (ClampMin = 0.05))
	float BlockedRetrySeconds = 0.5f;

	// ---------------------------------------------------------------- 비주얼

	/**
	 * 플레이어와 겹치는 동안 입는 반투명 머티리얼.
	 *
	 * 기본값은 엔진의 반투명 디버그 머티리얼이다. 프로젝트 툰 마스터는 Opaque라 인스턴스로는
	 * 반투명이 되지 않는다. Color 파라미터의 알파가 불투명도다.
	 */
	UPROPERTY(EditAnywhere, Category = "Grid NPC|Ghost")
	TObjectPtr<UMaterialInterface> GhostMaterial;

	UPROPERTY(EditAnywhere, Category = "Grid NPC|Ghost")
	FLinearColor GhostColor = FLinearColor(0.75f, 0.85f, 1.0f, 0.35f);

	/** 몸통 높이(cm). 머리는 그 위에 얹힌다. */
	UPROPERTY(EditAnywhere, Category = "Grid NPC|Body", meta = (ClampMin = 20.0))
	float BodyHeight = 120.0f;

	/** 몸통 지름(cm). 셀 하나(100 cm)보다 작아야 옆 셀을 넘보지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Grid NPC|Body", meta = (ClampMin = 10.0, ClampMax = 100.0))
	float BodyDiameter = 60.0f;

	// ---------------------------------------------------------------- 조회

	/** 스포너가 스폰 직후, FinishSpawning 전에 호출한다. */
	void Initialize(AGridNPCSpawner* Spawner, const TArray<FIntPoint>& InWaypoints, int32 InEntrySearchRadius);

	/** 그리드 위에 완전히 올라선 뒤에만 값이 있다. 진입하는 동안에는 비어 있다. */
	TOptional<FIntPoint> GetCurrentCell() const { return CurrentCell; }

	/** 지금 들어가고 있는 셀 (있다면). */
	TOptional<FIntPoint> GetNextCell() const
	{
		return Path.IsEmpty() ? TOptional<FIntPoint>() : TOptional<FIntPoint>(Path[0]);
	}

	bool IsOnGrid() const { return CurrentCell.IsSet(); }
	int32 GetRemainingSteps() const { return Path.Num(); }
	AGridActor* GetGrid() const { return Grid; }

private:
	// ---------------------------------------------------------------- 내부

	/** 스폰 위치에서 걸어 들어갈 셀을 고른다. */
	bool FindEntryCell(const FVector& FromWorld, FIntPoint& OutCell) const;

	/** FromCell에서 FirstWaypoint번째 이후 경유 셀들을 차례로 잇는다. 시작 셀은 결과에 없다. */
	bool PlanRoute(FIntPoint FromCell, int32 FirstWaypoint, TArray<FIntPoint>& OutPath) const;

	void HandleBlocked(float DeltaSeconds, FIntPoint BlockedCell, const FText& Reason);

	FVector CellStandLocation(FIntPoint Cell) const;
	void FaceMovement(const FVector& Delta);
	void GatherMeshes(TArray<UStaticMeshComponent*>& OutMeshes) const;
	void RefreshVisual();

	void UpdateGhost();
	bool IsOverlappingPlayer();
	void ApplyGhostMaterials(bool bGhost);

	void DrawDebugRoute() const;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	UPROPERTY(VisibleAnywhere, Category = "Grid NPC")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Grid NPC")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Grid NPC")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	/** 진행 방향 표시. 원기둥과 구만으로는 어느 쪽을 보는지 알 수 없다. */
	UPROPERTY(VisibleAnywhere, Category = "Grid NPC")
	TObjectPtr<UStaticMeshComponent> NoseMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GhostMID;

	/** 고스트를 벗을 때 되돌릴 원래 머티리얼. GatherMeshes 순서와 같다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> NormalMaterials;

	TWeakObjectPtr<AGridNPCSpawner> OwnerSpawner;
	TWeakObjectPtr<AGridPawn> PlayerPawn;

	/** 그리드에 올라서기 전에는 비어 있다. 가짜 셀로 겹침 판정이나 재계획을 하지 않기 위해서다. */
	TOptional<FIntPoint> CurrentCell;

	/** 아직 지나가야 할 셀들. 맨 앞이 바로 다음 스텝이다. */
	TArray<FIntPoint> Path;

	int32 NextWaypointIndex = 0;
	float RetryTimer = 0.0f;
	bool bGhosted = false;
	bool bLoggedBlocked = false;
	bool bLoggedMissingGhost = false;
};
