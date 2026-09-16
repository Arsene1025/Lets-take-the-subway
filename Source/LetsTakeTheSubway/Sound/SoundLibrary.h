// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SoundLibrary.generated.h"

class USoundAttenuation;
class USoundBase;

/** 사운드 키 하나에 붙는 애셋과 재생 값. */
USTRUCT(BlueprintType)
struct LETSTAKETHESUBWAY_API FSoundLibraryEntry
{
	GENERATED_BODY()

	/**
	 * 재생할 사운드. SoundWave든 SoundCue든 MetaSound든 된다.
	 *
	 * 비워 두면 그 키는 "아직 파일이 없는 소리"다. 코드는 조용히 건너뛰므로, 파일이 오기 전에
	 * 키부터 넣어 두면 어떤 소리가 비어 있는지 이 애셋에서 바로 보인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> Sound;

	/** 볼륨 배율. 마스터 볼륨은 사운드 믹스가 따로 곱하므로 여기에 넣지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = 0.0, UIMax = 4.0))
	float Volume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = 0.1, ClampMax = 4.0))
	float Pitch = 1.0f;

	/** 위치가 있는 재생(액터에 붙이거나 월드 위치)에 쓸 감쇠. 비우면 USoundSettings::DefaultAttenuation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundAttenuation> Attenuation;

	/**
	 * 같은 키를 이 시간(초) 안에 다시 부르면 무시한다. 0이면 거르지 않는다.
	 *
	 * 행인 무리가 개찰구를 한꺼번에 지나거나 블록을 빠르게 이어 밀 때 같은 소리가 겹겹이 쌓이지
	 * 않게 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = 0.0))
	float MinRetriggerSeconds = 0.0f;

	/**
	 * 같은 키의 소리가 **아직 들리는 동안** 다시 부르면 건너뛴다.
	 *
	 * 클립보다 짧은 간격으로 발동하는 소리가 겹겹이 쌓이는 것을 막는다: 블록을 이어 밀면
	 * 한 칸에 1/6초인데 드래그 소리는 0.44초라, 이것이 없으면 같은 소리가 두세 겹으로 난다.
	 * MinRetriggerSeconds와 달리 숫자를 손으로 맞출 필요가 없다 -- 애셋이 길어지면 간격도
	 * 따라 길어진다.
	 *
	 * **루프 사운드에는 걸리지 않는다.** 그쪽은 호출자가 컴포넌트를 들고 있다가 StopSound로
	 * 끄는 구조라, 페이드 아웃 중에 건너뛰면 새 루프를 잃고 이전 루프의 제어권도 사라진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	bool bSkipWhilePlaying = true;
};

/**
 * 사운드 키 -> 애셋 매핑. Content/Design/Sound/DA_SoundLibrary.
 *
 * 코드는 키(LTTSSoundKeys)만 알고 애셋을 참조하지 않는다. 아트가 파일을 바꾸거나 새 소리를 더해도
 * 이 애셋만 고치면 되고, 코드가 Art 폴더를 하드 참조하지 않는다(README의 참조 방향 규칙).
 */
UCLASS(BlueprintType)
class LETSTAKETHESUBWAY_API USoundLibrary : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ForceInlineRow))
	TMap<FName, FSoundLibraryEntry> Entries;
};
