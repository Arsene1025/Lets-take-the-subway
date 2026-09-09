// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GridTestGameMode.generated.h"

/** 그리드 폰, 컨트롤러, HUD를 묶어 주고 스테이지 클리어를 보고한다. */
UCLASS()
class LETSTAKETHESUBWAY_API AGridTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGridTestGameMode();

	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleStageClear(APawn* Pawn, FIntPoint Cell);
};
