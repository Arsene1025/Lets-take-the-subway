// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/GridCellRule.h"
#include "GameFramework/Pawn.h"

FText UGridCellRule::GetDeniedMessage_Implementation() const
{
	return NSLOCTEXT("LTTSGrid", "CellRuleDenied", "You cannot enter that cell yet.");
}

bool UGridCellRule_PawnHasTag::CanEnter_Implementation(const APawn* Pawn) const
{
	return Pawn != nullptr && Pawn->ActorHasTag(RequiredTag);
}

FText UGridCellRule_PawnHasTag::GetDeniedMessage_Implementation() const
{
	return FText::Format(
		NSLOCTEXT("LTTSGrid", "CellRuleNeedsTag", "Requires '{0}'."),
		FText::FromName(RequiredTag));
}
