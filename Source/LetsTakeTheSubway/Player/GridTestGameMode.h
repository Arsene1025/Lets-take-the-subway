// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GridTestGameMode.generated.h"

/** Wires the grid pawn, controller and HUD together, and reports stage clear. */
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
