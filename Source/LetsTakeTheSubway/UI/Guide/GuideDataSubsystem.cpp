// Fill out your copyright notice in the Description page of Project Settings.


#include "GuideDataSubsystem.h"
#include "UI/UISettings.h"


void UGuideDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const UUISettings* Settings = GetDefault<UUISettings>();
    Database = Settings->GuideDatabase.LoadSynchronous();

    if (!Database)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Guide] 데이터베이스가 지정되지 않았습니다."));
    }
}

bool UGuideDataSubsystem::GetEntry(EGuideType Type, FGuideEntry& OutEntry) const
{
    if (!Database) return false;

    if (const FGuideEntry* Found = Database->Entries.Find(Type))
    {
        OutEntry = *Found;
        return true;
    }
    return false;
}
