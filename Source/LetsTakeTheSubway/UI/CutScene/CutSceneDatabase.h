// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CutSceneDatabase.generated.h"

/**
 * 
 */

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
