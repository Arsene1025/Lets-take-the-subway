// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GuideDatabase.h"
#include "GuideDataSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class LETSTAKETHESUBWAY_API UGuideDataSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintPure, Category = "Guide")
    bool GetEntry(EGuideType Type, FGuideEntry& OutEntry) const;

private:
    UPROPERTY()
    TObjectPtr<UGuideDatabase> Database;

    //TArray<TSharedPtr<FStreamableHandle>> ActiveHandles;
};