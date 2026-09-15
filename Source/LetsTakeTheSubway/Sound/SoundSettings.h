// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SoundSettings.generated.h"

class USoundAttenuation;
class USoundClass;
class USoundLibrary;
class USoundMix;

/**
 * 사운드 시스템이 읽는 애셋 경로. 프로젝트 설정 > Game > Sound System.
 *
 * UUISettings와 같은 틀이다. 값은 Config/DefaultGame.ini에 저장된다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Sound System"))
class LETSTAKETHESUBWAY_API USoundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 키 -> 애셋 매핑. */
	UPROPERTY(Config, EditAnywhere, Category = "Data")
	TSoftObjectPtr<USoundLibrary> Library;

	/** 마스터 볼륨 오버라이드를 거는 믹스. 항상 켜 둔다(베이스 믹스). */
	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TSoftObjectPtr<USoundMix> MasterMix;

	/** 모든 사운드 클래스의 루트. 마스터 볼륨은 이 클래스와 그 자식 전부에 걸린다. */
	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TSoftObjectPtr<USoundClass> MasterClass;

	/** 라이브러리 항목에 감쇠가 없을 때 위치 있는 재생에 쓰는 감쇠. */
	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TSoftObjectPtr<USoundAttenuation> DefaultAttenuation;

	/** 저장된 값이 없을 때(첫 실행)의 마스터 볼륨. */
	UPROPERTY(Config, EditAnywhere, Category = "Mix", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DefaultMasterVolume = 1.0f;
};
