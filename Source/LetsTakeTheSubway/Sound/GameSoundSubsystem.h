// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundLibrary.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameSoundSubsystem.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USoundClass;
class USoundMix;

/** 사운드 시스템 로그. 키가 없거나 애셋이 비었을 때 여기에 남긴다. */
DECLARE_LOG_CATEGORY_EXTERN(LogLTTSSound, Log, All);

/**
 * 게임의 모든 효과음 재생과 마스터 볼륨을 맡는다.
 *
 * 게임 인스턴스 서브시스템인 이유는 볼륨이 레벨을 넘어 살아남아야 하기 때문이다. 스테이지 1 ->
 * 버스 정류장 컷씬 -> 스테이지 2로 넘어가도 슬라이더로 맞춘 값이 그대로여야 한다.
 *
 * 호출하는 쪽은 사운드 키(LTTSSoundKeys)만 넘긴다. 키 -> 애셋은 DA_SoundLibrary가 정하고, 애셋이
 * 비어 있거나 키가 없으면 아무 소리도 내지 않고 nullptr을 돌려준다. 그래서 파일이 아직 없는 소리도
 * 호출부를 미리 넣어 둘 수 있다.
 *
 * 마스터 볼륨은 SMix_Master 위에서 SC_Master(와 그 자식 전부)의 볼륨을 덮어써서 건다. 개별 재생의
 * 볼륨에는 곱하지 않는다 -- 두 번 곱하면 슬라이더가 제곱으로 들린다.
 *
 * 설계와 발동 지점 목록은 Docs/Plans/SoundSystem.md.
 */
UCLASS()
class LETSTAKETHESUBWAY_API UGameSoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 이 오브젝트가 속한 게임 인스턴스의 사운드 서브시스템. 게임 월드 밖(에디터 월드)에서는 null. */
	static UGameSoundSubsystem* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------------------------------------------------------------- 재생

	/** 위치 없이 재생한다. UI, 알림, 열차 안에서 듣는 소리. */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	UAudioComponent* PlaySound2D(FName Key);

	/** 월드 위치에서 한 번 재생한다. */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	UAudioComponent* PlaySoundAtLocation(FName Key, FVector Location);

	/**
	 * 컴포넌트에 붙여 재생한다. 열차·엘리베이터·블록처럼 소리가 나는 동안 움직이는 것.
	 *
	 * 루프 여부는 사운드 애셋(SoundWave의 Looping)이 정한다. 루프를 끝내려면 돌려받은 컴포넌트를
	 * StopSound에 넘긴다. 붙인 컴포넌트가 파괴되면 함께 멈춘다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	UAudioComponent* PlaySoundAttached(FName Key, USceneComponent* AttachTo);

	/** 재생 중인 소리를 페이드 아웃하며 멈춘다. null이어도 된다. */
	UFUNCTION(BlueprintCallable, Category = "Sound")
	static void StopSound(UAudioComponent* AudioComponent, float FadeOutSeconds = 0.2f);

	/** 키에 재생할 애셋이 연결돼 있는지. */
	UFUNCTION(BlueprintPure, Category = "Sound")
	bool HasSound(FName Key) const;

	// ---------------------------------------------------------------- 마스터 볼륨

	/** 0~1. 곧바로 들린다. 디스크에는 쓰지 않는다(SaveSettings). */
	UFUNCTION(BlueprintCallable, Category = "Sound|Volume")
	void SetMasterVolume(float Volume01);

	UFUNCTION(BlueprintPure, Category = "Sound|Volume")
	float GetMasterVolume() const { return MasterVolume; }

	/** 지금 마스터 볼륨을 GameUserSettings.ini에 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "Sound|Volume")
	void SaveSettings();

	/** 콘솔 ltts.Sound List. 라이브러리의 키와 연결 상태를 로그로 찍는다. */
	void LogLibrary() const;

private:
	/** 키를 풀어 재생할 사운드와 항목을 얻는다. 재연결 간격이나 재생 중 판정에 걸리면 false. */
	bool Resolve(FName Key, const FSoundLibraryEntry*& OutEntry, USoundBase*& OutSound);

	/** 방금 스폰한 컴포넌트를 키에 기록한다. 다음 호출의 "아직 재생 중인가" 판정 근거다. */
	UAudioComponent* Track(FName Key, UAudioComponent* Component);

	/** 항목의 감쇠, 없으면 기본 감쇠. */
	USoundAttenuation* ResolveAttenuation(const FSoundLibraryEntry& Entry) const;

	/** 현재 월드의 오디오 장치에 베이스 믹스와 마스터 볼륨 오버라이드를 건다. */
	void ApplyMasterVolumeToWorld(UWorld* World) const;

	/** 레벨이 바뀔 때마다 믹스를 다시 건다. PIE는 세션마다 월드가 새로 생긴다. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	UWorld* GetPlayWorld() const;

	UPROPERTY(Transient)
	TObjectPtr<USoundLibrary> Library;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> MasterMix;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MasterClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> DefaultAttenuation;

	/** 로드해 둔 사운드. 키마다 한 번만 로드하고 GC에 붙들어 둔다. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> LoadedSounds;

	/** 키마다 마지막으로 재생한 시각(실시간 초). MinRetriggerSeconds 판정용. */
	TMap<FName, double> LastPlayTime;

	/**
	 * 키마다 마지막으로 스폰한 컴포넌트. bSkipWhilePlaying 판정용.
	 *
	 * 약참조인 이유는 스폰된 컴포넌트가 끝나면 스스로 파괴되기 때문이다(bAutoDestroy).
	 * 무효해진 참조는 "재생 중이 아님"과 같은 뜻이므로 따로 지울 필요가 없다.
	 */
	TMap<FName, TWeakObjectPtr<UAudioComponent>> ActiveSounds;

	/** 이미 경고한 키. 매 호출마다 같은 줄이 쌓이지 않게 한다. */
	mutable TSet<FName> ReportedKeys;

	FDelegateHandle PostLoadMapHandle;

	float MasterVolume = 1.0f;
};
