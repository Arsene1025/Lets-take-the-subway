// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Blueprint/UserWidget.h"
#include "Guide/GuideDatabase.h"
#include "CutScene/CutSceneDatabase.h"
#include "UISettings.generated.h"

/**
 *
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "UI Manager"))
class LETSTAKETHESUBWAY_API UUISettings : public UDeveloperSettings
{
    GENERATED_BODY()


public:
    /** 타이틀 레벨(ATitleGameMode)이 BeginPlay에서 띄우는 메인 메뉴. */
    UPROPERTY(Config, EditAnywhere, Category = "Screens")
    TSoftClassPtr<UUserWidget> MainUIWidget;

    UPROPERTY(Config, EditAnywhere, Category = "Screens")
    TSoftClassPtr<UUserWidget> PathUIWidget;

    UPROPERTY(Config, EditAnywhere, Category = "Screens")
    TSoftClassPtr<UUserWidget> SideMenuUIWidget;

    UPROPERTY(Config, EditAnywhere, Category = "Screens")
    TSoftClassPtr<UUserWidget> GuidePopUpUIWidget;

    UPROPERTY(Config, EditAnywhere, Category = "Screens")
    TSoftClassPtr<UUserWidget> AlertUIWidget;

    UPROPERTY(Config, EditAnywhere, Category = "Screens")
    TSoftClassPtr<UUserWidget> CutsceneWidget;


    UPROPERTY(Config, EditAnywhere, Category = "Data")
    TSoftObjectPtr<UGuideDatabase> GuideDatabase;

    /** [2026/09/17] 시작 컷씬(타이틀 → 튜토리얼) 그림. UUIManagerSubsystem::PlayCutscene(Intro)이 쓴다. */
    UPROPERTY(Config, EditAnywhere, Category = "Data")
    TSoftObjectPtr<UCutSceneDatabase> IntroCutscene;

    /** [2026/09/17] 엔딩 컷씬(Stage2 → 타이틀) 그림. PlayCutscene(Ending)이 쓴다. */
    UPROPERTY(Config, EditAnywhere, Category = "Data")
    TSoftObjectPtr<UCutSceneDatabase> EndingCutscene;

};
