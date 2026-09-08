// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GridCellRule.generated.h"

class APawn;

/**
 * 폰이 Conditional 셀에 들어갈 수 있는지 런타임에 결정한다.
 *
 * 마커에 인라인으로 저작(EditInlineNew)한 뒤, 오버라이드를 구울 때 그리드 액터로
 * 복제한다 -- 마커는 에디터 전용이라 마커가 소유한 규칙은 패키징 빌드에서 사라지기
 * 때문이다. C++나 블루프린트로 서브클래스를 만들면 마커의 규칙 선택 목록에
 * 자동으로 나타난다.
 *
 * CanEnter는 경로를 계획할 때(들어갈 수 없는 셀을 우회하도록)와, 폰이 셀에
 * 들어서기 직전에 다시 한 번 호출된다. 두 시점 사이에 답이 바뀔 수 있기
 * 때문이다.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, CollapseCategories)
class LETSTAKETHESUBWAY_API UGridCellRule : public UObject
{
	GENERATED_BODY()

public:
	/** Pawn이 들어갈 수 있으면 true. 부수 효과가 없어야 한다: 경로 탐색 한 번에 여러 번 실행된다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Grid Rule")
	bool CanEnter(const APawn* Pawn) const;
	virtual bool CanEnter_Implementation(const APawn* Pawn) const { return true; }

	/** 이 규칙이 진입을 거부할 때 HUD에 표시된다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Grid Rule")
	FText GetDeniedMessage() const;
	virtual FText GetDeniedMessage_Implementation() const;
};

/** 예시 규칙: 폰에 지정된 액터 태그가 있어야 한다. 테스트 맵에서 쓴다. */
UCLASS(meta = (DisplayName = "Pawn Has Tag"))
class LETSTAKETHESUBWAY_API UGridCellRule_PawnHasTag : public UGridCellRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FName RequiredTag = TEXT("HasTicket");

	virtual bool CanEnter_Implementation(const APawn* Pawn) const override;
	virtual FText GetDeniedMessage_Implementation() const override;
};
