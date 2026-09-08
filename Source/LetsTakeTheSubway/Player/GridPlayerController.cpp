// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GridPlayerController.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"
#include "Grid/GridDebug.h"
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

		// 열차는 문이 열려 있고 폰이 그 문 앞에 서 있을 때만 잡는다. 그 밖의 경우에는
		// 없는 셈 치고 뒤쪽 바닥을 클릭하게 둔다 -- 차체가 승강장 셀을 통째로 가리기 때문이다.
		if (AGridTrain* Train = Cast<AGridTrain>(Hit.GetActor()))
		{
			if (Train->CanBoard(GetGridPawn()))
			{
				Pick.Kind = FCursorPick::EKind::Vehicle;
				Pick.Vehicle = Train;
				Pick.Cell = Grid->WorldToCell(Hit.Location);
				Pick.HitLocation = Hit.Location;
				return Pick;
			}
		}

		// 윗면만 잡힌다. 블록의 옆면은 카메라를 향해 서서 그 뒤의 바닥을 가로막고
		// 있으므로, 옆면 히트를 그랩으로 취급하면 그 셀들은 클릭으로 갈 수 없게
		// 된다.
		if (APuzzleBlock* Block = Cast<APuzzleBlock>(Hit.GetActor()))
		{
			if (Hit.ImpactNormal.Z > 0.7)
			{
				Pick.Kind = FCursorPick::EKind::Block;
				Pick.Block = Block;
				Pick.Cell = Grid->WorldToCell(Hit.Location);
				Pick.HitLocation = Hit.Location;
				return Pick;
			}
		}
	}

	// 모든 블록을 무시한 두 번째 패스. 옆면 히트든, 빗나감이든, 배경 히트든 똑같이
	// 돌려서 바닥 판정이 첫 레이가 우연히 무엇에 닿았느냐가 아니라 하나의 규칙으로
	// 나오게 한다.
	TArray<AActor*> BlockActors;
	if (const UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->GetBlockActors(BlockActors);
	}
	Params.AddIgnoredActors(BlockActors);

	FHitResult FloorHit;
	if (!GetWorld()->LineTraceSingleByChannel(FloorHit, WorldOrigin, RayEnd, Grid->TraceChannel, Params))
	{
		return Pick;
	}

	const FIntPoint Cell = Grid->WorldToCell(FloorHit.Location);
	if (!Grid->IsValidCell(Cell))
	{
		return Pick;
	}

	Pick.Kind = FCursorPick::EKind::Floor;
	Pick.Cell = Cell;
	Pick.HitLocation = FloorHit.Location;
	return Pick;
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

		FText Reason;
		if (Train->TryBoard(GridPawn, &Reason))
		{
			ShowFeedback(TEXT("You board the train."), FLinearColor(0.45f, 0.85f, 1.0f));
		}
		else
		{
			ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
		}
		return;
	}

	if (Pick.Kind == FCursorPick::EKind::Block)
	{
		APuzzleBlock* Block = Pick.Block.Get();
		if (!Block)
		{
			return;
		}

		// 블록이 움직일 수 없더라도 일단 쥔다: 이동 없이 떼면 클릭이고, 엘리베이터는
		// 그렇게 탑승한다.
		DraggedBlock = Block;
		GrabPoint = Pick.HitLocation;
		GrabOffset = Pick.HitLocation - Block->GetActorLocation();
		DragAxis = Block->GetWorldMoveAxis();
		bHasRefusedDir = false;
		StepsThisDrag = 0;
		Block->SetHeld(true);
		return;
	}

	if (Pick.Kind == FCursorPick::EKind::Floor)
	{
		// 바로 옆 칸이 막혀 있는데 클릭했다면 폰을 그쪽으로 살짝 부딪히게 한다. 기획의
		// "좁은 곳을 지나가려 할 때 지나갈 수 없다는 걸 보여 주는 연출"이다. 한 칸 떨어진
		// 곳만 대상으로 하는 이유는, 먼 셀을 클릭한 것은 길이 없다는 안내로 충분하기 때문이다.
		const FIntPoint Delta = Pick.Cell - GridPawn->GetCurrentCell();
		const bool bAdjacent = (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y)) == 1;

		if (bAdjacent && !GridPawn->IsMoving() && !Grid->CanPawnEnter(Pick.Cell, GridPawn))
		{
			GridPawn->Bump(Pick.Cell);
		}

		GridPawn->RequestMoveToCell(Pick.Cell);
		return;
	}

	ShowFeedback(TEXT("Click somewhere on the grid."), FLinearColor::Red);
}

void AGridPlayerController::OnReleased()
{
	FinishLeverDrag();
	FinishDrag();
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

	// 휠은 커서가 어느 쪽으로 가든 따라간다. 화면 시계 방향은 쓸어 돈 각도로는 음수이고
	// 월드에서는 양의 yaw이므로 부호를 뒤집는다.
	Lever->SetWheelPreviewAngle(static_cast<float>(-LeverSweptAngle));

	if (bLeverTurnSpent)
	{
		return;		// 드래그 한 번에 90도 회전 한 번: 더 돌리려면 놓았다가 다시 잡는다
	}

	if (FMath::Abs(LeverSweptAngle) < Lever->TurnThresholdDegrees)
	{
		return;
	}

	bLeverTurnSpent = true;

	// 카메라는 yaw 45도 방향을 보고 있어, 양의 월드 yaw가 화면에서는 시계 방향으로 보인다.
	// 따라서 커서를 시계 방향으로 돌리면 양의 회전을 요청하는 것이다.
	const int32 TurnSign = (LeverSweptAngle < 0.0) ? 1 : -1;

	FText Reason;
	if (!Lever->TryTurn(TurnSign, GetGridPawn(), &Reason))
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
		ShowFeedback(TEXT("Drag around the wheel to turn it a quarter."), FLinearColor::White);
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

	// 드래그 한 번에 이동 한 번. 뗌이 클릭이 아니라 드래그로 읽히도록 블록은 계속 쥔
	// 상태로 두되, 플레이어가 놓았다가 다시 잡기 전까지는 더 시도하지 않는다.
	if (Block->bOneStepPerDrag && StepsThisDrag >= 1)
	{
		return;
	}

	if (DragAxis == EPuzzleMoveAxis::None)
	{
		return;		// 움직일 수 없는 블록: 뗌이 클릭으로 등록되도록 쥐고만 있는다
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

	// 아무것도 움직이지 않은 누름은 조각을 클릭한 것이다.
	if (Block->IsA<APuzzleRotatingObstacle>())
	{
		ShowFeedback(TEXT("This turns only when its lever is turned."), FLinearColor::White);
		return;
	}

	if (APuzzleElevatorBlock* Elevator = Cast<APuzzleElevatorBlock>(Block))
	{
		FText Reason;
		if (!Elevator->TryBoard(GetGridPawn(), &Reason))
		{
			ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
			return;
		}

		// 탑승은 문 앞에 섰다는 확인일 뿐이다. 실제로 층을 옮기는 것은 차체 아래의 구조물이며,
		// 구조물 위가 아니면 차체는 그냥 밀 수 있는 상자다.
		UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this);
		APuzzleElevatorDock* Dock = Subsystem ? Subsystem->FindDockUnder(*Elevator) : nullptr;

		if (!Dock)
		{
			ShowFeedback(TEXT("Push the elevator onto its dock first."), FLinearColor(1.0f, 0.65f, 0.05f));
			return;
		}

		if (Dock->TryLaunch(GetGridPawn(), &Reason))
		{
			ShowFeedback(TEXT("You step into the elevator."), FLinearColor(0.45f, 0.85f, 1.0f));
		}
		else
		{
			ShowFeedback(Reason.ToString(), FLinearColor(1.0f, 0.65f, 0.05f));
		}
		return;
	}

	const EPuzzleMoveAxis Axis = Block->GetWorldMoveAxis();
	const TCHAR* AxisText =
		(Axis == EPuzzleMoveAxis::AxisX) ? TEXT("east and west") :
		(Axis == EPuzzleMoveAxis::AxisY) ? TEXT("north and south") :
		(Axis == EPuzzleMoveAxis::Both) ? TEXT("any direction") : TEXT("nowhere");

	ShowFeedback(FString::Printf(TEXT("Drag to push. This block moves %s."), AxisText), FLinearColor::White);
}

FString AGridPlayerController::GetDragStatusText() const
{
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

	if (DraggedLever.IsValid())
	{
		UpdateLeverDrag();
	}

	if (DraggedBlock.IsValid())
	{
		UpdateDrag();
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
	else if (Pick.Kind == FCursorPick::EKind::Block)
	{
		if (const APuzzleBlock* Block = Pick.Block.Get())
		{
			// 레이가 우연히 닿은 셀 하나가 아니라 풋프린트 전체를 그려서, 플레이어가 무엇을
			// 잡으려는 건지 볼 수 있게 한다.
			TArray<FIntPoint> Cells;
			Block->GatherOccupiedCells(Cells);
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
