// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Sound/SoundKeys.h"
#include "GridTestGameMode.generated.h"

/** 그리드 폰, 컨트롤러, HUD를 묶어 주고 스테이지 클리어를 보고한다. */
UCLASS()
class LETSTAKETHESUBWAY_API AGridTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGridTestGameMode();

	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 이 레벨에 있는 동안 Virtual Shadow Map 캐시(r.Shadow.Virtual.Cache)를 끈다.
	 *
	 * 열차·에스컬레이터·회전 기둥·버스처럼 계속 움직이는 물체의 그림자가 바닥에 잔상으로 남는 것을
	 * 막는다. 스켈레탈 메시는 Shadow Cache Invalidation Behavior를 Always로 해도 잡히지 않았고,
	 * 캐시를 끄면 사라지는 것을 Start·Arrived 맵에서 확인했다(2026-09-15).
	 * 레벨을 떠날 때(EndPlay) 원래 값으로 되돌리므로, 캐시가 필요한 레벨은 이 값을 끄면 된다.
	 *
	 * BeginPlay가 아니라 PostInitializeComponents에서 끈다. ABusStopGameMode처럼 부모 BeginPlay를
	 * 건너뛰는 하위 게임 모드에서도 빠지지 않게 하기 위해서다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Rendering")
	bool bDisableShadowCacheWhileHere = true;

	/**
	 * StageClear 셀을 밟았을 때 내는 소리(DA_SoundLibrary 키).
	 *
	 * 레벨에 bClearOnStageClear를 켠 엘리베이터 구조물이 있으면 내지 않는다. 그 레벨에서 셀은 "출구가
	 * 열렸다"는 뜻일 뿐이고, 클리어 소리는 구조물이 엔딩 승강을 마칠 때 낸다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	FName ClearSoundKey = LTTSSoundKeys::StageClear;

private:
	UFUNCTION()
	void HandleStageClear(APawn* Pawn, FIntPoint Cell);

	/** bDisableShadowCacheWhileHere: PostInitializeComponents에서 캐시를 끄고, EndPlay에서 저장해 둔 값으로 되돌린다. */
	void DisableShadowCache();
	void RestoreShadowCache();

	/** 캐시를 끄기 전 r.Shadow.Virtual.Cache 값. 되돌릴 게 없으면 -1. */
	int32 SavedShadowCacheValue = -1;
};
