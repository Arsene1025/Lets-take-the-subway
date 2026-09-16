// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cutscene/BusStopGameMode.h"

void ABusStopGameMode::BeginPlay()
{
	// 부모(AGridTestGameMode)는 그리드를 찾아 스테이지 클리어에 바인딩한다. 컷씬 맵에는 그리드도 클리어도 없다.
	// 그림자 캐시 끄기는 부모의 PostInitializeComponents에 있으므로 여기서 건너뛰어도 적용된다.
	AGameModeBase::BeginPlay();
}
