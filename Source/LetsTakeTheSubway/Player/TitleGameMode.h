// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TitleGameMode.generated.h"

class APlayerController;
class UUserWidget;

/**
 * 타이틀 레벨(Subway_Title) 전용 게임 모드.
 *
 * 폰을 만들지 않는다. 시점은 레벨에 놓인 고정 카메라(AutoActivateForPlayer = Player0, 지금은 `CAM_ToonIso`)가
 * 맡고, 게임 모드는 BeginPlay에서 그 카메라를 뷰 타깃으로 고정한 뒤 MainUI(UUISettings::MainUIWidget)를 띄우고
 * 입력을 UI 전용으로 바꾼다.
 *
 * AGridTestGameMode를 쓰지 않는 이유: 그쪽은 AGridPawn을 스폰하고, AGridPlayerController가 구역 카메라가 없는
 * 레벨에서는 폰 카메라를 켜 버려(ShouldFollowPawn) 고정 카메라를 덮어쓴다. 타이틀에는 그리드도 폰도 없다.
 *
 * 배경음악은 게임 모드가 아니라 레벨에 놓인 AmbientSound(`BGM_Title`)가 낸다. 타이틀 전용 곡이라 DA_SoundLibrary에
 * 넣지 않았고, 레벨을 떠나면 액터와 함께 멈춘다.
 */
UCLASS()
class LETSTAKETHESUBWAY_API ATitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATitleGameMode();

	virtual void BeginPlay() override;

private:
	/** 레벨에서 AutoActivateForPlayer = Player0인 카메라를 찾아 뷰 타깃으로 고정한다. 없으면 경고만 남긴다. */
	void LockViewToLevelCamera(APlayerController* PC) const;

	/** UUISettings::MainUIWidget을 만들어 뷰포트에 붙이고, 커서를 보이며 입력을 UI 전용으로 바꾼다. */
	void ShowMainUI(APlayerController* PC);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MainUI;
};
