// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GuideType.h"
#include "GuideDatabase.generated.h"


USTRUCT(BlueprintType)
struct FGuideEntry : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true))
    FText Body;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UTexture2D> Image;

    //UPROPERTY(EditAnywhere, BlueprintReadOnly)
    //TSoftObjectPtr<UMediaSource> Video;
};

/**
 * 
 */
UCLASS(BlueprintType)
class LETSTAKETHESUBWAY_API UGuideDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
    TMap<EGuideType, FGuideEntry> Entries;
};