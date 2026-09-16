// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Blueprint/UserWidget.h"
#include "Guide/GuideDatabase.h"
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

};
