// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GridHUD.generated.h"

/** 텍스트 전용 디버그 오버레이. ltts.GridDebug로 켜고 끈다. */
UCLASS()
class LETSTAKETHESUBWAY_API AGridHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
