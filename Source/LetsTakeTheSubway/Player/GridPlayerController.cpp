// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridPlayerController.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
#include "Vehicle/GridEscalator.h"
#include "Player/GridPawn.h"
#include "Puzzle/PuzzleBlock.h"
#include "Puzzle/PuzzleElevatorBlock.h"
#include "Puzzle/PuzzleElevatorDock.h"
#include "Puzzle/PuzzleLever.h"
#include "Puzzle/PuzzleRotatingObstacle.h"
#include "Puzzle/PuzzleSubsystem.h"
#include "Vehicle/GridTrain.h"

#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

AGridPlayerController::AGridPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AGridPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 클릭 이동에는 보이면서 잠기지 않은 커서가 필요하다. 이게 없으면 뷰포트가 커서를
	// 붙잡아 디프로젝트된 클릭 위치가 틀어진다.
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	FeedbackText = TEXT("Click a cell to move. Drag a block to push it.");
}

void AGridPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!MappingContext)
	{
		MappingContext = NewObject<UInputMappingContext>(this, TEXT("GridMappingContext"));

		ClickAction = NewObject<UInputAction>(this, TEXT("GridClick"));
		ClickAction->ValueType = EInputActionValueType::Boolean;

		MappingContext->MapKey(ClickAction, EKeys::LeftMouseButton);
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(MappingContext, 0);
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInput->BindAction(ClickAction, ETriggerEvent::Started, this, &AGridPlayerController::OnPressed);
		EnhancedInput->BindAction(ClickAction, ETriggerEvent::Completed, this, &AGridPlayerController::OnReleased);
	}
}

AGridActor* AGridPlayerController::GetGrid() const
{
	return AGridActor::FindGrid(GetWorld());
}

AGridPawn* AGridPlayerController::GetGridPawn() const
{
	return Cast<AGridPawn>(GetPawn());
}

void AGridPlayerController::ShowFeedback(const FString& Message, const FLinearColor& Color)
{
	FeedbackText = Message;
	FeedbackColor = Color;
}

bool AGridPlayerController::TraceCursor(FHitResult& OutHit) const
{
	const AGridActor* Grid = GetGrid();
	if (!Grid)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return false;
	}

	// GetHitResultUnderCursorByChannel은 액터를 무시할 수 없고, 폰이 카메라와 바닥 사이에
	// 있으므로 직접 트레이스한다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LTTSGridClick), /*bTraceComplex*/ false, GetPawn());

	return GetWorld()->LineTraceSingleByChannel(
		OutHit,
		WorldOrigin,
		WorldOrigin + WorldDirection * 100000.0,
		Grid->TraceChannel,
		Params);
}

bool AGridPlayerController::TraceCursorToCell(FIntPoint& OutCell) const
{
	const FCursorPick Pick = PickUnderCursor();
	if (Pick.Kind == FCursorPick::EKind::None)
	{
		return false;
	}

	OutCell = Pick.Cell;
	return true;
}

FCursorPick AGridPlayerController::PickUnderCursor() const
{
	FCursorPick Pick;

	const AGridActor* Grid = GetGrid();
	if (!Grid)
	{
		return Pick;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return Pick;
	}

	const FVector RayEnd = WorldOrigin + WorldDirection * 100000.0;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LTTSGridClick), /*bTraceComplex*/ false, GetPawn());

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, RayEnd, Grid->TraceChannel, Params))
	{
		// 레버는 한 셀 크기이고 폭보다 높이가 크므로, 뒤에 지켜 줄 만한 바닥이 숨어 있지
		// 않다: 어느 면을 눌러도 잡힌다.
		if (APuzzleLever* Lever = Cast<APuzzleLever>(Hit.GetActor()))
		{
			Pick.Kind = FCursorPick::EKind::Lever;
			Pick.Lever = Lever;
			Pick.Cell = Lever->GetCell();
			Pick.HitLocation = Hit.Location;
			return Pick;
		}

		// 열차는 어느 면을 눌러도, 언제 눌러도 잡는다. 누르면 폰이 가장 가까운 문 앞까지
		// 걸어가 기다리므로 "지금 탈 수 있는가"는 더 이상 커서가 답할 질문이 아니다.
		//
		// --- TRAIN PICK GATED BY CanBoard DISABLED 2026-09-11 ---
		// 예전에는 폰이 문 앞 셀에 서 있을 때만 열차를 잡았다. 차체가 승강장 셀을 통째로
		// 가리기 때문인데, 그 대가로 문 앞이 아닌 곳에서 누르면 클릭이 뒤쪽 바닥으로 흘러가
		// 폰이 말없이 걸어가 버렸다. 차체 뒤 승강장 셀을 클릭할 수 없게 되는 쪽을 택했다
		// (사용자 결정, 2026-09-11). 되살리려면 아래 #if 0을 1로 바꾼다.
		if (AGridTrain* Train = Cast<AGridTrain>(Hit.GetActor()))
		{
#if 0
			FText Refusal;
			if (!Train->CanBoard(GetGridPawn(), &Refusal))
			{
				// 잡지는 않되 사유는 들고 간다. 바닥 픽으로 넘어간 뒤 누름이 처리될 때 띄운다.
				Pick.VehicleRefusal = Refusal;
			}
			else
#endif
			{
				Pick.Kind = FCursorPick::EKind::Vehicle;
				Pick.Vehicle = Train;
				Pick.Cell = Grid->WorldToCell(Hit.Location);
				Pick.HitLocation = Hit.Location;
				return Pick;
			}
		}

		// 에스컬레이터는 엘리베이터와 같은 규칙이다: 어느 면을 눌러도 잡히고, 지금 탈 수 없으면
		// 왜 안 되는지 알려 준다. 열차처럼 없는 셈 치고 넘기면 반대쪽 끝에서 눌렀을 때 아무
		// 말도 없이 폰이 엉뚱한 바닥으로 걸어가 버린다 -- 거부 이유가 영영 화면에 닿지 않는다.
		if (AGridEscalator* Escalator = Cast<AGridEscalator>(Hit.GetActor()))
		{
			Pick.Kind = FCursorPick::EKind::Escalator;
			Pick.Escalator = Escalator;
			Pick.Cell = Grid->WorldToCell(Hit.Location);
			Pick.HitLocation = Hit.Location;
			return Pick;
		}

		// 블록은 어느 면으로든 잡힌다. 옆면이 그 뒤의 바닥을 가로막는 것은 여전하지만,
		// 이제 그 셀은 블록을 클릭해서 갈 수 있다: 블록 픽이 FloorCell을 함께 들고 간다.
		//
		// --- TOP-FACE-ONLY PICK DISABLED 2026-09-08 ---
		// 예전에는 윗면 히트만 잡기로 쳤다. 옆면 히트를 그랩으로 취급하면 그 뒤 셀을
		// 영영 클릭할 수 없었기 때문인데, 대신 옆면을 잡으려다 폰이 걸어가 버렸다.
		// 되살리려면 아래 #if 0을 1로 바꾸고 HandleBlockClick의 바닥 이동 분기를 지운다.
		if (APuzzleBlock* Block = Cast<APuzzleBlock>(Hit.GetActor()))
		{
#if 0
			if (Hit.ImpactNormal.Z > 0.7)
#endif
			{
				Pick.Kind = FCursorPick::EKind::Block;
				Pick.Block = Block;
				Pick.Cell = Grid->WorldToCell(Hit.Location);
				Pick.HitLocation = Hit.Location;

				// 이 누름이 드래그가 아니라 클릭으로 끝날 때 폰이 갈 곳. 누른 뒤에 구하면
				// 커서가 이미 움직였을 수 있으므로 픽과 같은 레이로 지금 구해 둔다.
				FVector FloorLocation;
				Pick.bHasFloorCell = TraceFloorIgnoringBlocks(
					WorldOrigin, RayEnd, Params, Pick.FloorCell, FloorLocation);

				return Pick;
			}
		}
	}

	// 모든 블록을 무시한 두 번째 패스. 옆면 히트든, 빗나감이든, 배경 히트든 똑같이
	// 돌려서 바닥 판정이 첫 레이가 우연히 무엇에 닿았느냐가 아니라 하나의 규칙으로
	// 나오게 한다.
	FVector FloorLocation;
	FIntPoint FloorCell;
	if (!TraceFloorIgnoringBlocks(WorldOrigin, RayEnd, Params, FloorCell, FloorLocation))
	{
		return Pick;
	}

	Pick.Kind = FCursorPick::EKind::Floor;
	Pick.Cell = FloorCell;
	Pick.HitLocation = FloorLocation;
	return Pick;
}

bool AGridPlayerController::TraceFloorIgnoringBlocks(
	const FVector& RayOrigin,
	const FVector& RayEnd,
	FCollisionQueryParams Params,
	FIntPoint& OutCell,
	FVector& OutHitLocation) const
{
	const AGridActor* Grid = GetGrid();
	if (!Grid)
	{
		return false;
	}

	// Params를 값으로 받는다: 여기서 더한 무시 목록이 호출자의 1차 트레이스 설정에
	// 남지 않아야 한다.
	TArray<AActor*> BlockActors;
	if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->GetBlockActors(BlockActors);
	}
	Params.AddIgnoredActors(BlockActors);

	FHitResult FloorHit;
	if (!GetWorld()->LineTraceSingleByChannel(FloorHit, RayOrigin, RayEnd, Grid->TraceChannel, Params))
	{
		return false;
	}

	const FIntPoint Cell = Grid->WorldToCell(FloorHit.Location);
	if (!Grid->IsValidCell(Cell))
	{
		return false;
	}

	OutCell = Cell;
	OutHitLocation = FloorHit.Location;
	return true;
}

void AGridPlayerController::OnPressed()
{
	AGridActor* Grid = GetGrid();
	AGridPawn* GridPawn = GetGridPawn();

	if (!Grid || !GridPawn)
	{
		ShowFeedback(TEXT("Grid or pawn is not ready."), FLinearColor::Red);
		return;
	}

	if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		if (Subsystem->IsInputLocked())
		{
			ShowFeedback(TEXT("Wait for the pieces to settle."), FLinearColor(1.0f, 0.65f, 0.05f));
			return;
		}
	}

	const FCursorPick Pick = PickUnderCursor();

	// 누름은 언제나 새로운 뜻이다. 걸어가던 탑승 예약은 여기서 버린다 -- 남겨 두면 플레이어가
	// 다른 곳을 눌러 폰을 돌려세운 뒤에도 예약이 살아남아 엉뚱한 순간에 태운다. 같은 탈것을
	// 다시 눌렀다면 아래에서 새로 예약하며 경로를 다시 잡는다.
	CancelPendingBoarding();

	if (Pick.Kind == FCursorPick::EKind::Lever)
	{
		APuzzleLever* Lever = Pick.Lever.Get();
		if (!Lever)
		{
			return;
		}

		// 드래그가 끝날 때가 아니라 누르는 순간에 거부해서, 플레이어가 제스처를 낭비하기
		// 전에 걸어오라고 알려 준다.
		if (!Lever->IsPawnAdjacent(GridPawn))
		{
			ShowFeedback(TEXT("Stand next to the lever to work it."), FLinearColor(1.0f, 0.65f, 0.05f));
			return;
		}

		FVector2D ScreenCentre;
		if (!ProjectWorldLocationToScreen(Lever->GetWheelWorldLocation(), ScreenCentre))
		{
			return;
		}

		FVector2D MousePosition;
		if (!GetMousePosition(MousePosition.X, MousePosition.Y))
		{
			return;
		}

		DraggedLever = Lever;
		LeverScreenCentre = ScreenCentre;

		// 화면 Y는 아래로 커지므로 부호를 뒤집어, 잰 각도가 플레이어가 보는 대로
		// 반시계 방향으로 증가하게 한다.
		LeverLastAngle = FMath::RadiansToDegrees(FMath::Atan2(
			-(MousePosition.Y - ScreenCentre.Y), MousePosition.X - ScreenCentre.X));
		LeverSweptAngle = 0.0;
		bLeverTurnSpent = false;

		ShowFeedback(TEXT("Turn the wheel."), FLinearColor::White);
		return;
	}

	if (Pick.Kind == FCursorPick::EKind::Vehicle)
	{
		AGridTrain* Train = Pick.Vehicle.Get();
		if (!Train)
		{
			return;
		}

		// 정차역을 가리지 않고 문 앞 셀을 모은다. 클릭한 순간 열차가 터널 한가운데 있을 수도
		// 있으므로, 폰은 "지금 서 있는 역"이 아니라 승강장 전체에서 가장 가까운 문으로 간다.
		TArray<FIntPoint> DoorCells;
		Train->GetApproachCells(DoorCells);

		BeginPendingBoarding(FPendingBoarding::EKind::Train, Train, DoorCells);
		return;
	}

	if (Pick.Kind == FCursorPick::EKind::Escalator)
	{
		AGridEscalator* Escalator = Pick.Escalator.Get();
		if (!Escalator)
		{
			return;
		}

		FText Reason;
		if (Escalator->TryBoard(GridPawn, &Reason))
		{
			ShowFeedback(TEXT("You step onto the escalator."), FLinearColor(0.45f, 0.85f, 1.0f));
		}
		else
		{
			ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
		}
		return;
	}

	if (Pick.Kind == FCursorPick::EKind::Block)
	{
		if (!Pick.Block.IsValid())
		{
			return;
		}

		// 아직 쥐지 않는다. 이 누름이 블록을 밀라는 뜻인지 그 뒤 바닥으로 걸어가라는
		// 뜻인지는 커서가 움직여 봐야 알 수 있다.
		FVector2D MousePosition;
		if (!GetMousePosition(MousePosition.X, MousePosition.Y))
		{
			return;
		}

		PressPick = Pick;
		PressScreenPosition = MousePosition;
		bPressPending = true;
		return;
	}

	if (Pick.Kind == FCursorPick::EKind::Floor)
	{
		// 커서 아래에 열차가 있었는데 탈 수 없었다면 그 이유부터 알려 준다. 걷는 동작은
		// 그대로다 -- 승강장 셀을 클릭하려던 것일 수도 있기 때문이다.
		//
		// 지금은 열차를 언제나 잡으므로 이 사유가 채워지는 일이 없다. PickUnderCursor의
		// TRAIN PICK GATED BY CanBoard 블록을 되살리면 다시 살아난다(2026-09-11).
		if (!Pick.VehicleRefusal.IsEmpty())
		{
			ShowFeedback(Pick.VehicleRefusal.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
		}

		// 바닥에는 끌 것이 없으므로 누르는 즉시 처리한다. 뗄 때까지 기다리면 응답만 늦다.
		MovePawnToCell(Pick.Cell);
		return;
	}

	ShowFeedback(TEXT("Click somewhere on the grid."), FLinearColor::Red);
}

void AGridPlayerController::MovePawnToCell(const FIntPoint& Cell)
{
	AGridActor* Grid = GetGrid();
	AGridPawn* GridPawn = GetGridPawn();

	if (!Grid || !GridPawn)
	{
		return;
	}

	// 바로 옆 칸이 막혀 있는데 클릭했다면 폰을 그쪽으로 살짝 부딪히게 한다. 기획의
	// "좁은 곳을 지나가려 할 때 지나갈 수 없다는 걸 보여 주는 연출"이다. 한 칸 떨어진
	// 곳만 대상으로 하는 이유는, 먼 셀을 클릭한 것은 길이 없다는 안내로 충분하기 때문이다.
	const FIntPoint Delta = Cell - GridPawn->GetCurrentCell();
	const bool bAdjacent = (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y)) == 1;

	if (bAdjacent && !GridPawn->IsMoving() && !Grid->CanPawnEnter(Cell, GridPawn))
	{
		GridPawn->Bump(Cell);
	}

	GridPawn->RequestMoveToCell(Cell);
}

// ---------------------------------------------------------------------------- 예약 탑승

bool AGridPlayerController::BeginPendingBoarding(
	FPendingBoarding::EKind Kind, AActor* Target, const TArray<FIntPoint>& DoorCells)
{
	AGridActor* Grid = GetGrid();
	AGridPawn* GridPawn = GetGridPawn();

	if (!Grid || !GridPawn || !Target || Kind == FPendingBoarding::EKind::None)
	{
		return false;
	}

	if (DoorCells.IsEmpty())
	{
		ShowFeedback(TEXT("There is no door to walk to from this floor."), FLinearColor(1.0f, 0.65f, 0.05f));
		return false;
	}

	// 직선 거리가 아니라 경로 길이로 고른다. 벽 하나를 사이에 둔 코앞의 문을 고르면 폰이
	// 역을 한 바퀴 돌아 그리로 간다.
	FIntPoint DoorCell;
	if (!Grid->FindNearestReachableCell(GridPawn->GetCurrentCell(), DoorCells, GridPawn, DoorCell))
	{
		ShowFeedback(TEXT("There is no way to reach a door from here."), FLinearColor(1.0f, 0.65f, 0.05f));
		return false;
	}

	Pending = FPendingBoarding();
	Pending.Kind = Kind;
	Pending.Target = Target;
	Pending.DoorCell = DoorCell;

	// 이미 문 앞에 서 있으면 걸을 것이 없다. 한 프레임도 기다리지 않고 그 자리에서 태운다.
	if (GridPawn->GetCurrentCell() == DoorCell && !GridPawn->IsMoving())
	{
		TryCompleteBoarding();
		return true;
	}

	GridPawn->RequestMoveToCell(DoorCell);

	// 길찾기가 남긴 "Moving to (x,y)"를 덮는다. 지금 중요한 것은 몇 걸음인지가 아니라
	// 폰이 왜 걸어가는지다.
	ShowFeedback(
		FString::Printf(TEXT("Walking to the door at (%d,%d)."), DoorCell.X, DoorCell.Y),
		FLinearColor(0.45f, 0.85f, 1.0f));

	return true;
}

void AGridPlayerController::CancelPendingBoarding(const FString& Reason)
{
	if (Pending.Kind == FPendingBoarding::EKind::None)
	{
		return;
	}

	Pending = FPendingBoarding();

	if (!Reason.IsEmpty())
	{
		ShowFeedback(Reason, FLinearColor(1.0f, 0.65f, 0.05f));
	}
}

void AGridPlayerController::UpdatePendingBoarding()
{
	AGridPawn* GridPawn = GetGridPawn();
	AActor* Target = Pending.Target.Get();

	if (!GridPawn || !Target)
	{
		CancelPendingBoarding();
		return;
	}

	// 폰이 그리드를 떠났다. 우리가 태운 것이라면 성공한 순간에 예약이 이미 지워졌으므로,
	// 여기까지 왔다는 것은 다른 무언가가 폰을 데려갔다는 뜻이다.
	if (!GridPawn->IsOnGrid())
	{
		CancelPendingBoarding();
		return;
	}

	if (GridPawn->IsMoving())
	{
		return;
	}

	// 서 있는데 문 앞이 아니다: 도중에 길이 막혀 폰이 멈춰 섰다는 뜻이다.
	if (GridPawn->GetCurrentCell() != Pending.DoorCell)
	{
		CancelPendingBoarding(TEXT("Could not reach the door."));
		return;
	}

	TryCompleteBoarding();
}

bool AGridPlayerController::TryCompleteBoarding()
{
	AGridPawn* GridPawn = GetGridPawn();
	if (!GridPawn)
	{
		CancelPendingBoarding();
		return false;
	}

	if (Pending.Kind == FPendingBoarding::EKind::Elevator)
	{
		APuzzleElevatorBlock* Elevator = Cast<APuzzleElevatorBlock>(Pending.Target.Get());
		UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
		APuzzleElevatorDock* Dock = (Elevator && Subsystem) ? Subsystem->FindDockUnder(*Elevator) : nullptr;

		if (!Elevator || !Dock)
		{
			CancelPendingBoarding(TEXT("The elevator is no longer on its dock."));
			return false;
		}

		// 걸어오는 사이에 다른 조각이 아직 정리되는 중일 수 있다. 그동안은 기다린다.
		if (Elevator->IsAnimating() || Dock->IsBusy())
		{
			return false;
		}

		// 차체에게 먼저 태워 달라고 한다. 문 앞에 섰다는 마지막 확인이자, 블루프린트가
		// 연출을 매다는 OnBoarded 이벤트가 나가는 자리다.
		FText Reason;
		if (!Elevator->TryBoard(GridPawn, &Reason))
		{
			CancelPendingBoarding(Reason.ToString());
			return false;
		}

		if (!Dock->TryLaunch(GridPawn, &Reason))
		{
			CancelPendingBoarding(Reason.ToString());
			return false;
		}

		Pending = FPendingBoarding();
		ShowFeedback(TEXT("You step into the elevator."), FLinearColor(0.45f, 0.85f, 1.0f));
		return true;
	}

	if (Pending.Kind == FPendingBoarding::EKind::Train)
	{
		AGridTrain* Train = Cast<AGridTrain>(Pending.Target.Get());
		if (!Train)
		{
			CancelPendingBoarding();
			return false;
		}

		FText Reason;
		if (!Train->CanBoard(GridPawn, &Reason))
		{
			// 열차는 기다리면 온다. 문이 닫혀 있든 아직 터널에 있든 예약은 살려 두고, 왜
			// 아직 못 타는지는 한 번만 알린다 -- 매 프레임 같은 줄을 다시 쓰면 그사이에
			// 일어난 다른 안내를 전부 덮어 버린다.
			if (!Pending.bWaitReported)
			{
				Pending.bWaitReported = true;
				ShowFeedback(
					FString::Printf(TEXT("Waiting at the door. %s"), *Reason.ToString()),
					FLinearColor::White);
			}
			return false;
		}

		if (!Train->TryBoard(GridPawn, &Reason))
		{
			CancelPendingBoarding(Reason.ToString());
			return false;
		}

		Pending = FPendingBoarding();
		ShowFeedback(TEXT("You board the train."), FLinearColor(0.45f, 0.85f, 1.0f));
		return true;
	}

	return false;
}

void AGridPlayerController::OnReleased()
{
	FinishLeverDrag();
	FinishDrag();

	// 드래그로 넘어가지 못한 누름은 클릭이다.
	if (bPressPending)
	{
		bPressPending = false;
		HandleBlockClick(PressPick);
	}
}

void AGridPlayerController::UpdatePendingPress()
{
	// 누른 뒤에 블록이 사라졌다면(퍼즐 리셋 등) 판정할 것이 없다.
	if (!PressPick.Block.IsValid())
	{
		bPressPending = false;
		return;
	}

	// 버튼을 누른 채 커서가 뷰포트를 벗어나면 뗌 이벤트가 유실된다. 그런 누름은 클릭도
	// 드래그도 아닌 것으로 버린다: 커서가 어디서 떨어졌는지 모르는 채 폰을 보내면 안 된다.
	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		bPressPending = false;
		return;
	}

	FVector2D MousePosition;
	if (!GetMousePosition(MousePosition.X, MousePosition.Y))
	{
		return;
	}

	if (FVector2D::Distance(MousePosition, PressScreenPosition) < DragStartThresholdPixels)
	{
		return;
	}

	bPressPending = false;

	// 누른 뒤에 조각이 움직이기 시작했을 수 있다. 그때는 이 제스처를 버린다.
	if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		if (Subsystem->IsInputLocked())
		{
			return;
		}
	}

	BeginBlockDrag(PressPick);
}

void AGridPlayerController::BeginBlockDrag(const FCursorPick& Pick)
{
	APuzzleBlock* Block = Pick.Block.Get();
	if (!Block)
	{
		return;
	}

	// 블록이 움직일 수 없더라도 쥔다: 떼는 순간 왜 안 움직이는지 알려 주기 위해서다.
	//
	// 잡는 지점은 커서의 현재 위치가 아니라 **누른 순간**의 히트 지점이다. 그래야 판정
	// 문턱을 넘느라 움직인 몇 픽셀만큼 블록이 손에서 미끄러지지 않는다.
	DraggedBlock = Block;
	GrabPoint = Pick.HitLocation;
	GrabOffset = Pick.HitLocation - Block->GetActorLocation();
	DragAxis = Block->GetWorldMoveAxis();
	bHasRefusedDir = false;
	StepsThisDrag = 0;
	Block->SetHeld(true);
}

void AGridPlayerController::HandleBlockClick(const FCursorPick& Pick)
{
	APuzzleBlock* Block = Pick.Block.Get();
	if (!Block)
	{
		return;
	}

	// 엘리베이터만은 클릭에 고유한 뜻이 있다: 탑승. 차체가 곧 문이므로 그 위를 클릭하는
	// 것을 뒤쪽 바닥으로 걸어가라는 뜻으로 읽을 수는 없다.
	if (APuzzleElevatorBlock* Elevator = Cast<APuzzleElevatorBlock>(Block))
	{
		// 층을 옮기는 것은 차체가 아니라 그 아래의 구조물이다. 구조물 위가 아니면 차체는
		// 그냥 밀 수 있는 상자이며, 그 사실을 누른 자리에서 바로 알려 준다.
		UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
		APuzzleElevatorDock* Dock = Subsystem ? Subsystem->FindDockUnder(*Elevator) : nullptr;

		if (!Dock)
		{
			ShowFeedback(TEXT("Push the elevator onto its dock first."), FLinearColor(1.0f, 0.65f, 0.05f));
			return;
		}

		// 탈 수 없는 엘리베이터라면 폰을 문 앞까지 걸어 보내 놓고 거기서 거절하는 것보다,
		// 누른 자리에서 곧바로 이유를 알려 주는 편이 낫다.
		FText Reason;
		if (!Dock->CanLaunch(&Reason))
		{
			ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
			return;
		}

		// 이 층에서 실제로 탈 수 있는 문 앞 셀만 후보로 삼는다. 샤프트에 걸린 차체는 한쪽
		// 문이 선로나 다른 층을 향하고 있어, 그쪽으로 폰을 보내면 영영 닿지 못한다.
		TArray<FIntPoint> DoorCells;
		Elevator->GetBoardableDoorCells(DoorCells);

		BeginPendingBoarding(FPendingBoarding::EKind::Elevator, Elevator, DoorCells);
		return;
	}

	// 그 밖의 블록 위 클릭은 그 블록이 없는 셈 치고 폰을 보낸다. 블록이 선 셀 자체를
	// 클릭한 것이라면 기존 "Blocked by object" 거부가 그대로 나온다.
	if (Pick.bHasFloorCell)
	{
		MovePawnToCell(Pick.FloorCell);
		return;
	}

	ShowFeedback(TEXT("Click somewhere on the grid."), FLinearColor::Red);
}

// ---------------------------------------------------------------------------- 레버 드래그

void AGridPlayerController::UpdateLeverDrag()
{
	APuzzleLever* Lever = DraggedLever.Get();
	if (!Lever)
	{
		FinishLeverDrag();
		return;
	}

	// 버튼을 누른 채 커서가 뷰포트를 벗어나면 뗌 이벤트가 유실된다.
	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		FinishLeverDrag();
		return;
	}

	FVector2D MousePosition;
	if (!GetMousePosition(MousePosition.X, MousePosition.Y))
	{
		return;
	}

	const FVector2D Offset(MousePosition.X - LeverScreenCentre.X, MousePosition.Y - LeverScreenCentre.Y);

	// 휠 바로 위에서는 각도가 의미 없고 심하게 떨리므로, 커서가 방향을 읽을 만큼 충분히
	// 벗어날 때까지 기다린다.
	constexpr double MinRadiusPixels = 12.0;
	if (Offset.Size() < MinRadiusPixels)
	{
		return;
	}

	const double Angle = FMath::RadiansToDegrees(FMath::Atan2(-Offset.Y, Offset.X));

	// 프레임 간 델타로 누적하므로, 각도가 한 바퀴 경계를 넘어도 되돌아 튀지 않고 계속
	// 세어진다.
	LeverSweptAngle += FMath::FindDeltaAngleDegrees(LeverLastAngle, Angle);
	LeverLastAngle = Angle;

	// 화면 시계 방향은 쓸어 돈 각도로는 음수이고 월드에서는 양의 yaw이므로 부호를 뒤집는다.
	// 카메라가 yaw 45도를 보고 있어 양의 월드 yaw는 화면에서도 시계 방향으로 보이므로, 커서를
	// 시계 방향으로 돌리는 것은 양의 회전을 요청하는 것이다.
	const int32 SweptSign = (LeverSweptAngle < 0.0) ? 1 : -1;

	// 휠은 커서를 따라가되, 레버가 받지 않는 방향으로는 따라 돌지 않는다. 임계값까지 돌렸다가
	// 거부당하는 것보다, 움직이지 않는 휠이 이쪽은 막혔다고 먼저 알려 주는 편이 친절하다.
	Lever->SetWheelPreviewAngle(Lever->AllowsTurn(SweptSign) ? static_cast<float>(-LeverSweptAngle) : 0.0f);

	if (bLeverTurnSpent)
	{
		return;		// 드래그 한 번에 90도 회전 한 번: 더 돌리려면 놓았다가 다시 잡는다
	}

	if (FMath::Abs(LeverSweptAngle) < Lever->TurnThresholdDegrees)
	{
		return;
	}

	bLeverTurnSpent = true;

	FText Reason;
	if (!Lever->TryTurn(SweptSign, GetGridPawn(), &Reason))
	{
		ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
	}
}

void AGridPlayerController::FinishLeverDrag()
{
	APuzzleLever* Lever = DraggedLever.Get();
	DraggedLever.Reset();

	if (!Lever)
	{
		return;
	}

	// 휠은 놓인 자리에 머무르지 않고 원래 자리로 돌아간다: 휠은 조작 장치일 뿐이고, 쉬고
	// 있을 때의 각도는 그것이 돌리는 구조물에 대해 아무 의미도 갖지 않는다.
	Lever->SetWheelPreviewAngle(0.0f);

	if (!bLeverTurnSpent)
	{
		// 방향이 고정된 휠이라면 그것까지 알려 준다. 그렇지 않으면 막힌 쪽으로 계속 돌려 보면서
		// 드래그가 짧아서 안 되는 것으로 오해하기 쉽다.
		if (Lever->Direction == EPuzzleRotationDirection::Free)
		{
			ShowFeedback(TEXT("Drag around the wheel to turn it a quarter."), FLinearColor::White);
		}
		else
		{
			ShowFeedback(
				FString::Printf(TEXT("Drag %s around the wheel to turn it a quarter."),
					LTTSPuzzle::DescribeTurnSign(LTTSPuzzle::ResolveTurnSign(Lever->Direction, 1))),
				FLinearColor::White);
		}
	}

	LeverSweptAngle = 0.0;
	bLeverTurnSpent = false;
}

void AGridPlayerController::UpdateDrag()
{
	APuzzleBlock* Block = DraggedBlock.Get();
	const AGridActor* Grid = GetGrid();

	if (!Block || !Grid)
	{
		FinishDrag();
		return;
	}

	// 버튼을 누른 채 커서가 뷰포트를 벗어나면 뗌 이벤트가 유실되고, 그러면 블록이
	// 마우스에 붙은 채로 남는다.
	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		FinishDrag();
		return;
	}

	if (Block->IsAnimating())
	{
		return;		// 한 번에 한 셀; 스텝이 끝나기를 기다린다
	}

	// 드래그 한 번에 이동 한 번. 블록은 계속 쥔 상태로 두되, 플레이어가 놓았다가 다시
	// 잡기 전까지는 더 시도하지 않는다.
	if (Block->bOneStepPerDrag && StepsThisDrag >= 1)
	{
		return;
	}

	if (DragAxis == EPuzzleMoveAxis::None)
	{
		return;		// 움직일 수 없는 블록: 뗄 때 왜 안 움직이는지 알려 주려고 쥐고만 있는다
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return;
	}

	// 트레이스하지 않고 블록을 잡은 평면과 교차시킨다: 트레이스는 블록 자신의 윗면에
	// 맞고, 블록이 높아질수록 그 히트 아래 셀이 원근 때문에 밀려나기 때문이다.
	const FVector Cursor = FMath::LinePlaneIntersection(
		WorldOrigin,
		WorldOrigin + WorldDirection * 100000.0,
		FPlane(GrabPoint, FVector::UpVector));

	// 드래그 시작점이 아니라 블록의 현재 위치에서 재기 때문에 자유 블록을 모퉁이 너머로
	// 끌고 갈 수 있다: 스텝마다 커서 기준으로 새로 고른다.
	const FVector Target = Cursor - GrabOffset;
	const FVector Current = Block->GetActorLocation();

	double DeltaX = (DragAxis == EPuzzleMoveAxis::AxisY) ? 0.0 : Target.X - Current.X;
	double DeltaY = (DragAxis == EPuzzleMoveAxis::AxisX) ? 0.0 : Target.Y - Current.Y;

	const double Threshold = Grid->CellSize * 0.5;

	// 커서가 가장 멀리 끌고 간 축을 먼저 시도한다. 다른 축으로 넘어가는 덕분에 벽에
	// 막힌 블록도 벽을 따라 슬라이드할 수 있다.
	EGridDirection Candidates[2];
	int32 NumCandidates = 0;

	const bool bPreferX = FMath::Abs(DeltaX) >= FMath::Abs(DeltaY);
	const double Primary = bPreferX ? DeltaX : DeltaY;
	const double Secondary = bPreferX ? DeltaY : DeltaX;

	if (FMath::Abs(Primary) >= Threshold)
	{
		Candidates[NumCandidates++] = bPreferX
			? (Primary > 0.0 ? EGridDirection::East : EGridDirection::West)
			: (Primary > 0.0 ? EGridDirection::North : EGridDirection::South);
	}
	if (FMath::Abs(Secondary) >= Threshold)
	{
		Candidates[NumCandidates++] = bPreferX
			? (Secondary > 0.0 ? EGridDirection::North : EGridDirection::South)
			: (Secondary > 0.0 ? EGridDirection::East : EGridDirection::West);
	}

	if (NumCandidates == 0)
	{
		bHasRefusedDir = false;		// 커서가 반 셀 안으로 돌아왔다; 메시지를 다시 준비한다
		return;
	}

	for (int32 Index = 0; Index < NumCandidates; ++Index)
	{
		if (Block->StartSlide(Candidates[Index]))
		{
			++StepsThisDrag;
			bHasRefusedDir = false;
			return;
		}
	}

	// 아무것도 움직이지 않았다. 플레이어가 실제로 끌던 방향을 보고한다.
	const EGridDirection Refused = Candidates[0];
	if (!bHasRefusedDir || LastRefusedDir != Refused)
	{
		FText Reason;
		Block->CanSlide(Refused, &Reason);
		LastRefusedDir = Refused;
		bHasRefusedDir = true;
		ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
	}
}

void AGridPlayerController::FinishDrag()
{
	APuzzleBlock* Block = DraggedBlock.Get();
	DraggedBlock.Reset();

	if (!Block)
	{
		return;
	}

	const int32 Steps = StepsThisDrag;
	StepsThisDrag = 0;
	bHasRefusedDir = false;
	DragAxis = EPuzzleMoveAxis::None;
	GrabOffset = FVector::ZeroVector;

	Block->SetHeld(false);

	if (Steps > 0)
	{
		return;		// 밀기였다; 회전 검사는 마지막 스텝이 끝날 때 발동한다
	}

	// 여기까지 왔다면 드래그로 판정된 누름인데 블록이 한 칸도 가지 못한 것이다. 끌지 않은
	// 누름은 클릭이므로 애초에 이 함수로 오지 않고 HandleBlockClick으로 간다.
	if (Block->IsA<APuzzleRotatingObstacle>())
	{
		ShowFeedback(TEXT("This turns only when its lever is turned."), FLinearColor::White);
		return;
	}

	// 저작에서 고정해 둔 물건. 축 문구("moves nowhere")는 규칙을 설명하지 못하므로 먼저
	// 가로챈다 -- 플레이어가 알아야 하는 것은 "이 벤치는 붙박이다"이지 축이 아니다.
	if (!Block->bCanMove)
	{
		ShowFeedback(TEXT("This object is fixed in place."), FLinearColor::White);
		return;
	}

	const EPuzzleMoveAxis Axis = Block->GetWorldMoveAxis();
	const TCHAR* AxisText =
		(Axis == EPuzzleMoveAxis::AxisX) ? TEXT("east and west") :
		(Axis == EPuzzleMoveAxis::AxisY) ? TEXT("north and south") :
		(Axis == EPuzzleMoveAxis::Both) ? TEXT("any direction") : TEXT("nowhere");

	// 엘리베이터를 끌었지만 밀리지 않았다면, 탑승이 드래그가 아니라 클릭이라는 것부터
	// 알려 주는 편이 낫다.
	if (Block->IsA<APuzzleElevatorBlock>())
	{
		ShowFeedback(
			FString::Printf(TEXT("This elevator moves %s. Click it to board."), AxisText),
			FLinearColor::White);
		return;
	}

	ShowFeedback(FString::Printf(TEXT("Drag to push. This block moves %s."), AxisText), FLinearColor::White);
}

FString AGridPlayerController::GetDragStatusText() const
{
	if (HasPendingBoarding())
	{
		return FString::Printf(TEXT("Boarding %s via cell (%d,%d)%s"),
			*GetNameSafe(Pending.Target.Get()),
			Pending.DoorCell.X, Pending.DoorCell.Y,
			Pending.bWaitReported ? TEXT(", waiting") : TEXT(""));
	}

	if (const APuzzleLever* Lever = DraggedLever.Get())
	{
		return FString::Printf(TEXT("Turning %s (%.0f deg%s)"),
			*Lever->GetName(), LeverSweptAngle, bLeverTurnSpent ? TEXT(", spent") : TEXT(""));
	}

	const APuzzleBlock* Block = DraggedBlock.Get();
	if (!Block)
	{
		return FString();
	}

	return FString::Printf(TEXT("Holding %s (%d step(s))"), *Block->GetName(), StepsThisDrag);
}

void AGridPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// --- CUTAWAY DISABLED 2026-09-04 ---
#if 0
	// 드래그와 호버 둘 다 플레이어가 보고 있는 높이를 쓰도록 그 앞에서 실행한다.
	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		const AGridPawn* GridPawn = GetGridPawn();
		const float SweepRadius = GridPawn ? GridPawn->BallRadius * 0.5f : 25.0f;
		const FVector CameraLocation = PlayerCameraManager
			? PlayerCameraManager->GetCameraLocation()
			: FVector::ZeroVector;

		Subsystem->UpdateOcclusion(CameraLocation, GridPawn, SweepRadius);
	}
#endif

	// 잡기보다 먼저 본다. 문턱을 넘는 프레임에 바로 드래그가 시작되도록.
	if (bPressPending)
	{
		UpdatePendingPress();
	}

	if (DraggedLever.IsValid())
	{
		UpdateLeverDrag();
	}

	if (DraggedBlock.IsValid())
	{
		UpdateDrag();
	}

	if (HasPendingBoarding())
	{
		UpdatePendingBoarding();
	}

	UpdateHover();
}

void AGridPlayerController::UpdateHover()
{
	const AGridActor* Grid = GetGrid();

	auto Clear = [this]()
	{
		if (bHadHover)
		{
			FGridRuntimeDebugDrawer::ClearHover(GetWorld());
			bHadHover = false;
			LastHoveredCell = FIntPoint(MIN_int32, MIN_int32);
			LastHoverKind = FCursorPick::EKind::None;
			LastHoveredBlock.Reset();
		}
	};

	if (!LTTSGridDebug::ShouldDrawWorld() || !Grid)
	{
		Clear();
		return;
	}

	const FCursorPick Pick = PickUnderCursor();
	if (Pick.Kind == FCursorPick::EKind::None)
	{
		Clear();
		return;
	}

	// 배치는 유지되므로 답이 실제로 바뀔 때만 다시 그린다.
	if (bHadHover
		&& Pick.Kind == LastHoverKind
		&& Pick.Cell == LastHoveredCell
		&& Pick.Block == LastHoveredBlock)
	{
		return;
	}

	if (Pick.Kind == FCursorPick::EKind::Lever)
	{
		if (const APuzzleLever* Lever = Pick.Lever.Get())
		{
			// 레버 자신의 셀이 아니라 레버를 조작할 수 있는 셀들: 손을 뻗기 전에 플레이어가
			// 알아야 할 단 하나는 어디에 서야 하느냐다.
			TArray<FIntPoint> Cells;
			Lever->GetOperatingCells(Cells);
			FGridRuntimeDebugDrawer::DrawHoverCells(GetWorld(), *Grid, Cells, /*bEnterable*/ true);
		}
	}
	else if (Pick.Kind == FCursorPick::EKind::Vehicle)
	{
		if (const AGridTrain* Train = Pick.Vehicle.Get())
		{
			// 차체가 아니라 탈 수 있는 자리를 그린다. 레버와 같은 생각이다: 누르기 전에
			// 플레이어가 알아야 하는 것은 폰이 어디로 걸어갈 것인가다.
			TArray<FIntPoint> Cells;
			Train->GetApproachCells(Cells);
			FGridRuntimeDebugDrawer::DrawHoverCells(GetWorld(), *Grid, Cells, /*bEnterable*/ true);
		}
	}
	else if (Pick.Kind == FCursorPick::EKind::Block)
	{
		if (const APuzzleBlock* Block = Pick.Block.Get())
		{
			// 레이가 우연히 닿은 셀 하나가 아니라 풋프린트 전체를 그려서, 플레이어가 무엇을
			// 잡으려는 건지 볼 수 있게 한다.
			TArray<FIntPoint> Cells;
			Block->GatherOccupiedCells(Cells);

			// 엘리베이터는 밀 수도 있고 탈 수도 있다. 탈 수 있는 문 앞 셀까지 함께 그려서
			// 클릭이 폰을 어디로 보낼지 보이게 한다.
			if (const APuzzleElevatorBlock* Elevator = Cast<APuzzleElevatorBlock>(Block))
			{
				TArray<FIntPoint> DoorCells;
				Elevator->GetBoardableDoorCells(DoorCells);
				Cells.Append(DoorCells);
			}

			FGridRuntimeDebugDrawer::DrawHoverCells(GetWorld(), *Grid, Cells, /*bEnterable*/ true);
		}
	}
	else
	{
		FGridRuntimeDebugDrawer::DrawHover(GetWorld(), *Grid, Pick.Cell, Grid->CanPawnEnter(Pick.Cell, GetPawn()));
	}

	LastHoveredCell = Pick.Cell;
	LastHoverKind = Pick.Kind;
	LastHoveredBlock = Pick.Block;
	bHadHover = true;
}
