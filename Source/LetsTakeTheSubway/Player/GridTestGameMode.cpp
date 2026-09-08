// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridTestGameMode.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridHUD.h"
#include "Player/GridPawn.h"
#include "Player/GridPlayerController.h"

AGridTestGameMode::AGridTestGameMode()
{
	DefaultPawnClass = AGridPawn::StaticClass();
	PlayerControllerClass = AGridPlayerController::StaticClass();
	HUDClass = AGridHUD::StaticClass();
}

void AGridTestGameMode::BeginPlay()
{
	Super::BeginPlay();

	AGridActor* Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: this level has no AGridActor. Place one, press Generate Grid and save."),
			*GetName());
		return;
	}

	Grid->OnStageClear.AddDynamic(this, &AGridTestGameMode::HandleStageClear);
}

void AGridTestGameMode::HandleStageClear(APawn* Pawn, FIntPoint Cell)
{
	// 행인은 StageClear 셀을 밟아도 아무 일이 없어야 한다. 지금은 행인이
	// NotifyPawnEnteredCell을 부르지 않지만, 스테이지 클리어는 플레이어의 사건이라는
	// 사실을 여기에도 적어 둔다.
	if (!Cast<AGridPawn>(Pawn))
	{
		return;
	}

	UE_LOG(LogLTTSGrid, Display, TEXT("STAGE CLEAR at cell (%d,%d)."), Cell.X, Cell.Y);

	if (AGridPlayerController* GridController = Cast<AGridPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		GridController->ShowFeedback(
			FString::Printf(TEXT("STAGE CLEAR at (%d,%d)!"), Cell.X, Cell.Y),
			FLinearColor(0.15f, 1.0f, 0.25f));
	}
}
