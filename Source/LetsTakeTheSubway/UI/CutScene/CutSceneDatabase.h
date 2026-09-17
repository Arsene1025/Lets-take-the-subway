// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CutSceneDatabase.generated.h"

/**
 * 그림 컷씬 한 편 = FCutsceneFrame 배열. 위젯(UCutSceneWidget)이 순서대로 그린다.
 */

/**
 * 어느 컷씬인지(2026-09-17). 데이터 애셋은 UUISettings::IntroCutscene / EndingCutscene에서 찾는다.
 * 컷씬이 늘면 여기와 설정 항목, UUIManagerSubsystem::FindCutsceneDatabase에 한 줄씩 더한다.
 */
UENUM(BlueprintType)
enum class ECutsceneKind : uint8
{
    /** 타이틀에서 시작을 누르면 튜토리얼 전에 나온다(UI_Open_1~4). */
    Intro,

    /** Stage2 엔딩 볼륨에 들어서면 타이틀로 돌아가기 전에 나온다(UI_End_1~4). */
    Ending
};

USTRUCT(BlueprintType)
struct FCutsceneFrame
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> Image;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Duration = 5.0f;
};


UCLASS(BlueprintType)
class LETSTAKETHESUBWAY_API UCutSceneDatabase : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FCutsceneFrame> Frames;
};
