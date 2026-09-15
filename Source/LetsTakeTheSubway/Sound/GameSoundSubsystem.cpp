// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sound/GameSoundSubsystem.h"

#include "Sound/LTTSGameUserSettings.h"
#include "Sound/SoundSettings.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogLTTSSound);

namespace
{
	/** 슬라이더를 끄는 동안 믹스가 뒤따라오는 시간(초). 0이면 드래그할 때 틱마다 뚝뚝 끊겨 들린다. */
	constexpr float VolumeFadeSeconds = 0.1f;
}

// ---------------------------------------------------------------------------- 생명주기

UGameSoundSubsystem* UGameSoundSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGameSoundSubsystem>() : nullptr;
}

void UGameSoundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const USoundSettings* Settings = GetDefault<USoundSettings>();

	Library = Settings->Library.LoadSynchronous();
	MasterMix = Settings->MasterMix.LoadSynchronous();
	MasterClass = Settings->MasterClass.LoadSynchronous();
	DefaultAttenuation = Settings->DefaultAttenuation.LoadSynchronous();

	if (!Library)
	{
		UE_LOG(LogLTTSSound, Warning,
			TEXT("Sound library is not set or failed to load (Project Settings > Sound System > Library). No sounds will play."));
	}
	else
	{
		// 효과음은 전부 합쳐 10 MB가 안 된다. 처음 울릴 때 로드하면 첫 클릭·첫 드래그에 끊김이 생기므로
		// 시작할 때 한꺼번에 올려 둔다.
		int32 NumLoaded = 0;
		int32 NumEmpty = 0;
		for (const TPair<FName, FSoundLibraryEntry>& Pair : Library->Entries)
		{
			if (Pair.Value.Sound.IsNull())
			{
				++NumEmpty;
				continue;
			}

			if (USoundBase* Sound = Pair.Value.Sound.LoadSynchronous())
			{
				LoadedSounds.Add(Pair.Key, Sound);
				++NumLoaded;
			}
			else
			{
				UE_LOG(LogLTTSSound, Warning, TEXT("Sound '%s': asset %s failed to load."),
					*Pair.Key.ToString(), *Pair.Value.Sound.ToString());
			}
		}

		UE_LOG(LogLTTSSound, Display, TEXT("Sound library %s: %d sound(s) loaded, %d key(s) still without a sound."),
			*GetNameSafe(Library), NumLoaded, NumEmpty);
	}

	if (!MasterMix || !MasterClass)
	{
		UE_LOG(LogLTTSSound, Warning,
			TEXT("Master mix or master sound class is not set (Project Settings > Sound System). The volume slider will do nothing."));
	}

	// 저장된 볼륨이 없으면(첫 실행) 설정의 기본값.
	MasterVolume = Settings->DefaultMasterVolume;
	if (const ULTTSGameUserSettings* UserSettings = ULTTSGameUserSettings::Get())
	{
		if (UserSettings->HasSavedMasterVolume())
		{
			MasterVolume = UserSettings->GetMasterVolume();
		}
	}
	else
	{
		UE_LOG(LogLTTSSound, Warning,
			TEXT("GameUserSettings is not ULTTSGameUserSettings (DefaultEngine.ini GameUserSettingsClassName). The volume will not be saved."));
	}

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UGameSoundSubsystem::HandlePostLoadMap);

	// 게임 인스턴스는 첫 레벨보다 먼저 만들어지므로 여기서 월드가 없을 수 있다. 그 경우 첫 PostLoadMap이 건다.
	ApplyMasterVolumeToWorld(GetPlayWorld());
}

void UGameSoundSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	LoadedSounds.Reset();
	Super::Deinitialize();
}

UWorld* UGameSoundSubsystem::GetPlayWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

void UGameSoundSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 다른 게임 인스턴스의 월드(멀티 PIE)에는 걸지 않는다.
	if (LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		ApplyMasterVolumeToWorld(LoadedWorld);
	}
}

// ---------------------------------------------------------------------------- 재생

bool UGameSoundSubsystem::Resolve(FName Key, const FSoundLibraryEntry*& OutEntry, USoundBase*& OutSound)
{
	OutEntry = nullptr;
	OutSound = nullptr;

	if (Key.IsNone() || !Library)
	{
		return false;
	}

	const FSoundLibraryEntry* Entry = Library->Entries.Find(Key);
	if (!Entry)
	{
		if (!ReportedKeys.Contains(Key))
		{
			ReportedKeys.Add(Key);
			UE_LOG(LogLTTSSound, Warning, TEXT("Sound key '%s' is not in %s."), *Key.ToString(), *GetNameSafe(Library));
		}
		return false;
	}

	const TObjectPtr<USoundBase>* Loaded = LoadedSounds.Find(Key);
	if (!Loaded || !*Loaded)
	{
		// 비어 있는 키는 정상이다(파일이 아직 없는 소리). 한 번만 알린다.
		if (!ReportedKeys.Contains(Key))
		{
			ReportedKeys.Add(Key);
			UE_LOG(LogLTTSSound, Verbose, TEXT("Sound key '%s' has no sound yet; skipping."), *Key.ToString());
		}
		return false;
	}

	if (Entry->MinRetriggerSeconds > 0.0f)
	{
		const double Now = FPlatformTime::Seconds();
		if (const double* Last = LastPlayTime.Find(Key))
		{
			if (Now - *Last < Entry->MinRetriggerSeconds)
			{
				return false;
			}
		}
		LastPlayTime.Add(Key, Now);
	}

	OutEntry = Entry;
	OutSound = *Loaded;
	return true;
}

USoundAttenuation* UGameSoundSubsystem::ResolveAttenuation(const FSoundLibraryEntry& Entry) const
{
	if (!Entry.Attenuation.IsNull())
	{
		if (USoundAttenuation* Attenuation = Entry.Attenuation.LoadSynchronous())
		{
			return Attenuation;
		}
	}
	return DefaultAttenuation;
}

UAudioComponent* UGameSoundSubsystem::PlaySound2D(FName Key)
{
	const FSoundLibraryEntry* Entry = nullptr;
	USoundBase* Sound = nullptr;
	UWorld* World = GetPlayWorld();

	if (!World || !Resolve(Key, Entry, Sound))
	{
		return nullptr;
	}

	return UGameplayStatics::SpawnSound2D(World, Sound, Entry->Volume, Entry->Pitch);
}

UAudioComponent* UGameSoundSubsystem::PlaySoundAtLocation(FName Key, FVector Location)
{
	const FSoundLibraryEntry* Entry = nullptr;
	USoundBase* Sound = nullptr;
	UWorld* World = GetPlayWorld();

	if (!World || !Resolve(Key, Entry, Sound))
	{
		return nullptr;
	}

	return UGameplayStatics::SpawnSoundAtLocation(World, Sound, Location, FRotator::ZeroRotator,
		Entry->Volume, Entry->Pitch, 0.0f, ResolveAttenuation(*Entry));
}

UAudioComponent* UGameSoundSubsystem::PlaySoundAttached(FName Key, USceneComponent* AttachTo)
{
	if (!AttachTo)
	{
		return nullptr;
	}

	const FSoundLibraryEntry* Entry = nullptr;
	USoundBase* Sound = nullptr;

	if (!Resolve(Key, Entry, Sound))
	{
		return nullptr;
	}

	return UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector,
		EAttachLocation::KeepRelativeOffset, /*bStopWhenAttachedToDestroyed*/ true,
		Entry->Volume, Entry->Pitch, 0.0f, ResolveAttenuation(*Entry));
}

void UGameSoundSubsystem::StopSound(UAudioComponent* AudioComponent, float FadeOutSeconds)
{
	if (!IsValid(AudioComponent) || !AudioComponent->IsPlaying())
	{
		return;
	}

	if (FadeOutSeconds > 0.0f)
	{
		AudioComponent->FadeOut(FadeOutSeconds, 0.0f);
	}
	else
	{
		AudioComponent->Stop();
	}
}

bool UGameSoundSubsystem::HasSound(FName Key) const
{
	const TObjectPtr<USoundBase>* Loaded = LoadedSounds.Find(Key);
	return Loaded && *Loaded;
}

// ---------------------------------------------------------------------------- 볼륨

void UGameSoundSubsystem::SetMasterVolume(float Volume01)
{
	MasterVolume = FMath::Clamp(Volume01, 0.0f, 1.0f);
	ApplyMasterVolumeToWorld(GetPlayWorld());
}

void UGameSoundSubsystem::ApplyMasterVolumeToWorld(UWorld* World) const
{
	if (!World || !MasterMix || !MasterClass)
	{
		return;
	}

	// 베이스 믹스는 오디오 장치당 하나라 몇 번을 걸어도 쌓이지 않는다. Push와 달리 짝을 맞출 Pop이 필요 없다.
	UGameplayStatics::SetBaseSoundMix(World, MasterMix);
	UGameplayStatics::SetSoundMixClassOverride(World, MasterMix, MasterClass,
		MasterVolume, 1.0f, VolumeFadeSeconds, /*bApplyToChildren*/ true);
}

void UGameSoundSubsystem::SaveSettings()
{
	ULTTSGameUserSettings* UserSettings = ULTTSGameUserSettings::Get();
	if (!UserSettings)
	{
		return;
	}

	UserSettings->SetMasterVolume(MasterVolume);
	UserSettings->SaveSettings();

	UE_LOG(LogLTTSSound, Display, TEXT("Master volume %.2f saved."), MasterVolume);
}

void UGameSoundSubsystem::LogLibrary() const
{
	if (!Library)
	{
		UE_LOG(LogLTTSSound, Display, TEXT("No sound library."));
		return;
	}

	UE_LOG(LogLTTSSound, Display, TEXT("%s: %d key(s), master volume %.2f."),
		*GetNameSafe(Library), Library->Entries.Num(), MasterVolume);

	TArray<FName> Keys;
	Library->Entries.GetKeys(Keys);
	Keys.Sort(FNameLexicalLess());

	for (const FName& Key : Keys)
	{
		const FSoundLibraryEntry& Entry = Library->Entries[Key];
		UE_LOG(LogLTTSSound, Display, TEXT("  %-28s %s"),
			*Key.ToString(),
			HasSound(Key) ? *Entry.Sound.GetAssetName() : TEXT("(no sound yet)"));
	}
}

// ---------------------------------------------------------------------------- 콘솔

namespace
{
	void SoundCommand(const TArray<FString>& Args, UWorld* World)
	{
		UGameSoundSubsystem* Sound = UGameSoundSubsystem::Get(World);
		if (!World || !World->IsGameWorld() || !Sound)
		{
			UE_LOG(LogLTTSSound, Warning, TEXT("ltts.Sound: run this in play mode."));
			return;
		}

		const FString Verb = Args.IsValidIndex(0) ? Args[0] : FString(TEXT("List"));

		if (Verb.Equals(TEXT("Play"), ESearchCase::IgnoreCase) && Args.IsValidIndex(1))
		{
			const bool bPlayed = Sound->PlaySound2D(FName(*Args[1])) != nullptr;
			UE_LOG(LogLTTSSound, Display, TEXT("ltts.Sound Play %s: %s."), *Args[1], bPlayed ? TEXT("playing") : TEXT("nothing to play"));
		}
		else if (Verb.Equals(TEXT("Volume"), ESearchCase::IgnoreCase) && Args.IsValidIndex(1))
		{
			Sound->SetMasterVolume(FCString::Atof(*Args[1]));
			UE_LOG(LogLTTSSound, Display, TEXT("ltts.Sound Volume: master volume now %.2f (not saved)."), Sound->GetMasterVolume());
		}
		else if (Verb.Equals(TEXT("Save"), ESearchCase::IgnoreCase))
		{
			Sound->SaveSettings();
		}
		else
		{
			Sound->LogLibrary();
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSoundCommand(
	TEXT("ltts.Sound"),
	TEXT("Sound system: ltts.Sound List | Play <Key> | Volume <0..1> | Save"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SoundCommand));
