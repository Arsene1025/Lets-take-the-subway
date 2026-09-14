// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageZoneVolume.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;

/**
 * 스테이지 안의 구역 하나를 덮는 박스. 플레이어가 들어오면 UStageSubsystem에 알린다.
 *
 * 구역 판정을 셀 도착(AGridActor::NotifyPawnEnteredCell)이 아니라 오버랩으로 하는 이유는 탈것에
 * 있다. 열차·에스컬레이터·엘리베이터에 실려 가는 동안 폰은 셀에 도착하지 않지만 위치는 계속
 * 바뀌므로, 오버랩은 그 사이 구역 경계를 넘는 것도 잡는다.
 *
 * 폰 쪽은 몸체가 아니라 폰에 붙은 작은 프로브(AGridPawn::ZoneProbe)가 이 박스와 겹친다. 그래서
 * 구역이 바뀌는 순간은 "공의 가장자리가 닿을 때"가 아니라 "공의 중심이 경계를 넘을 때"다.
 * 박스는 셀 경계에 맞춰 놓는다.
 *
 * 볼륨끼리 겹쳐도 된다. 겹친 곳에서는 마지막으로 들어간 볼륨이 현재 구역이다. 넓거나 ㄱ자인
 * 구역은 같은 ZoneIndex를 준 박스 여러 개로 덮는다.
 *
 * 콜리전 설정은 코드가 정한다(StageZone 전용 채널, 폰에만 Overlap). 디테일 패널에서 바꾸지 못하게
 * Collision 카테고리를 숨겼다.
 */
UCLASS(HideCategories = (Rendering, Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API AStageZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	AStageZoneVolume();

	// ---------------------------------------------------------------- 저작

	/**
	 * 구역 번호. 1부터 센다.
	 *
	 * 스테이지의 구역 수는 AStageInfo::ZoneCount가 정하고, 비워 두면 이 값들 중 가장 큰 것이 된다.
	 * 1부터 빈 번호 없이 채운다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Zone", meta = (ClampMin = 1))
	int32 ZoneIndex = 1;

	/** UI에 보여 줄 구역 이름(예: 동쪽 대합실). 비워 둬도 동작한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Zone")
	FText ZoneName;

	// ---------------------------------------------------------------- 조회

	/** 월드 좌표 한 점이 박스 안인지. 레벨 시작 판정이 쓴다. 회전·스케일된 박스도 맞게 잰다. */
	bool ContainsPoint(const FVector& WorldLocation) const;

	/** 로그용 이름. ZoneName이 비었으면 액터 이름. */
	FString GetDisplayName() const;

	UBoxComponent* GetBox() const { return Box; }

	// ---------------------------------------------------------------- 생명주기

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 구역 범위. Shape 카테고리의 Box Extent로 크기를 정한다. */
	UPROPERTY(VisibleAnywhere, Category = "Stage Zone")
	TObjectPtr<UBoxComponent> Box;
};
