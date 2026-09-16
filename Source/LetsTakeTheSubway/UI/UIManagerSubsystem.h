// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Guide/GuideType.h"
#include "Stage/StageTypes.h"
#include "UIManagerSubsystem.generated.h"

class UStageSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIClose);


/**
 * 화면 UI(PathUI, 사이드 메뉴, 가이드 팝업, 알림)를 만들고 띄우는 로컬 플레이어 서브시스템.
 *
 * [2026/09/16] PathUI 자동 갱신: 스테이지 서브시스템(UStageSubsystem)에 바인딩해 구역이 바뀌면
 * AStageInfo::ZonePathUIIndices에서 그림 번호를 읽어 ShowOrRefreshPathUI를 부른다. 이 서브시스템은
 * 레벨을 넘어 살아남고 스테이지 서브시스템은 레벨마다 새로 생기므로, 맵이 열리거나 플레이어
 * 컨트롤러가 바뀔 때마다 풀고 다시 바인딩한다(Docs/StageZone.html 4절, Docs/Plans/UIConnection.md).
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

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> CutsceneWidget;

    /** 바인딩한 스테이지 서브시스템. 레벨과 함께 사라지므로 약참조로 든다. */
    TWeakObjectPtr<UStageSubsystem> BoundStage;

    /** 마지막으로 PathUI에 넘긴 그림 번호. 같은 번호를 다시 그리지 않는다. -1이면 아직 없음. */
    int32 LastPathIndex = INDEX_NONE;

    /** 이 레벨의 진입 가이드(AStageInfo::EntryGuides)를 이미 요청했는지. */
    bool bStageEntryHandled = false;

    /** CallUIOpened만큼 늘고 CallUIClosed만큼 준다. 0보다 크면 게임 입력을 막고 UI만 클릭된다. */
    int32 OpenUICount = 0;

#pragma endregion


#pragma region Lifecycle
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

#pragma endregion


#pragma region GameInterface
public:
    /** 월드 문맥에서 첫 로컬 플레이어의 UI 매니저를 찾는다. 액터(도크 등)가 UI를 부를 때 쓴다. */
    static UUIManagerSubsystem* Get(const UObject* WorldContext);

    UPROPERTY()
    FOnUIOpen OnUIOpened;

    UPROPERTY()
    FOnUIClose OnUIClosed;


    /**
     * PathUI를 띄우고 그림 번호를 넘긴다. 위젯은 InitializeInt에서 Switch On Int로 그림을 고른다.
     *
     * 번호 규칙(2026-09-16): Stage1 구역 1~4 = 0~3, Stage1 클리어 엘리베이터 탑승 = 4,
     * Stage2 구역 1·3·4·6·8 = 5~9. 음수면 아무것도 하지 않는다.
     */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowOrRefreshPathUI(int32 PathIndex);

    UFUNCTION()
    void ShowSideMenuUI();

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowGuidePopUpUI(EGuideType guide);

    UFUNCTION()
    void ShowAlertUI(FText message);

    UFUNCTION()
    void PlayCutscene(int32 cutscene);

#pragma endregion


#pragma region UIInterface
public:
    /**
     * [2026/09/16] 입력을 막는 UI(사이드 메뉴 등)가 열렸다. 위젯이 열릴 때 부른다.
     *
     * 첫 번째로 열린 UI면 입력 모드를 UI 전용(FInputModeUIOnly)으로 바꿔 월드 클릭·이동키를 막고
     * UI만 클릭되게 한다. OnUIOpened를 받은 AGridPlayerController는 누르고 있던 이동키와 드래그를 정리한다.
     * 가이드 팝업처럼 저절로 뜨고 닫히는 UI는 부르지 않는다.
     */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void CallUIOpened();

    /** 입력을 막는 UI가 닫혔다. 열린 UI가 더 없으면 게임 입력 모드로 되돌린다. */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void CallUIClosed();

    /** 입력을 막는 UI가 하나라도 열려 있는지. */
    UFUNCTION(BlueprintPure, Category = "UI")
    bool IsUIOpen() const { return OpenUICount > 0; }


    UFUNCTION(BlueprintCallable, Category = "CutScene")
    void EndCutscene();

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


#pragma region Stage
private:
    void BindStage(UStageSubsystem* Stage);
    void UnbindStage();

    /** 구역이 바뀌면 PathUI를 갱신하고, 레벨 시작 판정 첫 방송이면 진입 가이드를 요청한다. */
    UFUNCTION()
    void HandleStageStateChanged(const FStageState& State);

    /** AStageInfo::EntryGuides를 요청한다. 레벨 첫 프레임을 피해 다음 틱에 부른다. */
    void RequestEntryGuides();

#pragma endregion


#pragma region Helper
private:
    UUserWidget* GetOrCreateWidget(TObjectPtr<UUserWidget>& Cached, const TSoftClassPtr<UUserWidget>& ClassPtr);

    void HandleMapLoaded(UWorld* NewWorld);

    void ClearAllCachedWidgets();

    /** true면 UI 전용 입력, false면 게임 입력(AGridPlayerController::BeginPlay와 같은 설정)으로 바꾼다. */
    void ApplyUIInputMode(bool bUIOnly);

#pragma endregion

};
