// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sound/LTTSGameUserSettings.h"

#include "Engine/Engine.h"

ULTTSGameUserSettings::ULTTSGameUserSettings()
{
}

ULTTSGameUserSettings* ULTTSGameUserSettings::Get()
{
	return GEngine ? Cast<ULTTSGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void ULTTSGameUserSettings::SetMasterVolume(float InVolume)
{
	MasterVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
}
