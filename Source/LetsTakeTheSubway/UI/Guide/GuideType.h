#pragma once

#include "CoreMinimal.h"
#include "GuideType.generated.h"


UENUM(BlueprintType)
enum class EGuideType : uint8
{
    None        UMETA(DisplayName = "None"),
    Tutorial1   UMETA(DisplayName = "Tutorial1"),
    Tutorial2   UMETA(DisplayName = "Tutorial2"),
    Stage1_1    UMETA(DisplayName = "Stage1_1"),
    Stage1_2    UMETA(DisplayName = "Stage1_2"),
    Stage1_3    UMETA(DisplayName = "Stage1_3"),
    Stage2_1    UMETA(DisplayName = "Stage2_1"),

    MAXVALUE    UMETA(DisplayName = "MAXVALUE"),
};