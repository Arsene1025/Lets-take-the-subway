// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GridPlayerController.generated.h"

class AGridActor;
class AGridPawn;
class UInputAction;
class UInputMappingContext;

/** Click a cell to move there. Input objects are built in C++, so no IA_/IMC_ assets exist. */
UCLASS()
class LETSTAKETHESUBWAY_API AGridPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGridPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	void ShowFeedback(const FString& Message, const FLinearColor& Color);
	const FString& GetFeedbackText() const { return FeedbackText; }
	const FLinearColor& GetFeedbackColor() const { return FeedbackColor; }

	/** Cell under the cursor. Returns false when the cursor is not over the grid. */
	bool TraceCursorToCell(FIntPoint& OutCell) const;

private:
	void OnClickDestination();

	AGridActor* GetGrid() const;
	AGridPawn* GetGridPawn() const;

	// Created with NewObject at runtime, so they need a UPROPERTY to stay rooted, and
	// Transient so they are never serialized.
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ClickAction;

	FString FeedbackText;
	FLinearColor FeedbackColor = FLinearColor::White;

	FIntPoint LastHoveredCell = FIntPoint(MIN_int32, MIN_int32);
	bool bHadHover = false;
};
