// Fill out your copyright notice in the Description page of Project Settings.


#include "UIManagerSubsystem.h"
#include "UISettings.h"
#include "UIInitializable.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"


#pragma region Helper

UUserWidget* UUIManagerSubsystem::GetOrCreateWidget(TObjectPtr<UUserWidget>& Cached, const TSoftClassPtr<UUserWidget>& ClassPtr)
{
    if (IsValid(Cached))
        return Cached;

    ULocalPlayer* LP = GetLocalPlayer();
    if (!LP)
        return nullptr;

    UWorld* World = LP->GetWorld();
    if (!World)
        return nullptr;

    APlayerController* PC = LP->GetPlayerController(World);
    if (!PC)
        return nullptr;


    UClass* WidgetClass = ClassPtr.LoadSynchronous();
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[UIManager] 위젯 클래스가 지정되지 않았습니다. "));
        return nullptr;
    }

    UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
    if (!Widget)
        return nullptr;

    Widget->AddToViewport();
    Widget->SetVisibility(ESlateVisibility::Collapsed);

    Cached = Widget;
    return Widget;
}

void UUIManagerSubsystem::HandleMapLoaded(UWorld* NewWorld)
{
    ClearAllCachedWidgets();
}

void UUIManagerSubsystem::ClearAllCachedWidgets()
{
    if (IsValid(PathUIWidget))
        PathUIWidget->RemoveFromParent();       PathUIWidget = nullptr;

    if (IsValid(SideMenuUIWidget))
        SideMenuUIWidget->RemoveFromParent();   SideMenuUIWidget = nullptr;

    if (IsValid(GuidePopUpUIWidget))
        GuidePopUpUIWidget->RemoveFromParent(); GuidePopUpUIWidget = nullptr;

    if (IsValid(AlertUIWidget))
        AlertUIWidget->RemoveFromParent();      AlertUIWidget = nullptr;
}

#pragma endregion



#pragma region Lifecycle

void UUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UUIManagerSubsystem::HandleMapLoaded);
}

void UUIManagerSubsystem::Deinitialize()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    OnUIOpened.RemoveAll(this);
    ClearAllCachedWidgets();

    Super::Deinitialize();
}

#pragma endregion



#pragma region GameInterface

void UUIManagerSubsystem::ShowOrRefreshPathUI()
{
    const UUISettings* Settings = GetDefault<UUISettings>();

    if (UUserWidget* W = GetOrCreateWidget(PathUIWidget, Settings->PathUIWidget))
    {
        W->SetVisibility(ESlateVisibility::Visible);

        if (W->GetClass()->ImplementsInterface(UUIInitializable::StaticClass()))
        {
            IUIInitializable::Execute_Initialize(W);
        }
    }
}

void UUIManagerSubsystem::ShowSideMenuUI()
{
    const UUISettings* Settings = GetDefault<UUISettings>();

    if (UUserWidget* W = GetOrCreateWidget(SideMenuUIWidget, Settings->SideMenuUIWidget))
    {
        W->SetVisibility(ESlateVisibility::Visible);

        if (W->GetClass()->ImplementsInterface(UUIInitializable::StaticClass()))
        {
            IUIInitializable::Execute_Initialize(W);
        }
    }
}

void UUIManagerSubsystem::ShowGuidePopUpUI(EGuideType guide)
{
    const UUISettings* Settings = GetDefault<UUISettings>();

    if (UUserWidget* W = GetOrCreateWidget(GuidePopUpUIWidget, Settings->GuidePopUpUIWidget))
        W->SetVisibility(ESlateVisibility::Visible);
}

void UUIManagerSubsystem::ShowAlertUI(FText message)
{
    const UUISettings* Settings = GetDefault<UUISettings>();

    if (UUserWidget* W = GetOrCreateWidget(AlertUIWidget, Settings->AlertUIWidget))
        W->SetVisibility(ESlateVisibility::Visible);
}

#pragma endregion



#pragma region UIInterface

void UUIManagerSubsystem::CallUIOpened()
{
    OnUIOpened.Broadcast();
}

void UUIManagerSubsystem::CallUIClosed()
{
    OnUIClosed.Broadcast();
}

#pragma endregion
