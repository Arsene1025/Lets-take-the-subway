// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GridCellRule.generated.h"

class APawn;

/**
 * Decides at runtime whether a pawn may enter a Conditional cell.
 *
 * Authored inline on a marker (EditInlineNew), then duplicated onto the grid actor when
 * overrides are baked -- markers are editor-only, so a rule owned by a marker would be
 * gone in a packaged build. Subclass in C++ or Blueprint; subclasses show up in the
 * marker's rule picker automatically.
 *
 * CanEnter is called both while planning a path (so unreachable cells are routed around)
 * and again immediately before the pawn steps into the cell, because the answer can change
 * between those two moments.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, CollapseCategories)
class LETSTAKETHESUBWAY_API UGridCellRule : public UObject
{
	GENERATED_BODY()

public:
	/** True when Pawn may enter. Must be side-effect free: it runs many times per path search. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Grid Rule")
	bool CanEnter(const APawn* Pawn) const;
	virtual bool CanEnter_Implementation(const APawn* Pawn) const { return true; }

	/** Shown in the HUD when this rule refuses entry. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Grid Rule")
	FText GetDeniedMessage() const;
	virtual FText GetDeniedMessage_Implementation() const;
};

/** Sample rule: the pawn must carry a given actor tag. Used by the test map. */
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
