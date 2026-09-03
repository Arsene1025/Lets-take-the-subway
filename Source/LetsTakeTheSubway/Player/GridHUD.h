// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GridHUD.generated.h"

/** Text-only debug overlay. Gated on ltts.GridDebug. */
UCLASS()
class LETSTAKETHESUBWAY_API AGridHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
