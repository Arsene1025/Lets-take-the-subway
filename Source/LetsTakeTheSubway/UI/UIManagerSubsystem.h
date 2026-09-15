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
    /**
     * 설정 패널 슬라이더에 넣을 현재 마스터 볼륨(0~1). OpenSettingPanel에서 슬라이더 값을 맞출 때 쓴다.
     *
     * 실제 음향은 UGameSoundSubsystem이 맡고, 이 매니저는 위젯이 오디오 API를 직접 만지지 않도록 값을
     * 넘겨 주는 창구다.
     */
    UFUNCTION(BlueprintPure, Category = "UI|Settings")
    float GetMasterVolume() const;

    /**
     * 슬라이더 값을 곧바로 음향에 적용한다. 슬라이더 OnValueChanged에 연결한다.
     *
     * 드래그하는 동안 매 프레임 불려도 된다. 디스크에는 쓰지 않는다(CommitSettings).
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Settings")
    void ApplyMasterVolume(float Volume01);

    /** 지금 설정값을 저장한다. 설정 패널을 닫거나 적용할 때(CloseSettingPanel / ApplySettings) 부른다. */
    UFUNCTION(BlueprintCallable, Category = "UI|Settings")
    void CommitSettings();

#pragma endregion


#pragma region Helper
private:
    UUserWidget* GetOrCreateWidget(TObjectPtr<UUserWidget>& Cached, const TSoftClassPtr<UUserWidget>& ClassPtr);

    void HandleMapLoaded(UWorld* NewWorld);

    void ClearAllCachedWidgets();

#pragma endregion

};
