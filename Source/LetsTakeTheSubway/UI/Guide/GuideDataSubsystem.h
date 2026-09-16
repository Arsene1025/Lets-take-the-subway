// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GuideDatabase.h"
#include "GuideDataSubsystem.generated.h"

/**
 * 가이드 데이터(DA_GuideDatabase)를 들고, 가이드 팝업 요청의 입구 역할을 한다.
 *
 * [2026/09/16] 트리거(스테이지 진입, 엘리베이터 도착, 열차 하차 뒤 이동, 셀 트리거)는 전부
 * RequestGuide 하나로 들어온다. 같은 종류는 게임 인스턴스가 살아 있는 동안 한 번만 띄운다.
 * 다시 보려면 사이드 메뉴의 가이드 패널을 쓴다. 트리거 표는 Docs/Plans/UIConnection.md 2절.
 */
UCLASS()
class LETSTAKETHESUBWAY_API UGuideDataSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 월드 문맥에서 게임 인스턴스의 가이드 서브시스템을 찾는다. */
    static UGuideDataSubsystem* Get(const UObject* WorldContext);

    UFUNCTION(BlueprintPure, Category = "Guide")
    bool GetEntry(EGuideType Type, FGuideEntry& OutEntry) const;

    /**
     * 가이드 팝업을 요청한다. None·MAXVALUE는 무시한다.
     *
     * @param bForce 이미 보여 준 종류여도 다시 띄운다(콘솔 확인용).
     * @return 팝업을 띄웠으면 true.
     */
    UFUNCTION(BlueprintCallable, Category = "Guide")
    bool RequestGuide(EGuideType Type, bool bForce = false);

    /** 이미 보여 준 종류인지. */
    UFUNCTION(BlueprintPure, Category = "Guide")
    bool HasShownGuide(EGuideType Type) const { return ShownGuides.Contains(Type); }

    /** 보여 준 기록을 지운다. 새 게임을 시작할 때 부른다. */
    UFUNCTION(BlueprintCallable, Category = "Guide")
    void ResetShownGuides();

    /**
     * 다음 이동 입력 때 띄울 가이드를 걸어 둔다. 이미 걸린 것이 있으면 덮어쓴다.
     *
     * 열차가 플레이어를 내려 준 뒤 부른다. 열차가 시키는 자동 걸음은 입력이 아니므로 세지 않는다.
     */
    void ArmGuideOnNextMoveInput(EGuideType Type);

    /** 플레이어가 이동 입력(이동키, 바닥 클릭)을 했다. 걸린 가이드가 있으면 요청하고 푼다. */
    void NotifyMoveInput();

private:
    void HandleMapLoaded(UWorld* NewWorld);

    UPROPERTY()
    TObjectPtr<UGuideDatabase> Database;

    /** 이미 띄운 가이드. */
    TSet<EGuideType> ShownGuides;

    /** 다음 이동 입력 때 띄울 가이드. None이면 없음. 레벨이 바뀌면 푼다. */
    EGuideType ArmedMoveGuide = EGuideType::None;

    //TArray<TSharedPtr<FStreamableHandle>> ActiveHandles;
};
