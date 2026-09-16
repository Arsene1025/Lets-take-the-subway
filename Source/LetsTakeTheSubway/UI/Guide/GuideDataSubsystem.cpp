// Fill out your copyright notice in the Description page of Project Settings.


#include "GuideDataSubsystem.h"
#include "UI/UISettings.h"
#include "UI/UIManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"


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

    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UGuideDataSubsystem::HandleMapLoaded);
}

void UGuideDataSubsystem::Deinitialize()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

    Super::Deinitialize();
}

UGuideDataSubsystem* UGuideDataSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<UGuideDataSubsystem>() : nullptr;
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

bool UGuideDataSubsystem::RequestGuide(EGuideType Type, bool bForce)
{
    if (Type == EGuideType::None || Type == EGuideType::MAXVALUE)
        return false;

    if (!bForce && ShownGuides.Contains(Type))
        return false;

    const ULocalPlayer* LP = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr;
    UUIManagerSubsystem* UI = LP ? LP->GetSubsystem<UUIManagerSubsystem>() : nullptr;
    if (!UI)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Guide] No UI manager; guide %s was not shown."), *UEnum::GetValueAsString(Type));
        return false;
    }

    ShownGuides.Add(Type);
    UI->ShowGuidePopUpUI(Type);

    UE_LOG(LogTemp, Display, TEXT("[Guide] Showing %s."), *UEnum::GetValueAsString(Type));
    return true;
}

void UGuideDataSubsystem::ResetShownGuides()
{
    ShownGuides.Reset();
    ArmedMoveGuide = EGuideType::None;
}

void UGuideDataSubsystem::ArmGuideOnNextMoveInput(EGuideType Type)
{
    //이미 본 가이드는 걸어 두지 않는다. 걸어 두면 입력 때마다 조용히 무시될 뿐이지만 로그가 헷갈린다.
    if (ShownGuides.Contains(Type))
        return;

    ArmedMoveGuide = Type;
}

void UGuideDataSubsystem::NotifyMoveInput()
{
    if (ArmedMoveGuide == EGuideType::None)
        return;

    const EGuideType Type = ArmedMoveGuide;
    ArmedMoveGuide = EGuideType::None;
    RequestGuide(Type);
}

void UGuideDataSubsystem::HandleMapLoaded(UWorld* NewWorld)
{
    //다른 레벨에서 걸어 둔 "다음 이동 때" 가이드가 엉뚱한 레벨에서 뜨지 않게 한다.
    ArmedMoveGuide = EGuideType::None;
}
