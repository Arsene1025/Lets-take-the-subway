// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/GridTestGameMode.h"
#include "BusStopGameMode.generated.h"

/**
 * 버스 정류장 컷씬 맵용 게임 모드.
 *
 * 폰·컨트롤러·HUD는 AGridTestGameMode와 같다. 다른 점은 그리드를 요구하지 않는다는 것 하나다.
 * 컷씬 맵에는 AGridActor가 없으므로, 부모의 BeginPlay가 매번 찍는 "그리드 없음" Error를 건너뛴다.
 *
 * 맵의 World Settings → GameMode Override에 지정한다.
 */
UCLASS()
class LETSTAKETHESUBWAY_API ABusStopGameMode : public AGridTestGameMode
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
};
