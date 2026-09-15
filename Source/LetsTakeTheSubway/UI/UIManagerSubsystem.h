// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Guide/GuideType.h"
#include "UIManagerSubsystem.generated.h"

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


#pragma region Settings
public:

    //[2026/09/15] 21시 08분 고성현 수정
    //볼륨 슬라이더에 넣을 마스터 볼륨(0~1) 얻어오는 함수
    UFUNCTION(BlueprintPure, Category = "UI|Settings")
    float GetMasterVolume() const;

    //슬라이더 값으로 실제 음향 적용하는거, OnValueChanged에 연결하면 됨.
    UFUNCTION(BlueprintCallable, Category = "UI|Settings")
    void ApplyMasterVolume(float Volume01);

    //현재 설정값 저장하는 함수.
    UFUNCTION(BlueprintCallable, Category = "UI|Settings")
    void CommitSettings();
    //수정 끝
#pragma endregion


#pragma region Helper
private:
    UUserWidget* GetOrCreateWidget(TObjectPtr<UUserWidget>& Cached, const TSoftClassPtr<UUserWidget>& ClassPtr);

    void HandleMapLoaded(UWorld* NewWorld);

    void ClearAllCachedWidgets();

#pragma endregion

};
