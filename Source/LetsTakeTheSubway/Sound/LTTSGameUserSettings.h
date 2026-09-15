// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "LTTSGameUserSettings.generated.h"

/**
 * 플레이어별 설정. 엔진의 GameUserSettings에 마스터 볼륨을 더한다.
 *
 * 설정 패널 위젯이 이미 GetGameUserSettings / ApplySettings를 쓰고 있어서 저장소를 새로 만들지 않고
 * 여기에 얹는다. 엔진이 Saved/Config/<플랫폼>/GameUserSettings.ini에 읽고 쓴다.
 *
 * 쓰이려면 DefaultEngine.ini의 [/Script/Engine.Engine] GameUserSettingsClassName이 이 클래스를
 * 가리켜야 한다.
 */
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class LETSTAKETHESUBWAY_API ULTTSGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	ULTTSGameUserSettings();

	/** 엔진의 설정 오브젝트를 이 클래스로. ini가 다른 클래스를 가리키면 null. */
	static ULTTSGameUserSettings* Get();

	UFUNCTION(BlueprintPure, Category = "Settings|Sound")
	float GetMasterVolume() const { return MasterVolume; }

	/** 값만 바꾼다. 디스크에는 SaveSettings가 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "Settings|Sound")
	void SetMasterVolume(float InVolume);

	/** 저장된 값이 없으면 USoundSettings::DefaultMasterVolume을 쓴다. */
	bool HasSavedMasterVolume() const { return MasterVolume >= 0.0f; }

protected:
	/**
	 * 0~1. 슬라이더 값 그대로다.
	 *
	 * 음수는 "저장된 적 없음"이다. 기본값을 CDO에 1로 박으면 USoundSettings::DefaultMasterVolume을
	 * 바꿔도 첫 실행에 반영되지 않는다.
	 */
	UPROPERTY(Config)
	float MasterVolume = -1.0f;
};
