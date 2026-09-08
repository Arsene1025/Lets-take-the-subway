// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PuzzleSubsystem.generated.h"

class AGridActor;
class AGridPawn;
class AGridPlayerController;
class AGridTrain;
class APuzzleBlock;
class APuzzleElevatorBlock;
class APuzzleElevatorDock;
class APuzzleLever;
class APuzzleRegion;
class APuzzleRotatingObstacle;
class APuzzleFloorTile;
class APuzzleRotationTile;
struct FGridRect;

/**
 * 레벨에 있는 퍼즐 조각들과, 조각 하나를 넘어 여러 조각에 걸치는 규칙들을
 * 관리한다.
 *
 * 셀 점유는 셀의 속성이고 모든 이동체가 그 혜택을 보므로 그리드 액터가 소유한다. 대신
 * 여기에는 퍼즐 전체를 봐야 하는 것들이 산다: 어떤 블록과 타일이 존재하는지, 지금
 * 애니메이션이 진행 중이라 입력을 무시해야 하는지, 그리고 "블록이 정지했는데 타일이
 * 그것을 돌리고 싶어 하는가" 규칙.
 *
 * 등록부를 여기에 두면 플레이어가 마우스를 움직이는 동안 드래그 코드가 액터 이터레이터로
 * 레벨을 훑을 필요도 없어진다.
 */
UCLASS()
class LETSTAKETHESUBWAY_API UPuzzleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 액터가 속한 월드의 서브시스템. 게임 월드 밖에서는 null. */
	static UPuzzleSubsystem* Get(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	void RegisterBlock(APuzzleBlock* Block);
	void UnregisterBlock(APuzzleBlock* Block);

	void RegisterTile(APuzzleFloorTile* Tile);
	void UnregisterTile(APuzzleFloorTile* Tile);

	/**
	 * 돌아가는 장애물은 밀 수 있는 블록과 따로 관리한다.
	 *
	 * 같은 클래스에서 파생되지만, 블록을 밀어낼 대상으로 다루는 코드는 어느 것도 장애물을
	 * 집어서는 안 된다: 드래그 코드도, 위에 무엇이 서 있는지 판단하는 회전 타일도, 동승
	 * 블록을 찾는 장애물 자신도.
	 */
	void RegisterObstacle(APuzzleRotatingObstacle* Obstacle);
	void UnregisterObstacle(APuzzleRotatingObstacle* Obstacle);

	void RegisterLever(APuzzleLever* Lever);
	void UnregisterLever(APuzzleLever* Lever);

	/**
	 * 퍼즐 구간. 조각을 담는 영역이므로 조각 목록과 따로 둔다.
	 *
	 * 구간은 등록만 되고 서로를 모른다. 소속 판정은 블록이 BeginPlay에서 한 번 하고
	 * 그 결과를 들고 다닌다.
	 */
	void RegisterRegion(APuzzleRegion* Region);
	void UnregisterRegion(APuzzleRegion* Region);

	/**
	 * 열차. 퍼즐 조각은 아니지만 커서 앞을 가리는 큰 물체라 여기 등록한다.
	 *
	 * GetBlockActors가 이것들을 함께 돌려주므로, 바닥을 찾는 두 번째 트레이스가 차체를
	 * 통과해 뒤쪽 승강장 셀을 짚는다. 등록하지 않으면 열차가 서 있는 동안 그 뒤 승강장을
	 * 클릭할 수 없다.
	 */
	void RegisterVehicle(AGridTrain* Vehicle);
	void UnregisterVehicle(AGridTrain* Vehicle);

	int32 GetNumVehicles() const { return Vehicles.Num(); }

	/**
	 * 사각형 전체를 담는 첫 번째 구간. 없으면 null.
	 *
	 * 구간은 겹치지 않는 것이 전제이므로 "첫 번째"는 사실상 "유일한"이다. 겹쳐 있으면
	 * 구간 자신이 BeginPlay에서 경고한다.
	 */
	APuzzleRegion* FindRegionContaining(const FGridRect& Rect) const;

	/**
	 * 이 엘리베이터를 완전히 담고 있는 승강 구조물. 없으면 null.
	 *
	 * 구조물도 바닥 타일이라 Tiles에 들어 있다. 컨트롤러가 엘리베이터 클릭을 처리할 때
	 * "지금 이 차체가 구조물 위에 있는가"를 여기서 묻는다.
	 */
	APuzzleElevatorDock* FindDockUnder(const APuzzleElevatorBlock& Elevator) const;

	const TArray<TWeakObjectPtr<APuzzleBlock>>& GetBlocks() const { return Blocks; }

	const TArray<TWeakObjectPtr<APuzzleRotatingObstacle>>& GetObstacles() const { return Obstacles; }

	int32 GetNumBlocks() const { return Blocks.Num(); }

	int32 GetNumObstacles() const { return Obstacles.Num(); }

	/**
	 * 무언가가 애니메이션 중인 동안 true.
	 *
	 * 회전 중의 입력은 큐에 넣지 않고 버린다: 플레이어는 조각들이 향해 가는 셀을 볼 수
	 * 없으므로, 회전 도중에 한 클릭은 회전이 끝난 뒤에 했을 클릭과 거의 언제나 다른
	 * 것이다.
	 */
	bool IsInputLocked() const;

	APuzzleBlock* FindBlockAtCell(const AGridActor& Grid, FIntPoint Cell) const;

	/**
	 * 커서에 맞을 수 있는 모든 조각을 일반 액터로 돌려준다. 그것들 전부를 지나쳐 뒤쪽 바닥을
	 * 봐야 하는 트레이스용이다.
	 *
	 * 블록뿐 아니라 장애물과 레버도 포함한다. 카메라가 가파르게 내려다보므로 서 있는 것은
	 * 무엇이든 뒤쪽 바닥 셀을 가리는데, 그 셀들은 계속 클릭할 수 있어야 한다.
	 */
	void GetBlockActors(TArray<AActor*>& OutActors) const;

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	/**
	 * 카메라와 폰 사이에 서 있는 블록을 찾아내 납작하게 만든다.
	 *
	 * 카메라가 가파른 각도로 고정돼 있어 3미터 엘리베이터는 뒤쪽 바닥을 대략 두 셀 가린다.
	 * 카메라를 옮기는 대신, 방해하는 블록을 방해가 되는 동안 슬랩으로 눌러 둔다.
	 */
	void UpdateOcclusion(const FVector& CameraLocation, const APawn* Pawn, float SweepRadius);

	int32 GetNumOccludingBlocks() const { return NumOccludingBlocks; }
#endif

	/** 폰이 서 있는 셀과, 있다면 걸어 들어가고 있는 셀. */
	void GetPawnReservedCells(TArray<FIntPoint>& OutCells) const;

	AGridPawn* GetGridPawn() const;
	AGridPlayerController* GetGridController() const;

	/** 보여 줄 컨트롤러가 있다면 화면의 피드백 메시지 줄로 메시지를 보낸다. */
	void ShowFeedback(const FString& Message, const FLinearColor& Color) const;

	/**
	 * 블록이 드래그의 마지막 걸음을 마쳤다. 이제 회전 타일 안에 완전히 들어가 있다면 그
	 * 타일이 공간을 돌린다.
	 */
	void NotifyBlockCameToRest(APuzzleBlock* Block);

private:
	TArray<TWeakObjectPtr<APuzzleBlock>> Blocks;
	TArray<TWeakObjectPtr<APuzzleFloorTile>> Tiles;
	TArray<TWeakObjectPtr<APuzzleRotatingObstacle>> Obstacles;
	TArray<TWeakObjectPtr<APuzzleLever>> Levers;
	TArray<TWeakObjectPtr<APuzzleRegion>> Regions;
	TArray<TWeakObjectPtr<AGridTrain>> Vehicles;

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	int32 NumOccludingBlocks = 0;
#endif
};
