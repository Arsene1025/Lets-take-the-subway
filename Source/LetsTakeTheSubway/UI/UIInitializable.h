// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UIInitializable.generated.h"

/**
 *
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UUIInitializable : public UInterface
{
    GENERATED_BODY()

};


class IUIInitializable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI")
    void Initialize();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI")
    void InitializeInt(int32 paramInt);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI")
    void InitializeText(const FText& paramText);
};