// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "UIManagerSubsystem.generated.h"

UENUM()
enum class EGuideType
{

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIClose);


/**
 *
 */
UCLASS()
class LETSTAKETHESUBWAY_API UUIManagerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()


#pragma region Variable
private:
    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> PathUIWidget;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> SideMenuUIWidget;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> GuidePopUpUIWidget;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> AlertUIWidget;

#pragma endregion


#pragma region Lifecycle
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

#pragma endregion


#pragma region GameInterface
public:
    UPROPERTY()
    FOnUIOpen OnUIOpened;

    UPROPERTY()
    FOnUIClose OnUIClosed;


    UFUNCTION()
    void ShowOrRefreshPathUI();

    UFUNCTION()
    void ShowSideMenuUI();

    UFUNCTION()
    void ShowGuidePopUpUI(EGuideType guide);

    UFUNCTION()
    void ShowAlertUI(FText message);

#pragma endregion


#pragma region UIInterface
public:
    UFUNCTION(BlueprintCallable, Category = "UI")
    void CallUIOpened();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void CallUIClosed();

#pragma endregion


#pragma region Helper
private:
    UUserWidget* GetOrCreateWidget(TObjectPtr<UUserWidget>& Cached, const TSoftClassPtr<UUserWidget>& ClassPtr);

    void HandleMapLoaded(UWorld* NewWorld);

    void ClearAllCachedWidgets();

#pragma endregion

};
