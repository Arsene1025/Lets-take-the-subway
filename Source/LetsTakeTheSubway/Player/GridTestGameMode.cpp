// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridTestGameMode.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Player/GridHUD.h"
#include "Player/GridPawn.h"
#include "Player/GridPlayerController.h"
#include "Puzzle/PuzzleElevatorDock.h"
#include "Sound/GameSoundSubsystem.h"

#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

AGridTestGameMode::AGridTestGameMode()
{
	DefaultPawnClass = AGridPawn::StaticClass();
	PlayerControllerClass = AGridPlayerController::StaticClass();
	HUDClass = AGridHUD::StaticClass();
}

void AGridTestGameMode::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// BeginPlay가 아니라 여기서 끈다. ABusStopGameMode는 부모 BeginPlay를 건너뛰므로 거기 두면 빠진다.
	if (bDisableShadowCacheWhileHere)
	{
		DisableShadowCache();
	}
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

void AGridTestGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreShadowCache();

	Super::EndPlay(EndPlayReason);
}

void AGridTestGameMode::DisableShadowCache()
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Cache"));
	if (!CVar)
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: r.Shadow.Virtual.Cache not found; shadow ghosting fix skipped."), *GetName());
		return;
	}

	SavedShadowCacheValue = CVar->GetInt();
	if (SavedShadowCacheValue == 0)
	{
		// 이미 꺼져 있으면 건드리지 않는다(되돌릴 것도 없다).
		SavedShadowCacheValue = -1;
		return;
	}

	// 콘솔에서 손으로 바꾼 값보다 우선순위가 낮으면 무시되므로 콘솔과 같은 우선순위로 쓴다.
	CVar->Set(0, ECVF_SetByConsole);
	UE_LOG(LogLTTSGrid, Log, TEXT("%s: r.Shadow.Virtual.Cache %d -> 0 while this level is loaded."),
		*GetName(), SavedShadowCacheValue);
}

void AGridTestGameMode::RestoreShadowCache()
{
	if (SavedShadowCacheValue < 0)
	{
		return;
	}

	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Cache")))
	{
		CVar->Set(SavedShadowCacheValue, ECVF_SetByConsole);
		UE_LOG(LogLTTSGrid, Log, TEXT("%s: r.Shadow.Virtual.Cache restored to %d."), *GetName(), SavedShadowCacheValue);
	}
	SavedShadowCacheValue = -1;
}

void AGridTestGameMode::HandleStageClear(APawn* Pawn, FIntPoint Cell)
{
	// 행인은 StageClear 셀을 밟아도 아무 일이 없어야 한다. 행인도 NotifyPawnEnteredCell을 부르므로
	// (2026-09-15, 개찰구 소리) 이 검사가 실제로 행인을 거른다.
	if (!Cast<AGridPawn>(Pawn))
	{
		return;
	}

	UE_LOG(LogLTTSGrid, Display, TEXT("STAGE CLEAR at cell (%d,%d)."), Cell.X, Cell.Y);

	bool bDockOwnsClear = false;
	for (TActorIterator<APuzzleElevatorDock> It(GetWorld()); It; ++It)
	{
		if (It->bClearOnStageClear)
		{
			bDockOwnsClear = true;
			break;
		}
	}

	if (!bDockOwnsClear)
	{
		if (UGameSoundSubsystem* Sound = UGameSoundSubsystem::Get(this))
		{
			Sound->PlaySound2D(ClearSoundKey);
		}
	}

	if (AGridPlayerController* GridController = Cast<AGridPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		GridController->ShowFeedback(
			FString::Printf(TEXT("STAGE CLEAR at (%d,%d)!"), Cell.X, Cell.Y),
			FLinearColor(0.15f, 1.0f, 0.25f));
	}
}
