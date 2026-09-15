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

	virtual void BeginPlay() override;

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
};
