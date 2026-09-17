// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Guide/GuideType.h"
#include "CutScene/CutSceneDatabase.h"
#include "Stage/StageTypes.h"
#include "UIManagerSubsystem.generated.h"

class IInputProcessor;
class UStageSubsystem;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIClose);

/** 컷씬이 끝났다(스킵 포함). 다음 레벨을 열기 직전에 한 번 온다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCutsceneEnded, ECutsceneKind, Kind);


/**
 * 화면 UI(PathUI, 사이드 메뉴, 가이드 팝업, 알림)를 만들고 띄우는 로컬 플레이어 서브시스템.
 *
 * [2026/09/16] PathUI 자동 갱신: 스테이지 서브시스템(UStageSubsystem)에 바인딩해 구역이 바뀌면
 * AStageInfo::ZonePathUIIndices에서 그림 번호를 읽어 ShowOrRefreshPathUI를 부른다. 이 서브시스템은
 * 레벨을 넘어 살아남고 스테이지 서브시스템은 레벨마다 새로 생기므로, 맵이 열리거나 플레이어
 * 컨트롤러가 바뀔 때마다 풀고 다시 바인딩한다(Docs/StageZone.html 4절, Docs/Plans/UIConnection.md).
 *
 * [2026/09/17] ESC 토글: 슬레이트 입력 전처리기(EscapeProcessor)가 ESC를 받아 ToggleSideMenuUI를 부른다.
 * 플레이어 컨트롤러의 입력 액션으로는 닫을 수 없다 -- 사이드 메뉴가 열리면 입력 모드가 UI 전용이 되어
 * 게임 뷰포트가 키를 받지 않기 때문이다(HandleEscapeKey, UIConnection.md 11절).
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

    /** [2026/09/17] 컷씬이 끝나면 열 레벨. 비어 있으면 레벨을 바꾸지 않는다. EndCutscene이 비운다. */
    TSoftObjectPtr<UWorld> PendingLevelAfterCutscene;

    /** 지금 재생 중인(또는 마지막으로 재생한) 컷씬. OnCutsceneEnded에 실어 보낸다. */
    ECutsceneKind ActiveCutscene = ECutsceneKind::Intro;

    /** 바인딩한 스테이지 서브시스템. 레벨과 함께 사라지므로 약참조로 든다.   */
    TWeakObjectPtr<UStageSubsystem> BoundStage;

    /** 마지막으로 PathUI에 넘긴 그림 번호. 같은 번호를 다시 그리지 않는다. -1이면 아직 없음. */
    int32 LastPathIndex = INDEX_NONE;

    /** 이 레벨의 진입 가이드(AStageInfo::EntryGuides)를 이미 요청했는지. */
    bool bStageEntryHandled = false;

    /** CallUIOpened만큼 늘고 CallUIClosed만큼 준다. 0보다 크면 게임 입력을 막고 UI만 클릭된다. */
    int32 OpenUICount = 0;

    /** ESC를 받아 사이드 메뉴를 여닫는 슬레이트 입력 전처리기. Initialize에서 등록하고 Deinitialize에서 뺀다. */
    TSharedPtr<IInputProcessor> EscapeProcessor;

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

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowSideMenuUI();

    /**
     * [2026/09/17] 사이드 메뉴를 접는다. WBP_SideMenuUI의 닫기 버튼(CloseSideMenuUI → CallUIClosed)과 같은 순서로
     * 위젯을 Collapsed로 두고 CallUIClosed를 부른다. 위젯은 없애지 않으므로 다음 ShowSideMenuUI가 그대로 다시 쓴다.
     */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void HideSideMenuUI();

    /** 사이드 메뉴가 열려 있으면 접고, 접혀 있으면 연다. ESC가 이것을 부른다. */
    UFUNCTION(BlueprintCallable, Category = "UI")
    void ToggleSideMenuUI();

    /** 사이드 메뉴 위젯이 만들어져 있고 보이는 상태인지. */
    UFUNCTION(BlueprintPure, Category = "UI")
    bool IsSideMenuOpen() const;

    UFUNCTION(BlueprintCallable, Category = "UI")
    void ShowGuidePopUpUI(EGuideType guide);

    UFUNCTION()
    void ShowAlertUI(FText message);

    //[2026/09/17] 정수 번호로 위젯의 InitializeInt를 부르던 옛 경로. 재생 논리가 UCutSceneWidget으로 옮겨 가며
    //ECutsceneKind 버전으로 바꿨다. 호출하는 곳이 없어 주석으로 남긴다.
    //UFUNCTION()
    //void PlayCutscene(int32 cutscene);

    /**
     * [2026/09/17] 컷씬을 띄우고, 끝나면(스킵 포함) NextLevel을 연다. NextLevel이 비어 있으면 레벨을 바꾸지 않는다.
     *
     * 데이터 애셋은 UUISettings(IntroCutscene / EndingCutscene)에서 찾고, 위젯은 UUISettings::CutsceneWidget이다.
     * 위젯이 UCutSceneWidget이 아니거나(재부모화 전) 애셋이 비어 있으면 경고를 남기고 곧바로 EndCutscene한다.
     * 컷씬이 빠져도 루프는 끊기지 않는다. 이미 재생 중이면 무시한다.
     *
     * Intro는 새 게임의 시작이므로 UGuideDataSubsystem::ResetShownGuides도 부른다(두 번째 판에도 가이드가 뜬다).
     *
     * 타이틀(WBP_MainUI 시작 버튼)과 엔딩 볼륨(ACutsceneCellTrigger)이 부른다. Docs/Plans/GameLoop.md.
     */
    UFUNCTION(BlueprintCallable, Category = "CutScene")
    void PlayCutscene(ECutsceneKind Kind, TSoftObjectPtr<UWorld> NextLevel);

    /** 컷씬 위젯이 떠 있는지. */
    UFUNCTION(BlueprintPure, Category = "CutScene")
    bool IsCutscenePlaying() const;

    UPROPERTY(BlueprintAssignable, Category = "CutScene")
    FOnCutsceneEnded OnCutsceneEnded;

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


    /**
     * 컷씬을 끝낸다. 위젯의 OnFinished(재생 완료·스킵)가 부르고, ESC와 콘솔도 부른다.
     *
     * 위젯을 지우고 OnCutsceneEnded를 보낸 뒤, PlayCutscene에 받아 둔 NextLevel이 있으면 연다.
     * 두 번 불려도 레벨을 두 번 열지 않는다(열기 전에 대기 레벨을 비운다).
     */
    UFUNCTION(BlueprintCallable, Category = "CutScene")
    void EndCutscene();

#pragma endregion


#pragma region Input
public:
    /**
     * [2026/09/17] ESC가 눌렸다(EscapeProcessor가 부른다). 사이드 메뉴를 여닫을 상황이면 토글하고 true를 돌려
     * 키를 소비한다. false면 키는 원래대로 흘러간다(에디터에서는 PIE 정지 등).
     *
     * 게임 월드가 아니거나, 타이틀처럼 AGridPlayerController가 없는 레벨이거나, 컷씬 중이면 받지 않는다.
     * 에디터에서는 PIE 뷰포트(또는 그 위의 위젯)에 포커스가 있을 때만 받아, 다른 패널에서 누른 ESC를
     * 가로채지 않는다.
     */
    bool HandleEscapeKey();

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

    /** [2026/09/17] Kind에 맞는 컷씬 데이터 애셋을 UUISettings에서 동기 로드한다. 없으면 null. */
    static const UCutSceneDatabase* FindCutsceneDatabase(ECutsceneKind Kind);

    /** true면 UI 전용 입력, false면 게임 입력(AGridPlayerController::BeginPlay와 같은 설정)으로 바꾼다. */
    void ApplyUIInputMode(bool bUIOnly);

#pragma endregion

};
