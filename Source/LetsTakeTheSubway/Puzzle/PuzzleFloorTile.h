// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/GridFootprint.h"
#include "PuzzleFloorTile.generated.h"

class AGridActor;
class APuzzleBlock;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * 블록이 그 위에 완전히 올라서면 반응하는 정사각형 바닥 패치.
 *
 * 회전판과 엘리베이터 구조물이 공유하는 뼈대다. 둘은 반응이 다를 뿐(하나는 공간을 돌리고
 * 하나는 층을 옮긴다) 나머지가 같다: 셀에 맞춰 놓이고, 자기 영역을 계산하고, 무엇이 자기
 * 안에 완전히 들어왔는지 판정하고, 블록이 드래그를 마칠 때 알림을 받는다.
 *
 * 정사각형인 이유는 회전판에서 왔다. 회전판은 돌고 나서도 자기 내용물을 담아야 하므로
 * 정사각형이어야 하고, 구조물은 4x4 엘리베이터를 담기만 하면 되므로 정사각형이어도 무방하다.
 *
 * 타일끼리 겹쳐서는 안 된다. 겹치면 그 사이에 멈춘 블록을 양쪽이 자기 것이라 주장하고,
 * 어느 쪽이 이기는지를 등록 순서가 정하게 된다.
 */
UCLASS(Abstract, HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleFloorTile : public AActor
{
	GENERATED_BODY()

public:
	APuzzleFloorTile();

	/** 한 변의 길이(셀 단위). */
	UPROPERTY(EditAnywhere, Category = "Floor Tile", meta = (ClampMin = 2, ClampMax = 16))
	int32 SizeInCells = 4;

	// ---------------------------------------------------------------- 아트
	//
	// 블록의 ArtMesh 슬롯(APuzzleBlock)과 같은 규약이다. 다만 타일은 프록시를 남길 이유가
	// 없어 컴포넌트를 따로 두지 않고 PadMesh의 메시를 갈아 끼운다 -- 패드는 애초에 콜리전이
	// 없어(생성자) 커서 판정에도, 그리드 생성에도 관여하지 않는다.

	/**
	 * 그레이박스 패드 대신 보여 줄 스태틱 메시. 비우면 지금까지처럼 납작한 큐브를 그린다.
	 *
	 * 파생 클래스가 생성자에서 기본값을 잡아 두므로 레벨에 놓기만 하면 아트가 따라온다.
	 * 인스턴스마다 다른 메시를 쓰고 싶으면 여기서 덮어쓴다.
	 */
	UPROPERTY(EditAnywhere, Category = "Floor Tile|Art")
	TObjectPtr<UStaticMesh> ArtMesh;

	/**
	 * 아트 메시를 타일 원점(영역 중심의 바닥)에 맞추는 보정.
	 *
	 * 스케일은 여기에 넣지 않아도 된다: XY는 영역 크기에 맞춰 코드가 계산하고, 이 값의
	 * 스케일은 그 위에 곱해진다. 피벗이 바닥 중앙이 아닌 메시의 Z 보정이 주된 용도다.
	 */
	UPROPERTY(EditAnywhere, Category = "Floor Tile|Art")
	FTransform ArtMeshOffset = FTransform::Identity;

	/**
	 * 아트 메시의 슬롯 0에 씌울 머티리얼. 비우면 메시 자신의 머티리얼을 그대로 쓴다.
	 *
	 * 블록의 ArtFallbackMaterial과 달리 **덮어쓴다**. 층 구분과 도킹 상태를 색으로 읽는
	 * 것이 타일의 기능이라(엘리베이터 구조물의 Idle/Ready), 슬롯이 이미 칠해져 있는지와
	 * 무관하게 코드가 색을 쥐고 있어야 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Floor Tile|Art")
	TObjectPtr<UMaterialInterface> ArtMaterial;

	// ---------------------------------------------------------------- 조회

	FGridRect GetRegion() const { return Region; }

	/** 영역 중심의 월드 위치, 바닥 높이 기준. 회전축이자 좌석 위치다. */
	FVector GetPivotWorld() const { return PivotWorld; }

	/**
	 * 이 타일이 놓인 층의 바닥 높이.
	 *
	 * 셀 하나에는 바닥이 하나뿐이라 지금까지는 층이라는 개념이 필요 없었다. 엘리베이터
	 * 구조물을 층마다 하나씩, 같은 XY에 겹쳐 두기 시작하면서 필요해졌다: 두 구조물을
	 * 가르는 것은 XY가 아니라 높이다.
	 */
	double GetFloorZ() const { return PivotWorld.Z; }

	bool IsDisabled() const { return bDisabled; }

	/**
	 * 블록의 모든 셀이 영역 안에 들어 있으면(완전히 포함하면) true.
	 *
	 * 가상 함수인 이유는 같은 XY를 층마다 나눠 가지는 타일이 있기 때문이다. 엘리베이터
	 * 구조물은 여기에 "블록이 나와 같은 층에 있는가"를 더한다.
	 */
	virtual bool FullyContains(const APuzzleBlock& Block) const;

	/** 블록이 영역 일부를 덮지만 가장자리 밖으로 삐져나와 있으면(걸침) true. */
	bool Straddles(const APuzzleBlock& Block) const;

	/**
	 * 타일이 지금 무언가를 하고 있어 입력을 막아야 하면 true.
	 *
	 * 회전판은 도는 동안, 구조물은 엘리베이터가 층을 오가는 동안이다.
	 */
	virtual bool IsBusy() const { return false; }

	/**
	 * 블록이 이 타일 안에서 드래그를 마치고 멈췄다.
	 *
	 * 서브시스템이 블록을 완전히 담는 타일을 찾아 한 번 부른다. 회전판은 여기서 공간을
	 * 돌리고, 엘리베이터 구조물은 도킹한다.
	 */
	virtual void OnBlockCameToRest(APuzzleBlock& Block) {}

	// ---------------------------------------------------------------- 생명주기

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** 패드와 장식의 크기를 SizeInCells에 맞춘다. ArtMesh가 있으면 그쪽을 그린다. */
	virtual void RefreshVisual();

	/** BeginPlay 로그 끝에 붙는 한 마디. 파생 클래스가 자기 설정을 알린다. */
	virtual FString DescribeTile() const { return FString(); }

	/** 영역이 확정되고 등록까지 끝난 뒤. 파생 클래스의 추가 초기화 자리다. */
	virtual void OnTileReady() {}

	/**
	 * 이 타일이 어느 층에 놓인 것으로 칠지.
	 *
	 * 기본은 지금까지처럼 영역 첫 셀의 바닥 높이다. 셀 하나에 바닥이 하나뿐이므로 그것이
	 * 곧 층이었다. 샤프트 위에 놓이는 엘리베이터 구조물만은 다르다: 샤프트 셀은 아래층
	 * 높이로 구워지므로, 위층 구조물까지 그 값을 쓰면 아래층으로 끌려 내려간다.
	 *
	 * @param PlacedZ 디자이너가 레벨에 놓은 높이. 셀에서 답을 얻을 수 없을 때의 근거다.
	 */
	virtual double ResolveFloorZ(const AGridActor& InGrid, FIntPoint MinCell, double PlacedZ) const;

	/**
	 * 영역이 겹치는 다른 타일과 나란히 있어도 되는지.
	 *
	 * 기본은 false다 -- 겹친 두 타일은 그 사이에 멈춘 블록을 서로 자기 것이라 주장하고,
	 * 어느 쪽이 이기는지를 등록 순서가 정하게 된다. 층이 다른 엘리베이터 구조물만은 예외다.
	 */
	virtual bool CanCoexistWith(const APuzzleFloorTile& Other) const { return false; }

	AGridActor* GetGrid() const { return Grid; }

	UPROPERTY(VisibleAnywhere, Category = "Floor Tile")
	TObjectPtr<USceneComponent> SceneRoot;

	/**
	 * 영역을 표시하는 평평한 패드. 클릭 트레이스와 바닥 트레이스가 통과하도록 콜리전이 없다.
	 *
	 * ArtMesh가 채워져 있으면 이 컴포넌트가 그 메시를 그린다. 컴포넌트를 따로 두지 않는
	 * 이유는 패드에 남겨 둘 것이 없기 때문이다 -- 블록의 프록시 큐브와 달리 여기에는
	 * 커서가 잡을 콜리전이 애초에 없다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Floor Tile")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	FGridRect Region;

	FVector PivotWorld = FVector::ZeroVector;

	bool bDisabled = false;

private:
	/** 아트 메시를 영역 크기에 맞춰 패드에 적용한다. RefreshVisual에서만 부른다. */
	void ApplyArtVisual(double CellSize);

	/** 생성자가 잡아 둔 그레이박스 큐브. ArtMesh를 비우면 여기로 되돌아간다. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> GreyBoxMesh;
};
