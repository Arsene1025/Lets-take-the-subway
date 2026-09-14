// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridHUD.h"

#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
#include "NPC/GridNPC.h"
#include "Player/GridPawn.h"
#include "Player/GridPlayerController.h"
#include "Puzzle/PuzzleSubsystem.h"
#include "Stage/StageSubsystem.h"
#include "Stage/StageZoneVolume.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"

void AGridHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!LTTSGridDebug::ShouldDrawHUD())
	{
		return;
	}

	AGridPlayerController* GridController = Cast<AGridPlayerController>(GetOwningPlayerController());
	AGridPawn* GridPawn = GridController ? Cast<AGridPawn>(GridController->GetPawn()) : nullptr;
	const AGridActor* Grid = GridPawn ? GridPawn->GetGrid() : AGridActor::FindGrid(GetWorld());

	const FLinearColor Label(0.75f, 0.82f, 0.90f);
	const FLinearColor Value(1.0f, 0.82f, 0.2f);

	float Y = 30.0f;
	const float X = 35.0f;
	const float LineHeight = 23.0f;

	DrawText(TEXT("GRID MOVEMENT"), FLinearColor::White, X, Y, GEngine->GetMediumFont(), 1.1f);
	Y += LineHeight * 1.4f;

	DrawText(TEXT("WASD / Arrows: move   Left Drag: push a block"), Label, X, Y);
	Y += LineHeight;
	DrawText(TEXT("Click an elevator, train or escalator to walk over and board"), Label, X, Y);
	Y += LineHeight;
	DrawText(TEXT("Console: ltts.GridDebug 0 | 1 | 2"), Label, X, Y);
	Y += LineHeight * 1.4f;

	if (Grid)
	{
		DrawText(
			FString::Printf(TEXT("Grid %dx%d  walkable %d  blocked %d  noFloor %d  stageClear %d  conditional %d"),
				Grid->SizeInCells.X, Grid->SizeInCells.Y,
				Grid->NumWalkable, Grid->NumBlocked, Grid->NumNoFloor,
				Grid->NumStageClear, Grid->NumConditional),
			Label, X, Y);
		Y += LineHeight;
	}
	else
	{
		DrawText(TEXT("No grid actor in this level."), FLinearColor::Red, X, Y);
		Y += LineHeight;
	}

	if (GridPawn)
	{
		const FIntPoint Current = GridPawn->GetCurrentCell();
		const FIntPoint Goal = GridPawn->GetGoalCell();

		// 눌린 이동 키가 어느 셀 축으로 읽혔는지. 카메라 각도와 KeyboardYawOffset이 맞는지
		// 확인하려면 이것이 가장 빠르다.
		const TOptional<EGridDirection> Held = GridPawn->GetHeldDirection();
		const FString HeldText = Held.IsSet()
			? FString::Printf(TEXT("  [holding %s]"),
				*StaticEnum<EGridDirection>()->GetNameStringByValue(static_cast<int64>(Held.GetValue())))
			: FString();

		DrawText(
			FString::Printf(TEXT("Current (%d,%d)%s%s"), Current.X, Current.Y,
				GridPawn->IsMoving() ? TEXT("  [moving]") : TEXT(""),
				*HeldText),
			Value, X, Y);
		Y += LineHeight;

		DrawText(
			FString::Printf(TEXT("Goal (%d,%d)   steps left %d"), Goal.X, Goal.Y, GridPawn->GetRemainingSteps()),
			Value, X, Y);
		Y += LineHeight;

		if (const FGridCellData* Cell = Grid ? Grid->GetCell(Current) : nullptr)
		{
			DrawText(
				FString::Printf(TEXT("Floor Z %.1f   slope %.1f deg"), Cell->FloorZ, Cell->SlopeDeg),
				Label, X, Y);
			Y += LineHeight;
		}
	}

	if (const UPuzzleSubsystem* Puzzle = UPuzzleSubsystem::Get(this))
	{
		FString Status = FString::Printf(TEXT("Blocks %d   obstacles %d   occupied cells %d"),
			Puzzle->GetNumBlocks(), Puzzle->GetNumObstacles(), Grid ? Grid->GetNumOccupiedCells() : 0);

		// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
		if (const int32 Hidden = Puzzle->GetNumOccludingBlocks())
		{
			Status += FString::Printf(TEXT("   hidden by %d"), Hidden);
		}
#endif

		if (Puzzle->IsInputLocked())
		{
			Status += TEXT("   [pieces moving]");
		}
		else if (GridController && (GridController->IsDraggingBlock() || GridController->IsDraggingLever()))
		{
			Status += TEXT("   ") + GridController->GetDragStatusText();
		}

		DrawText(Status, Label, X, Y);
		Y += LineHeight;
	}

	// 스테이지·구역. UI 매니저가 받는 값과 같은 스냅샷이다.
	if (const UStageSubsystem* Stage = UStageSubsystem::Get(this))
	{
		const FStageState& StageState = Stage->GetState();
		const FString StageText = StageState.bResolved
			? FString::Printf(TEXT("Stage %d '%s'   zone %d / %d '%s'"),
				StageState.StageIndex, *StageState.StageName.ToString(),
				StageState.CurrentZoneIndex, StageState.ZoneCount, *StageState.CurrentZoneName.ToString())
			: FString(TEXT("Stage [unresolved]"));
		DrawText(StageText, Value, X, Y);
		Y += LineHeight;

		// 레벨 2에서는 구역 박스를 그린다. 현재 구역은 초록, 나머지는 회색. 매 프레임 한 프레임짜리로 그린다.
		if (LTTSGridDebug::ShouldDrawWorld())
		{
			const AStageZoneVolume* Current = Stage->GetCurrentZone();
			for (const TWeakObjectPtr<AStageZoneVolume>& Entry : Stage->GetZones())
			{
				const AStageZoneVolume* Zone = Entry.Get();
				const UBoxComponent* ZoneBox = Zone ? Zone->GetBox() : nullptr;
				if (!ZoneBox)
				{
					continue;
				}

				const bool bIsCurrent = Zone == Current;
				DrawDebugBox(GetWorld(), ZoneBox->GetComponentLocation(), ZoneBox->GetScaledBoxExtent(),
					ZoneBox->GetComponentQuat(), bIsCurrent ? FColor::Green : FColor(140, 140, 140),
					false, -1.0f, 0, bIsCurrent ? 6.0f : 2.0f);
			}
		}
	}

	// 지금 시점을 맡은 대상. 구역 카메라, 보간 중인지, 폰 디버그 카메라인지.
	if (GridController)
	{
		const APlayerCameraManager* CameraManager = GridController->PlayerCameraManager;
		const AActor* ViewTarget = GridController->GetViewTarget();
		const AActor* PendingTarget = CameraManager ? CameraManager->PendingViewTarget.Target.Get() : nullptr;

		FString ViewText = FString::Printf(TEXT("View %s"), ViewTarget ? *ViewTarget->GetActorNameOrLabel() : TEXT("none"));
		if (PendingTarget)
		{
			ViewText += FString::Printf(TEXT(" -> %s [blending]"), *PendingTarget->GetActorNameOrLabel());
		}

		FLinearColor ViewColor = Label;
		if (GridPawn && GridPawn->IsPawnCameraActive())
		{
			// --- PAWN CAMERA DISABLED 2026-09-14 ---
			// 스위치로 강제한 것인지, 구역 카메라가 없어 자동으로 켜진 것인지 구분한다.
			ViewText += GridPawn->ShouldUsePawnCamera()
				? TEXT("   [pawn debug camera: ltts.PawnCamera 0 to turn off]")
				: TEXT("   [pawn camera: no zone cameras in this level]");
		}
		else if (GridPawn && ViewTarget == GridPawn && !PendingTarget)
		{
			// 폰이 뷰 타깃인데 폰 카메라가 꺼져 있으면 공 안에서 수평으로 보는 이상한 시점이 된다.
			ViewText += TEXT("   [no zone camera and pawn camera off: ltts.PawnCamera 1]");
			ViewColor = FLinearColor(1.0f, 0.35f, 0.3f);
		}

		DrawText(ViewText, ViewColor, X, Y);
		Y += LineHeight;
	}

	// 행인은 퍼즐에 속하지 않으므로 서브시스템이 아니라 월드에서 직접 센다.
	{
		int32 NumNPCs = 0;
		for (TActorIterator<AGridNPC> It(GetWorld()); It; ++It)
		{
			++NumNPCs;
		}

		if (NumNPCs > 0)
		{
			DrawText(FString::Printf(TEXT("NPCs alive %d"), NumNPCs), Label, X, Y);
			Y += LineHeight;
		}
	}

	// 폰이 탈것에 실려 있는 동안에는 셀 좌표가 의미를 잃으므로, 대신 무엇을 타고 있는지 쓴다.
	if (const AGridPawn* RidingPawn = Cast<AGridPawn>(GetOwningPawn()))
	{
		if (!RidingPawn->IsOnGrid())
		{
			const TCHAR* StateText =
				(RidingPawn->GetRideState() == AGridPawn::ERideState::Entering) ? TEXT("boarding") :
				(RidingPawn->GetRideState() == AGridPawn::ERideState::Riding) ? TEXT("riding") : TEXT("stepping off");

			DrawText(
				FString::Printf(TEXT("Pawn %s %s"), StateText, *GetNameSafe(RidingPawn->GetVehicle())),
				FLinearColor(0.45f, 0.85f, 1.0f), X, Y);
			Y += LineHeight;
		}
	}

	if (GridController && !GridController->GetFeedbackText().IsEmpty())
	{
		Y += LineHeight * 0.4f;
		DrawText(GridController->GetFeedbackText(), GridController->GetFeedbackColor(), X, Y, GEngine->GetMediumFont());
	}
}
