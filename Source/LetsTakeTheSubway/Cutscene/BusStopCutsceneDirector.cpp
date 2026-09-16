// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cutscene/BusStopCutsceneDirector.h"

#include "LetsTakeTheSubway.h"
#include "Player/GridPawn.h"
#include "Vehicle/VehicleSeat.h"

#include "AnimationRuntime.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "ReferenceSkeleton.h"
#include "TimerManager.h"

namespace
{
	/** 문 하나를 이루는 두 문짝 본. SM_BUS_001의 본 이름이다. */
	void GetDoorBoneNames(EBusDoor WhichDoor, FName& OutA, FName& OutB)
	{
		switch (WhichDoor)
		{
		case EBusDoor::Middle:
			OutA = TEXT("Door_Middle_A");
			OutB = TEXT("Door_Middle_B");
			return;

		case EBusDoor::Front:
		default:
			OutA = TEXT("Door_Front_A");
			OutB = TEXT("Door_Front_B");
			return;
		}
	}

	const TCHAR* DoorName(EBusDoor WhichDoor)
	{
		return WhichDoor == EBusDoor::Middle ? TEXT("middle") : TEXT("front");
	}
}

ABusStopCutsceneDirector::ABusStopCutsceneDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABusStopCutsceneDirector::BeginPlay()
{
	Super::BeginPlay();

	Phase = EPhase::WaitingForPlayer;

	// 버스 그림자 잔상을 막는 Virtual Shadow Map 캐시 끄기는 AGridTestGameMode가 맡는다
	// (ABusStopGameMode가 그 클래스를 상속하므로 이 맵도 함께 적용된다).

	// 첫 프레임부터 검게 가린다. 컨트롤러가 아직 없으면 Tick에서 다시 한다.
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, true);
		}
	}

	if (const ALevelSequenceActor* SeqActor = SequenceActor.Get())
	{
		if (SeqActor->PlaybackSettings.bAutoPlay)
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: %s has Auto Play on; the sequence will start before the cutscene is ready. Turn it off."),
				*GetName(), *SeqActor->GetName());
		}
		if (!SeqActor->PlaybackSettings.bPauseAtEnd)
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: %s has Pause at End off; the bus may snap back when the sequence stops. Turn it on."),
				*GetName(), *SeqActor->GetName());
		}
	}
}

void ABusStopCutsceneDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(OpenLevelTimer);

	if (ULevelSequencePlayer* Player = GetPlayer())
	{
		Player->OnFinished.RemoveDynamic(this, &ABusStopCutsceneDirector::HandleSequenceFinished);
	}

	Super::EndPlay(EndPlayReason);
}

void ABusStopCutsceneDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (Phase)
	{
	case EPhase::WaitingForPlayer:
		if (ResolvePlayer())
		{
			if (Mode == EBusStopCutsceneMode::Alight)
			{
				SeatPawnInBus();
			}
			StartIntro();
		}
		return;

	case EPhase::Intro:
	{
		const ULevelSequencePlayer* Player = GetPlayer();
		if (!Player)
		{
			StartTransfer();
			return;
		}

		if (Player->IsPlaying())
		{
			bSawPlaying = true;
			return;
		}

		// OnPause는 러너 평가가 끝난 뒤로 미뤄질 수 있어서 상태를 직접 본다.
		if ((bSawPlaying && Player->IsPaused()) || bSequenceEnded)
		{
			StartTransfer();
		}
		return;
	}

	case EPhase::Transfer:
	{
		const AGridPawn* Rider = Pawn.Get();
		if (!Rider)
		{
			UE_LOG(LogLTTSGrid, Error, TEXT("%s: the pawn disappeared during the transfer; skipping ahead."), *GetName());
			StartOutro();
			return;
		}

		const bool bDone = Mode == EBusStopCutsceneMode::Board ? Rider->IsRiding() : Rider->IsOnGrid();
		if (bDone)
		{
			UE_LOG(LogLTTSGrid, Display,
				TEXT("%s: %s finished at (%.0f, %.0f, %.0f)."),
				*GetName(), Mode == EBusStopCutsceneMode::Board ? TEXT("boarding") : TEXT("alighting"),
				Rider->GetActorLocation().X, Rider->GetActorLocation().Y, Rider->GetActorLocation().Z);
			StartOutro();
		}
		return;
	}

	case EPhase::Outro:
	case EPhase::Leaving:
	default:
		return;
	}
}

// ---------------------------------------------------------------------------- 단계

bool ABusStopCutsceneDirector::ResolvePlayer()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, true);
	}

	AGridPawn* GridPawn = Cast<AGridPawn>(PC->GetPawn());
	if (!GridPawn)
	{
		// 게임 모드가 틀리면 영영 오지 않는다. 2초 뒤 한 번만 알린다.
		if (!bWarnedWrongPawn && GetGameTimeSinceCreation() > 2.0f)
		{
			UE_LOG(LogLTTSGrid, Error,
				TEXT("%s: the player pawn is %s, not a GridPawn. Set World Settings > GameMode Override to BusStopGameMode."),
				*GetName(), *GetNameSafe(PC->GetPawn()));
			bWarnedWrongPawn = true;
		}
		return false;
	}

	Controller = PC;
	Pawn = GridPawn;

	// 같은 프레임에 폰이 탔는지·내렸는지를 보려고 폰 다음에 틱한다.
	AddTickPrerequisiteActor(GridPawn);
	return true;
}

void ABusStopCutsceneDirector::StartIntro()
{
	LockPlayer(true);

	if (APlayerController* PC = Controller.Get())
	{
		if (ACameraActor* Camera = ViewCamera.Get())
		{
			PC->SetViewTarget(Camera);
		}

		if (PC->PlayerCameraManager)
		{
			if (FadeInSeconds > 0.0f)
			{
				PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FadeInSeconds, FLinearColor::Black, true, false);
			}
			else
			{
				PC->PlayerCameraManager->StopCameraFade();
			}
		}
	}

	Phase = EPhase::Intro;
	bSawPlaying = false;
	bSequenceEnded = false;
	bPauseAtEndInsteadOfMark = false;

	ULevelSequencePlayer* Player = GetPlayer();
	if (!Player)
	{
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: no level sequence to play (Sequence Actor is empty or has no sequence). Transferring without it."),
			*GetName());
		bSequenceEnded = true;
		return;
	}

	Player->OnFinished.AddUniqueDynamic(this, &ABusStopCutsceneDirector::HandleSequenceFinished);

	const ALevelSequenceActor* SeqActor = SequenceActor.Get();
	const ULevelSequence* Sequence = SeqActor ? SeqActor->GetSequence() : nullptr;
	const UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	FString Label = PauseMarkLabel.ToString();

	// 라벨이 없더라도 마크가 하나뿐이면 그것을 쓴다. 시퀀서에서 마크를 새로 찍으면 라벨이 "A" 같은 자동 이름이 되는데,
	// 이름을 안 바꿨다고 승하차 시점을 잃는 것보다 낫다.
	if (MovieScene && (Label.IsEmpty() || MovieScene->FindMarkedFrameByLabel(Label) == INDEX_NONE)
		&& MovieScene->GetMarkedFrames().Num() == 1)
	{
		const FString OnlyLabel = MovieScene->GetMarkedFrames()[0].Label;
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: %s has no marked frame '%s'; using its only marked frame '%s'. Rename it to '%s' in the Sequencer."),
			*GetName(), *GetNameSafe(Sequence), *Label, *OnlyLabel, *Label);
		Label = OnlyLabel;
	}

	if (MovieScene && !Label.IsEmpty() && MovieScene->FindMarkedFrameByLabel(Label) != INDEX_NONE)
	{
		Player->PlayTo(
			FMovieSceneSequencePlaybackParams(Label, EUpdatePositionMethod::Play),
			FMovieSceneSequencePlayToParams());

		UE_LOG(LogLTTSGrid, Display, TEXT("%s: playing %s up to mark '%s'."),
			*GetName(), *GetNameSafe(Sequence), *Label);
	}
	else
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: %s has no marked frame '%s'; the %s happens at the end of the sequence."),
			*GetName(), *GetNameSafe(Sequence), *Label,
			Mode == EBusStopCutsceneMode::Board ? TEXT("boarding") : TEXT("alighting"));

		bPauseAtEndInsteadOfMark = true;
		Player->Play();
	}

	bSawPlaying = Player->IsPlaying();
}

void ABusStopCutsceneDirector::StartTransfer()
{
	Phase = EPhase::Transfer;

	AGridPawn* Rider = Pawn.Get();
	AActor* BusActor = Bus.Get();
	if (!Rider || !BusActor)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no %s; skipping the transfer."),
			*GetName(), Rider ? TEXT("bus (set Bus)") : TEXT("pawn"));
		StartOutro();
		return;
	}

	const FVector PawnLocation = Rider->GetActorLocation();

	if (Mode == EBusStopCutsceneMode::Board)
	{
		FBusFrame Frame;
		if (!ComputeBusFrame(Door, PawnLocation, Frame))
		{
			StartOutro();
			return;
		}

		// 지하철(AGridTrain::TryBoard)과 같은 규칙: 폰의 자리를 진행축 방향으로만 옮겨 문 한가운데와 일직선이 되게
		// 한 뒤, 거기서 차체 중심선의 좌석까지 옆면에 수직으로 들어간다. 높이는 폰의 높이 그대로다.
		const double PawnAlong = LTTSVehicle::AlongAxis(PawnLocation, Frame.Centre, Frame.Axis);
		FVector InLine = PawnLocation + Frame.Axis * (Frame.DoorAlong - PawnAlong);
		InLine.Z = PawnLocation.Z;

		const double SeatHalfExtent = FMath::Max(Frame.HalfLength - SeatEndMargin, 0.0);
		const FVector Seat = LTTSVehicle::SeatAtDoor(Frame.Centre, Frame.Axis, Frame.DoorAlong, SeatHalfExtent, PawnLocation.Z);

		TOptional<FVector> Approach;
		if (!InLine.Equals(PawnLocation, 1.0))
		{
			Approach = InLine;
		}

		Rider->BoardVehicle(BusActor, Seat, nullptr, Approach);

		UE_LOG(LogLTTSGrid, Display,
			TEXT("%s: boarding through the %s door (%.0f cm along the bus): lining up %.0f cm, seat (%.0f, %.0f, %.0f), height kept at %.0f."),
			*GetName(), DoorName(Door), Frame.DoorAlong, Frame.DoorAlong - PawnAlong,
			Seat.X, Seat.Y, Seat.Z, PawnLocation.Z);
		return;
	}

	// Alight
	const AActor* Target = ExitPoint.Get();
	const FVector Hint = Target ? Target->GetActorLocation() : PawnLocation;
	FBusFrame Frame;
	if (!ComputeBusFrame(Door, Hint, Frame))
	{
		StartOutro();
		return;
	}

	// 문 한가운데에서 옆면에 수직으로 나와 문 바로 앞에 선 뒤 하차 목표로 걷는다. 높이는 그대로다.
	const double OutDistance = FMath::Max(Frame.HalfWidth, FMath::Abs(Frame.DoorLateral)) + DoorClearance;
	FVector Through = Frame.Centre + Frame.Axis * Frame.DoorAlong + Frame.Side * OutDistance;
	Through.Z = PawnLocation.Z;

	FVector Exit = Through;
	TOptional<FVector> Via;
	if (Target)
	{
		Exit = FVector(Target->GetActorLocation().X, Target->GetActorLocation().Y, PawnLocation.Z);
		Via = Through;
	}

	Rider->LeaveVehicleTo(Exit, Via);

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: alighting through the %s door (%.0f cm along the bus): door front (%.0f, %.0f, %.0f), exit (%.0f, %.0f, %.0f), height kept at %.0f."),
		*GetName(), DoorName(Door), Frame.DoorAlong,
		Through.X, Through.Y, Through.Z, Exit.X, Exit.Y, Exit.Z, PawnLocation.Z);
}

void ABusStopCutsceneDirector::StartOutro()
{
	ULevelSequencePlayer* Player = GetPlayer();
	if (!Player || bSequenceEnded || bPauseAtEndInsteadOfMark)
	{
		StartFadeOut();
		return;
	}

	Phase = EPhase::Outro;
	Player->Play();

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: resuming the sequence."), *GetName());
}

void ABusStopCutsceneDirector::StartFadeOut()
{
	if (Phase == EPhase::Leaving)
	{
		return;
	}
	Phase = EPhase::Leaving;

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: fading out, then opening %s."),
		*GetName(), *NextLevel.ToSoftObjectPath().ToString());

	const APlayerController* PC = Controller.Get();
	if (FadeOutSeconds <= 0.0f || !PC || !PC->PlayerCameraManager)
	{
		OpenNextLevel();
		return;
	}

	PC->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FadeOutSeconds, FLinearColor::Black, true, true);
	GetWorldTimerManager().SetTimer(OpenLevelTimer, this, &ABusStopCutsceneDirector::OpenNextLevel, FadeOutSeconds, false);
}

void ABusStopCutsceneDirector::OpenNextLevel()
{
	if (NextLevel.IsNull())
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: Next Level is empty; staying on this level."), *GetName());
		return;
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, NextLevel);
}

void ABusStopCutsceneDirector::SkipToNextLevel()
{
	GetWorldTimerManager().ClearTimer(OpenLevelTimer);
	Phase = EPhase::Leaving;
	OpenNextLevel();
}

void ABusStopCutsceneDirector::HandleSequenceFinished()
{
	bSequenceEnded = true;

	// Intro에서 끝났다면(마크 없음) Tick이 승하차를 시작하고, 끝나면 곧바로 페이드 아웃한다.
	if (Phase == EPhase::Outro)
	{
		StartFadeOut();
	}
}

void ABusStopCutsceneDirector::SeatPawnInBus()
{
	AGridPawn* Rider = Pawn.Get();
	AActor* BusActor = Bus.Get();
	if (!Rider || !BusActor)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: cannot seat the pawn; %s is missing."),
			*GetName(), Rider ? TEXT("Bus") : TEXT("the pawn"));
		return;
	}

	const FVector PawnLocation = Rider->GetActorLocation();
	const AActor* Target = ExitPoint.Get();
	const FVector Hint = Target ? Target->GetActorLocation() : PawnLocation;

	FBusFrame Frame;
	if (!ComputeBusFrame(Door, Hint, Frame))
	{
		return;
	}

	// 내릴 문 한가운데를 마주 보는 좌석에서 출발한다. 그래야 내릴 때 진행축으로 옮겨 서지 않고 곧장 나온다.
	const double SeatHalfExtent = FMath::Max(Frame.HalfLength - SeatEndMargin, 0.0);
	const FVector Seat = LTTSVehicle::SeatAtDoor(Frame.Centre, Frame.Axis, Frame.DoorAlong, SeatHalfExtent, PawnLocation.Z);
	Rider->SitInVehicle(BusActor, Seat);
}

void ABusStopCutsceneDirector::LockPlayer(bool bLock)
{
	APlayerController* PC = Controller.Get();
	if (!PC)
	{
		return;
	}

	// 이동·회전 입력과 HUD를 잠근다. 폰은 숨기지 않는다 -- 타고 내리는 모습이 이 장면의 전부다.
	PC->SetCinematicMode(bLock, /*bHidePlayer*/ false, /*bAffectsHUD*/ true, /*bAffectsMovement*/ true, /*bAffectsTurning*/ true);
	PC->bShowMouseCursor = !bLock;
}

// ---------------------------------------------------------------------------- 버스 치수

USkeletalMeshComponent* ABusStopCutsceneDirector::FindBusMesh() const
{
	const AActor* BusActor = Bus.Get();
	return BusActor ? BusActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

ULevelSequencePlayer* ABusStopCutsceneDirector::GetPlayer() const
{
	const ALevelSequenceActor* SeqActor = SequenceActor.Get();
	if (!SeqActor || !SeqActor->GetSequence())
	{
		return nullptr;
	}
	return SeqActor->GetSequencePlayer();
}

bool ABusStopCutsceneDirector::ComputeBusFrame(EBusDoor WhichDoor, const FVector& SideHint, FBusFrame& Out) const
{
	const USkeletalMeshComponent* Mesh = FindBusMesh();
	const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
	if (!Asset)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: Bus has no skeletal mesh; cannot find the doors."), *GetName());
		return false;
	}

	const FTransform MeshTransform = Mesh->GetComponentTransform();
	const FVector Scale = MeshTransform.GetScale3D().GetAbs();
	const FBoxSphereBounds Bounds = Asset->GetBounds();

	// SM_BUS_001은 로컬 +Y가 진행축, X가 폭이다. 중심은 바운드 중심을 바닥으로 내린 점.
	Out.Centre = MeshTransform.TransformPosition(FVector(Bounds.Origin.X, Bounds.Origin.Y, 0.0));
	const FVector LocalY = MeshTransform.GetUnitAxis(EAxis::Y);
	Out.Axis = FVector(LocalY.X, LocalY.Y, 0.0).GetSafeNormal();
	if (Out.Axis.IsNearlyZero())
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: the bus is tilted too far to read its travel axis."), *GetName());
		return false;
	}
	Out.HalfLength = Bounds.BoxExtent.Y * Scale.Y;
	Out.HalfWidth = Bounds.BoxExtent.X * Scale.X;

	FName BoneA;
	FName BoneB;
	GetDoorBoneNames(WhichDoor, BoneA, BoneB);

	const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
	FVector Sum = FVector::ZeroVector;
	int32 Found = 0;
	for (const FName& BoneName : { BoneA, BoneB })
	{
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			continue;
		}

		const FTransform ComponentSpace = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex);
		Sum += MeshTransform.TransformPosition(ComponentSpace.GetLocation());
		++Found;
	}

	Out.bDoorFound = Found > 0;
	if (Out.bDoorFound)
	{
		Out.DoorCentre = Sum / Found;
	}
	else
	{
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: %s has no bones %s / %s; using the middle of the bus instead."),
			*GetName(), *Asset->GetName(), *BoneA.ToString(), *BoneB.ToString());
		Out.DoorCentre = Out.Centre;
	}

	Out.DoorAlong = LTTSVehicle::AlongAxis(Out.DoorCentre, Out.Centre, Out.Axis);

	// 문이 달린 옆면. 문 본이 중심선에서 떨어져 있으면 그쪽, 아니면 힌트(정류장에 선 폰이나 하차 목표) 쪽이다.
	const FVector Perp = FVector::CrossProduct(FVector::UpVector, Out.Axis).GetSafeNormal();
	const FVector DoorDelta(Out.DoorCentre.X - Out.Centre.X, Out.DoorCentre.Y - Out.Centre.Y, 0.0);
	const FVector HintDelta(SideHint.X - Out.Centre.X, SideHint.Y - Out.Centre.Y, 0.0);
	const double DoorDot = FVector::DotProduct(DoorDelta, Perp);
	const double SideSign = FMath::Abs(DoorDot) > 10.0
		? FMath::Sign(DoorDot)
		: (FVector::DotProduct(HintDelta, Perp) >= 0.0 ? 1.0 : -1.0);

	Out.Side = Perp * SideSign;
	Out.DoorLateral = FVector::DotProduct(DoorDelta, Out.Side);
	return true;
}

void ABusStopCutsceneDirector::MeasureDoor()
{
	for (EBusDoor WhichDoor : { EBusDoor::Front, EBusDoor::Middle })
	{
		FBusFrame Frame;
		if (!ComputeBusFrame(WhichDoor, GetActorLocation(), Frame))
		{
			return;
		}

		UE_LOG(LogLTTSGrid, Display,
			TEXT("%s: %s door centre (%.0f, %.0f, %.0f) = %.1f cm along the bus, %.1f cm to the side (half length %.0f, half width %.0f)%s."),
			*GetName(), DoorName(WhichDoor),
			Frame.DoorCentre.X, Frame.DoorCentre.Y, Frame.DoorCentre.Z,
			Frame.DoorAlong, Frame.DoorLateral, Frame.HalfLength, Frame.HalfWidth,
			Frame.bDoorFound ? TEXT("") : TEXT(" [bones missing]"));
	}
}

// ---------------------------------------------------------------------------- 콘솔

namespace
{
	void BusCutsceneCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}

		const FString Verb = Args.Num() > 0 ? Args[0] : TEXT("Skip");
		if (!Verb.Equals(TEXT("Skip"), ESearchCase::IgnoreCase))
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.BusCutscene: unknown argument '%s'. Usage: ltts.BusCutscene Skip"), *Verb);
			return;
		}

		for (TActorIterator<ABusStopCutsceneDirector> It(World); It; ++It)
		{
			UE_LOG(LogLTTSGrid, Display, TEXT("ltts.BusCutscene: skipping %s."), *It->GetName());
			It->SkipToNextLevel();
			return;
		}

		UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.BusCutscene: no BusStopCutsceneDirector in this level."));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GBusCutsceneCommand(
	TEXT("ltts.BusCutscene"),
	TEXT("Skip the bus stop cutscene and open its Next Level right away: ltts.BusCutscene Skip"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BusCutsceneCommand));
