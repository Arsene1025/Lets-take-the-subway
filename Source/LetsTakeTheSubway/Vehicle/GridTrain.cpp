// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/GridTrain.h"

#include "LetsTakeTheSubway.h"
#include "Art/ArtMaterialUtil.h"
#include "Grid/GridActor.h"
#include "Curves/CurveFloat.h"
#include "Grid/GridTypes.h"
#include "NPC/GridNPCSpawner.h"
#include "Player/GridPawn.h"
#include "Player/GridPlayerController.h"
#include "Vehicle/VehicleSeat.h"
#include "Puzzle/PuzzleSubsystem.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 문 표시 슬랩의 두께와 높이 비율. */
	constexpr double TrainDoorThickness = 24.0;
	constexpr double TrainDoorHeightRatio = 0.7;
	constexpr int32 NumTrainDoors = 2;
}

AGridTrain::AGridTrain()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// 아트 차체. 스켈레탈 메시는 블루프린트가 정하고, 여기서는 문과 바퀴를 움직일 준비만 한다.
	// 판정에는 끼지 않는다: 커서는 BodyMesh 프록시가, 폰의 위치는 그리드가 정한다.
	ArtMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArtMesh"));
	ArtMesh->SetupAttachment(SceneRoot);
	ArtMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArtMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArtMesh->SetGenerateOverlapEvents(false);
	ArtMesh->SetCanEverAffectNavigation(false);
	ArtMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode, false);

	// 화면 밖에서도 포즈를 갱신한다. 카메라가 열차를 벗어난 사이 문이 열리고 닫혀도, 다시
	// 보았을 때 문이 이전 자세로 멈춰 있지 않게 한다. 짚는 애니메이션이라 갱신 빈도를 낮추는
	// 최적화도 끈다 -- 프레임을 건너뛰면 단계와 보이는 문이 어긋난다.
	ArtMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	ArtMesh->bEnableUpdateRateOptimizations = false;

#if WITH_EDITORONLY_DATA
	// 뷰포트에서 끌리지 않게 잠근다(Lock Actor Movement). 아트가 역 구조물을 박스 선택으로
	// 옮길 때 열차가 같이 끌려가 좌표가 어긋난 사고가 있었다(2026-09-14 노선 연장).
	// 옮겨야 하면 액터 우클릭 > Transform > Lock Actor Movement를 끄거나 디테일 패널에 값을 넣는다.
	bLockLocation = true;
#endif

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_Movable.MI_GreyBox_Movable"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ClosedFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_B2.MI_GreyBox_B2"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpenFinder(
		TEXT("/Game/Art/GreyBox/Materials/MI_GreyBox_F0.MI_GreyBox_F0"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);

	// 퍼즐 조각과 같은 규칙이다: Visibility만 막아 커서에는 잡히고, 물리와 폰은 관여하지
	// 않는다. 폰에는 콜리전이 아예 없다.
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BodyMesh->SetGenerateOverlapEvents(false);

	if (CubeFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeFinder.Object);
	}
	if (BodyFinder.Succeeded())
	{
		BodyMesh->SetMaterial(0, BodyFinder.Object);
	}

	if (ClosedFinder.Succeeded())
	{
		DoorClosedMaterial = ClosedFinder.Object;
	}
	if (OpenFinder.Succeeded())
	{
		DoorOpenMaterial = OpenFinder.Object;
	}

	DoorMeshes.Reserve(NumTrainDoors);
	for (int32 Index = 0; Index < NumTrainDoors; ++Index)
	{
		UStaticMeshComponent* Door = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("DoorMesh%d"), Index));
		Door->SetupAttachment(SceneRoot);
		Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Door->SetCollisionResponseToAllChannels(ECR_Ignore);
		Door->SetGenerateOverlapEvents(false);

		if (CubeFinder.Succeeded())
		{
			Door->SetStaticMesh(CubeFinder.Object);
		}
		if (ClosedFinder.Succeeded())
		{
			Door->SetMaterial(0, ClosedFinder.Object);
		}

		DoorMeshes.Add(Door);
	}

	// 열차는 선로 위를 지나다닌다. 그리드가 이것을 바닥으로 구우면 지나간 자리가 통째로
	// 걸을 수 있는 셀이 된다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

// ---------------------------------------------------------------------------- 비주얼

// ---------------------------------------------------------------------------- 문과 탑승 셀

void AGridTrain::GetDoorOffsets(TArray<float>& OutOffsets) const
{
	OutOffsets.Reset();

	if (DoorOffsetsLocal.Num() > 0)
	{
		OutOffsets = DoorOffsetsLocal;
		return;
	}

	// 저작 값이 없으면 그레이박스 프록시 문 두 짝이 곧 문이다. RefreshVisual이 그 자리에
	// 슬랩을 그리므로 보이는 것과 탈 수 있는 자리가 어긋나지 않는다.
	const double DoorSpacing = BodyLength * 0.25;
	OutOffsets.Add(static_cast<float>(-DoorSpacing));
	OutOffsets.Add(static_cast<float>(DoorSpacing));
}

void AGridTrain::ComputeBoardingCells(int32 StopIndex, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();

	if (!Grid || !Stops.IsValidIndex(StopIndex))
	{
		return;
	}

	TArray<float> Offsets;
	GetDoorOffsets(Offsets);
	if (Offsets.IsEmpty())
	{
		return;
	}

	// 정차 위치에서 잰다. 열차가 지금 어디 있든 그 역의 문 앞은 같은 자리다.
	const FVector StopLocation = GetStopLocation(StopIndex);
	const FQuat Rotation = GetActorQuat();
	const double CellSize = Grid->CellSize;

	// 문은 승강장 쪽(로컬 +Y) 면에 있다. 차체 반폭 바깥으로 한 칸 나가야 승강장 셀이다.
	const double SideOffset = BodyWidth * 0.5 + CellSize * 0.75;

	// 선로 높이. BeginPlay 전(에디터 미리보기)에는 TrackZ가 아직 없으므로 배치 Z를 쓴다.
	const double RailZ = HasActorBegunPlay() ? TrackZ : GetActorLocation().Z;
	const int32 SearchCells = FMath::Max(1, BoardingSearchCells);

	for (const float Offset : Offsets)
	{
		// 문 폭에 걸치는 셀을 모두 넣는다. 문 하나가 셀 두 칸에 걸쳐 있으면 어느 쪽에
		// 서 있든 탈 수 있어야 한다.
		const double Half = FMath::Max(DoorWidth, 1.0f) * 0.5;
		const int32 Samples = FMath::Max(2, FMath::CeilToInt((Half * 2.0) / (CellSize * 0.5)) + 1);

		for (int32 Sample = 0; Sample < Samples; ++Sample)
		{
			const double Along = Offset - Half + (Half * 2.0) * Sample / (Samples - 1);

			// 승강장이 차체에서 몇 칸 떨어져 있을 수도 있다(턱이나 틈). 선로 높이 근처의
			// 걸을 수 있는 셀이 나올 때까지 바깥으로 BoardingSearchCells칸까지 본다.
			for (int32 Step = 0; Step < SearchCells; ++Step)
			{
				const FVector Local(Along, SideOffset + Step * CellSize, 0.0);
				const FIntPoint Cell = Grid->WorldToCell(StopLocation + Rotation.RotateVector(Local));

				if (!Grid->IsValidCell(Cell) || !Grid->IsCellWalkableStatic(Cell))
				{
					continue;
				}

				// 걸을 수는 있어도 승강장보다 한참 낮은 턱이면 건너뛰고 더 바깥을 본다.
				if (BoardingFloorTolerance > 0.0f
					&& FMath::Abs(Grid->CellToWorld(Cell).Z - RailZ) > BoardingFloorTolerance)
				{
					continue;
				}

				OutCells.AddUnique(Cell);
				break;
			}
		}
	}
}

void AGridTrain::GetBoardingCells(int32 StopIndex, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();

	if (!Stops.IsValidIndex(StopIndex))
	{
		return;
	}

	// 손으로 적은 목록이 있으면 그쪽이 이긴다. 문 위치로는 표현할 수 없는 배치
	// (그레이박스 테스트 맵)가 이미 있고, 그것을 계산 값으로 덮어써서는 안 된다.
	if (Stops[StopIndex].BoardingCells.Num() > 0)
	{
		OutCells = Stops[StopIndex].BoardingCells;
		return;
	}

	ComputeBoardingCells(StopIndex, OutCells);
}

void AGridTrain::GetApproachCells(TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();

	TArray<FIntPoint> Cells;
	for (int32 Index = 0; Index < Stops.Num(); ++Index)
	{
		GetBoardingCells(Index, Cells);
		for (const FIntPoint& Cell : Cells)
		{
			OutCells.AddUnique(Cell);
		}
	}
}

void AGridTrain::GetDoorFrontCells(TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();

	if (!Grid)
	{
		return;
	}

	TArray<float> Offsets;
	GetDoorOffsets(Offsets);

	const FVector Axis = GetActorForwardVector();
	const double Reach = DoorWidth * 0.5 + Grid->CellSize * 0.5;

	TArray<FIntPoint> Cells;
	for (int32 StopIndex = 0; StopIndex < Stops.Num(); ++StopIndex)
	{
		GetBoardingCells(StopIndex, Cells);

		// 셀이 문 한가운데에서 진행축으로 얼마나 떨어졌는지는 그 역에 선 열차 기준으로 잰다.
		const FVector StopLocation = GetStopLocation(StopIndex);

		for (const float Offset : Offsets)
		{
			FIntPoint Best(-1, -1);
			double BestDistance = TNumericLimits<double>::Max();

			for (const FIntPoint& Cell : Cells)
			{
				if (!Grid->IsValidCell(Cell))
				{
					continue;
				}

				const double Distance = FMath::Abs(
					LTTSVehicle::AlongAxis(Grid->CellToWorld(Cell), StopLocation, Axis) - Offset);
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					Best = Cell;
				}
			}

			if (Grid->IsValidCell(Best) && BestDistance <= Reach)
			{
				OutCells.AddUnique(Best);
			}
		}
	}
}

// ---------------------------------------------------------------------------- 가감속

float AGridTrain::GetSpeedFactor(float Alpha) const
{
	const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);

	float Factor = 1.0f;

	if (SpeedCurve)
	{
		Factor = SpeedCurve->GetFloatValue(Clamped);
	}
	else
	{
		// 기본 곡선: 앞쪽 AccelFraction 동안 올리고 뒤쪽 DecelFraction 동안 내린다.
		// 둘 중 작은 쪽이 이기므로 구간이 짧아 두 구간이 겹쳐도 값이 1을 넘지 않는다.
		const float Rise = (AccelFraction > KINDA_SMALL_NUMBER) ? (Clamped / AccelFraction) : 1.0f;
		const float Fall = (DecelFraction > KINDA_SMALL_NUMBER) ? ((1.0f - Clamped) / DecelFraction) : 1.0f;
		const float Linear = FMath::Clamp(FMath::Min(Rise, Fall), 0.0f, 1.0f);

		// 지수를 씌워 시작과 끝의 꺾임을 부드럽게 한다. PuzzleBlock의 회전 이징과 같은 생각이다.
		Factor = FMath::Pow(Linear, EaseExponent);
	}

	// 0이 되면 영영 도착하지 못한다.
	return FMath::Clamp(Factor, MinSpeedFactor, 1.0f);
}

double AGridTrain::GetSeatHalfExtent() const
{
	// 차체 끝에서 1 m 안쪽까지만. 좌석이 차체 밖으로 나가면 폰이 허공에 실린다.
	return FMath::Max(BodyLength * 0.5 - 100.0, 0.0);
}

// ---------------------------------------------------------------------------- 아트

void AGridTrain::ApplyArtFallbackMaterial()
{
	if (!ArtFallbackMaterial)
	{
		return;
	}

	// 블루프린트가 붙인 메시만 손본다. 코드가 만든 프록시(차체와 문 슬랩)는 자기
	// 그레이박스 머티리얼을 갖고 있고, 문 슬랩은 열림·닫힘에 따라 색을 바꾼다.
	//
	// 스태틱과 스켈레탈을 가리지 않는다. 아트 스켈레탈 차체(ArtMesh)도 임포트 직후라 슬롯이
	// 엔진 기본 머티리얼이다.
	TArray<UMeshComponent*> Meshes;
	GetComponents(Meshes);

	for (UMeshComponent* Mesh : Meshes)
	{
		if (!Mesh || Mesh == BodyMesh || DoorMeshes.Contains(Cast<UStaticMeshComponent>(Mesh)))
		{
			continue;
		}

		LTTSArt::ReplaceDefaultMaterials(*Mesh, ArtFallbackMaterial);
	}
}

void AGridTrain::RefreshVisual()
{
	if (BodyMesh)
	{
		// 아트 메시를 쓰는 블루프린트에서는 프록시 상자를 숨기되 콜리전은 남긴다. 눈에
		// 보이는 것은 아트이고, 커서가 잡는 것은 언제나 이 상자다.
		BodyMesh->SetVisibility(bShowProxyBody);

		// 액터 원점이 객차 바닥이다. 폰의 좌석 높이를 그 위로 잡을 수 있어 계산이 단순해진다.
		BodyMesh->SetRelativeLocation(FVector(0.0, 0.0, BodyHeight * 0.5));
		BodyMesh->SetRelativeScale3D(FVector(BodyLength / 100.0, BodyWidth / 100.0, BodyHeight / 100.0));
	}

	// 문은 승강장 쪽(+Y) 면에 둘. 열차는 X축을 따라 달린다는 전제다.
	//
	// 자리는 DoorOffsetsLocal을 따른다. 프록시 슬랩과 탑승 셀이 같은 값에서 나와야
	// 그레이박스 맵에서 보이는 문과 실제로 탈 수 있는 자리가 어긋나지 않는다.
	TArray<float> DoorOffsets;
	GetDoorOffsets(DoorOffsets);
	for (int32 Index = 0; Index < DoorMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Door = DoorMeshes[Index];
		if (!Door)
		{
			continue;
		}

		Door->SetVisibility(bShowProxyBody);

		const double OffsetX = DoorOffsets.IsValidIndex(Index) ? DoorOffsets[Index] : 0.0;
		Door->SetRelativeLocation(FVector(
			OffsetX,
			BodyWidth * 0.5 + TrainDoorThickness * 0.5,
			BodyHeight * TrainDoorHeightRatio * 0.5));
		Door->SetRelativeScale3D(FVector(
			1.2,
			TrainDoorThickness / 100.0,
			BodyHeight * TrainDoorHeightRatio / 100.0));
	}
}

void AGridTrain::RefreshDoorLook()
{
	const bool bOpen = AreDoorsOpen();
	if (bOpen == bDoorsLookOpen)
	{
		return;
	}

	bDoorsLookOpen = bOpen;

	if (UMaterialInterface* Material = bOpen ? DoorOpenMaterial : DoorClosedMaterial)
	{
		for (UStaticMeshComponent* Door : DoorMeshes)
		{
			if (Door)
			{
				Door->SetMaterial(0, Material);
			}
		}
	}
}

// ---------------------------------------------------------------------------- 연출 훅

void AGridTrain::AnimateDoorsOpening_Implementation(float Alpha)
{
	// 보이는 문을 단계 진행도에 맞출 뿐이다. 문이 열리는 동안 타고 내리지 못하게 막는 것은
	// 여전히 DoorsOpening 단계이지 이 함수가 아니다.
	ScrubDoorAnimation(DoorOpenAnimation, Alpha, false);
}

void AGridTrain::AnimateDoorsClosing_Implementation(float Alpha)
{
	// 닫힘 애니메이션이 없으면 열림을 거꾸로 짚는다. Alpha 0(열림)이 열림 애니메이션의 끝이다.
	//
	// 애니메이션을 먼저 받아 두어야 한다. 한 호출식의 인자로 함께 넘기면 bReverse를 읽는 쪽이
	// GetDoorClosingAnimation보다 먼저 평가될 수 있어(MSVC) 역재생이 정방향으로 돈다.
	bool bReverse = false;
	UAnimSequenceBase* Closing = GetDoorClosingAnimation(bReverse);
	ScrubDoorAnimation(Closing, Alpha, bReverse);
}

bool AGridTrain::HasArtMesh() const
{
	return ArtMesh && ArtMesh->GetSkeletalMeshAsset();
}

UAnimSequenceBase* AGridTrain::GetDoorClosingAnimation(bool& bOutReverse) const
{
	bOutReverse = false;

	if (DoorCloseAnimation)
	{
		return DoorCloseAnimation;
	}

	if (DoorOpenAnimation)
	{
		bOutReverse = true;
		return DoorOpenAnimation;
	}

	return nullptr;
}

float AGridTrain::GetAnimationDuration(const UAnimSequenceBase* Animation)
{
	if (!Animation)
	{
		return 0.0f;
	}

	// GetPlayLength는 RateScale을 모른다. 애셋에서 속도를 바꿔 두었으면 실제 걸리는 시간이 다르다.
	return Animation->GetPlayLength() / FMath::Max(FMath::Abs(Animation->RateScale), KINDA_SMALL_NUMBER);
}

void AGridTrain::ScrubDoorAnimation(UAnimSequenceBase* Animation, float Alpha, bool bReverse)
{
	if (!Animation || !HasArtMesh() || !ArtMesh->GetSingleNodeInstance())
	{
		return;
	}

	// SetAnimation은 재생 위치를 0으로 되돌린다. 애니메이션이 바뀔 때만 부른다.
	if (CurrentArtAnimation != Animation)
	{
		ArtMesh->SetAnimation(Animation);
		CurrentArtAnimation = Animation;
	}
	else if (ArtMesh->IsPlaying())
	{
		ArtMesh->Stop();
	}

	// 재생하지 않고 위치만 짚는다. 멈춰 있어도 컴포넌트 틱이 짚은 위치의 포즈를 평가한다.
	const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float Position = (bReverse ? 1.0f - Clamped : Clamped) * Animation->GetPlayLength();
	ArtMesh->SetPosition(Position, false);
}

void AGridTrain::StartWheelAnimation()
{
	if (!WheelAnimation || !HasArtMesh() || !ArtMesh->GetSingleNodeInstance())
	{
		return;
	}

	// 문과 바퀴는 같은 스켈레톤의 단일 애니메이션 슬롯을 나눠 쓴다. 문은 서 있을 때만, 바퀴는
	// 달릴 때만 움직이므로 겹치지 않는다. 다음 역에 서면 문 애니메이션이 슬롯을 다시 가져간다.
	if (CurrentArtAnimation != WheelAnimation)
	{
		ArtMesh->SetAnimation(WheelAnimation);
		CurrentArtAnimation = WheelAnimation;
	}

	ArtMesh->SetPlayRate(1.0f);
	ArtMesh->Play(true);
}

void AGridTrain::UpdateWheelPlayRate(double CurrentSpeed)
{
	if (WheelAnimSpeedAtRate1 <= 0.0f || !WheelAnimation || !HasArtMesh() || CurrentArtAnimation != WheelAnimation)
	{
		return;
	}

	ArtMesh->SetPlayRate(static_cast<float>(CurrentSpeed / WheelAnimSpeedAtRate1));
}

void AGridTrain::LogDoorBoneOffsets()
{
	if (!HasArtMesh())
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: ArtMesh has no skeletal mesh; nothing to measure."), *GetName());
		return;
	}

	// 본 이름에서 L/R(한 문의 두 짝)과 front/back(차체의 두 면)을 떼면 문 하나가 남는다.
	// 예: Subway1Door_L1_front, Subway1Door_R1_back -> Subway1Door_1.
	const FReferenceSkeleton& RefSkeleton = ArtMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	const FVector ActorLocation = GetActorLocation();
	const FQuat ActorRotation = GetActorQuat();

	TMap<FString, TArray<double>> Doors;
	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		FString Key = BoneName.ToString();
		if (!Key.Contains(TEXT("Door"), ESearchCase::CaseSensitive))
		{
			continue;
		}

		Key.ReplaceInline(TEXT("_front"), TEXT(""), ESearchCase::CaseSensitive);
		Key.ReplaceInline(TEXT("_back"), TEXT(""), ESearchCase::CaseSensitive);
		Key.ReplaceInline(TEXT("_L"), TEXT("_"), ESearchCase::CaseSensitive);
		Key.ReplaceInline(TEXT("_R"), TEXT("_"), ESearchCase::CaseSensitive);

		// 탑승 셀 계산과 같은 방식으로 잰다: 액터 회전만 되돌리고 스케일은 보지 않는다.
		const FVector World = ArtMesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
		const FVector Local = ActorRotation.UnrotateVector(World - ActorLocation);
		Doors.FindOrAdd(Key).Add(Local.X);
	}

	if (Doors.IsEmpty())
	{
		UE_LOG(LogLTTSGrid, Warning, TEXT("%s: no bone names containing 'Door' in %s."),
			*GetName(), *GetNameSafe(ArtMesh->GetSkeletalMeshAsset()));
		return;
	}

	TArray<float> Offsets;
	GetDoorOffsets(Offsets);

	Doors.KeySort([](const FString& A, const FString& B) { return A < B; });
	for (const TPair<FString, TArray<double>>& Door : Doors)
	{
		double Sum = 0.0;
		for (const double X : Door.Value)
		{
			Sum += X;
		}
		const double DoorCentre = Sum / Door.Value.Num();

		float Nearest = 0.0f;
		double BestDistance = TNumericLimits<double>::Max();
		for (const float Offset : Offsets)
		{
			if (FMath::Abs(DoorCentre - Offset) < BestDistance)
			{
				BestDistance = FMath::Abs(DoorCentre - Offset);
				Nearest = Offset;
			}
		}

		const bool bOff = BestDistance > 50.0;
		UE_LOG(LogLTTSGrid, Display,
			TEXT("%s: door '%s' (%d bones) centre X %.1f cm, nearest DoorOffsetsLocal %.1f, diff %.1f cm%s"),
			*GetName(), *Door.Key, Door.Value.Num(), DoorCentre, Nearest, BestDistance,
			bOff ? TEXT(" -- CHECK: boarding cells may not face this door.") : TEXT("."));
	}
}

void AGridTrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisual();
	ApplyArtFallbackMaterial();
}

// ---------------------------------------------------------------------------- 생명주기

void AGridTrain::BeginPlay()
{
	Super::BeginPlay();

	Grid = AGridActor::FindGrid(GetWorld());
	if (!Grid)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no AGridActor in the level; the train is idle."), *GetName());
		return;
	}

	// 높이는 배치된 그대로 쓴다. 선로 셀은 걸을 수 없어 그리드에 바닥 높이가 없으므로
	// 셀에서 Z를 얻을 수 없다.
	TrackZ = GetActorLocation().Z;

	// 아트 문. 단계 시간을 애니메이션 길이에 맞추는 일은 아래 설정 로그보다 앞서야 맞춘 값이 찍힌다.
	if (HasArtMesh())
	{
		// 블루프린트가 애니메이션 모드를 바꿔 두었거나 인스턴스가 아직 없으면 단일 노드로 되돌린다.
		if (!ArtMesh->GetSingleNodeInstance())
		{
			ArtMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			ArtMesh->InitializeAnimScriptInstance(true);
		}

		// 열차 틱에서 짚은 위치를 같은 프레임에 포즈로 반영한다.
		ArtMesh->AddTickPrerequisiteActor(this);

		if (bMatchDoorTimingToAnimation)
		{
			if (DoorOpenAnimation)
			{
				DoorOpeningSeconds = GetAnimationDuration(DoorOpenAnimation);
			}

			bool bReverse = false;
			if (const UAnimSequenceBase* Closing = GetDoorClosingAnimation(bReverse))
			{
				DoorCloseSeconds = GetAnimationDuration(Closing);
			}
		}

		if (DoorOpenAnimation)
		{
			// 닫힌 모습으로 시작한다.
			ScrubDoorAnimation(DoorOpenAnimation, 0.0f, false);
		}
		else
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: ArtMesh has a skeletal mesh but DoorOpenAnimation is not set; the doors will not move."),
				*GetName());
		}
	}

	if (Stops.Num() < 2)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: needs at least two stops to run; it will stand still."), *GetName());
		RefreshVisual();
		return;
	}

	for (int32 Index = 0; Index < Stops.Num(); ++Index)
	{
		const FGridTrainStop& Stop = Stops[Index];
		if (!Grid->IsValidCell(Stop.StopCell))
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: stop %d '%s' cell (%d,%d) is outside the grid."),
				*GetName(), Index, *Stop.StopName.ToString(), Stop.StopCell.X, Stop.StopCell.Y);
		}

		// 탑승 셀은 저작 값이 있으면 그것, 없으면 문 위치에서 계산한 것이다. 여기서 한 번
		// 찍어 두면 아트 문이 승강장과 어긋났을 때 로그만 보고 알 수 있다.
		TArray<FIntPoint> Boarding;
		GetBoardingCells(Index, Boarding);

		if (Boarding.IsEmpty())
		{
			UE_LOG(LogLTTSGrid, Warning,
				TEXT("%s: stop %d '%s' has no boarding cells; the player cannot board there."),
				*GetName(), Index, *Stop.StopName.ToString());
		}
		else
		{
			FString CellList;
			for (const FIntPoint& Cell : Boarding)
			{
				CellList += FString::Printf(TEXT("%s(%d,%d)"), CellList.IsEmpty() ? TEXT("") : TEXT(" "), Cell.X, Cell.Y);
			}

			UE_LOG(LogLTTSGrid, Display,
				TEXT("%s: stop %d '%s': %d boarding cell(s) %s -- %s."),
				*GetName(), Index, *Stop.StopName.ToString(), Boarding.Num(),
				Stop.BoardingCells.Num() > 0 ? TEXT("authored") : TEXT("from door offsets"),
				*CellList);
		}
	}

	CurrentStop = 0;
	SetActorLocation(GetStopLocation(0));
	RefreshVisual();

	// 첫 역에는 조금 뜸을 들이고 들어온다. 레벨이 시작하자마자 문이 열려 있으면 열차가
	// 원래 거기 서 있었던 것처럼 보인다.
	Phase = ETrainPhase::Idle;
	PhaseTimer = StartDelaySeconds;

	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->RegisterVehicle(this);
	}

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %d stops, %.0f cm/s, doors opening %.1f s / open %.1f s / closing %.1f s, loop %s."),
		*GetName(), Stops.Num(), Speed,
		DoorOpeningSeconds, DoorOpenSeconds, DoorCloseSeconds,
		bLoop ? TEXT("on") : TEXT("off"));
}

void AGridTrain::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPuzzleSubsystem* Subsystem = UPuzzleSubsystem::Get(this))
	{
		Subsystem->UnregisterVehicle(this);
	}

	Rider.Reset();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------- 정차역

FVector AGridTrain::GetStopLocation(int32 StopIndex) const
{
	if (!Grid || !Stops.IsValidIndex(StopIndex))
	{
		return GetActorLocation();
	}

	const FVector Origin = Grid->GetGridOrigin();
	const FIntPoint Cell = Stops[StopIndex].StopCell;

	// 셀 중심의 XY만 쓰고 높이는 선로 높이를 유지한다. CellToWorld는 걸을 수 있는 바닥의
	// Z를 돌려주는데, 선로 셀에는 그런 바닥이 없다.
	return FVector(
		Origin.X + (Cell.X + 0.5) * Grid->CellSize,
		Origin.Y + (Cell.Y + 0.5) * Grid->CellSize,
		TrackZ);
}

int32 AGridTrain::GetNextStopIndex() const
{
	const int32 Next = CurrentStop + 1;

	if (Next < Stops.Num())
	{
		return Next;
	}

	return bLoop ? 0 : INDEX_NONE;
}

// ---------------------------------------------------------------------------- 상태 전이

void AGridTrain::EnterMoving()
{
	// 닫힘 단계를 다 거치지 않고 떠나는 경우(콘솔 TrainArrive)에도 문은 닫힌 모습이어야 한다.
	AnimateDoorsClosing(1.0f);

	const int32 Next = GetNextStopIndex();
	if (Next == INDEX_NONE)
	{
		// 마지막 역이고 순환하지 않는다. 문을 닫은 채 여기 선다.
		Phase = ETrainPhase::Idle;
		PhaseTimer = 0.0f;
		return;
	}

	CurrentStop = Next;
	Phase = ETrainPhase::Moving;
	StartWheelAnimation();

	// 구간을 통째로 기억한다. 가감속은 "지금 몇 퍼센트를 왔는가"를 알아야 하는데,
	// 매 프레임 현재 위치에서 목표까지의 남은 거리만 보면 그 답이 나오지 않는다.
	MoveFrom = GetActorLocation();
	MoveTo = GetStopLocation(CurrentStop);
	MoveLength = FVector::Distance(MoveFrom, MoveTo);
	MoveDistance = 0.0;

	UE_LOG(LogLTTSGrid, Verbose,
		TEXT("%s: departing for stop %d '%s'."),
		*GetName(), CurrentStop, *Stops[CurrentStop].StopName.ToString());
}

float AGridTrain::GetPhaseAlpha(float Duration) const
{
	// 시간이 0이면 그 단계는 한 틱 만에 지나간다. 훅에는 끝난 모습(1)을 넘겨야 문이 반쯤
	// 열린 채로 멈춘 것처럼 보이지 않는다.
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}

	return FMath::Clamp(1.0f - PhaseTimer / Duration, 0.0f, 1.0f);
}

void AGridTrain::EnterDoorsOpening()
{
	Phase = ETrainPhase::DoorsOpening;
	PhaseTimer = DoorOpeningSeconds;
	RefreshDoorLook();

	UE_LOG(LogLTTSGrid, Verbose,
		TEXT("%s: doors opening at stop %d '%s'."),
		*GetName(), CurrentStop, *Stops[CurrentStop].StopName.ToString());

	// 첫 프레임을 훅에 알려 둔다. 다음 틱까지 문이 이전 자세로 남아 있지 않게 한다.
	AnimateDoorsOpening(0.0f);
}

void AGridTrain::EnterDoorsOpen()
{
	Phase = ETrainPhase::DoorsOpen;
	PhaseTimer = DoorOpenSeconds;
	RefreshDoorLook();

	// 마지막 틱의 진행도가 1에 조금 못 미쳤을 수 있다. 문이 다 열린 모습을 확정한다.
	AnimateDoorsOpening(1.0f);

	const FGridTrainStop& Stop = Stops[CurrentStop];

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: doors open at stop %d '%s'."), *GetName(), CurrentStop, *Stop.StopName.ToString());

	OnDoorsOpened.Broadcast(this, Stop.StopName);

	// 행인이 내린다. 플레이어가 없는 승강장에서도 일어나는 배경 연출이다.
	if (AGridNPCSpawner* Spawner = Stop.DisembarkSpawner)
	{
		if (Stop.DisembarkCount > 0)
		{
			Spawner->SpawnBurst(Stop.DisembarkCount);
		}
	}

	// 타고 있던 폰을 내려 준다. 카메라가 열차 안을 들여다보지 않으므로, 문이 열리는 것을
	// 기준으로 자동 하차시킨다(기획).
	//
	// 폰은 언제나 문 한가운데에서 문 앞 셀로 똑바로 나온다. 저작된 ExitCell로 곧장 걸어 나오게
	// 하던 시절에는 승강장 가운데 셀을 적어 둔 정차역에서 폰이 차체 벽을 뚫고 비스듬히 나왔다.
	if (AGridPawn* Pawn = Rider.Get())
	{
		FVector ExitWorld = GetActorLocation();
		FIntPoint LandingCell(-1, -1);

		if (Grid)
		{
			TArray<FIntPoint> Boarding;
			GetBoardingCells(CurrentStop, Boarding);

			if (Boarding.Contains(Stop.ExitCell))
			{
				// 저작된 하차 셀이 문 앞이면 그대로 거기로 나온다.
				LandingCell = Stop.ExitCell;
			}
			else
			{
				// 문 앞 셀 가운데 폰에게 가장 가까운 칸으로 내린다. 폰은 탄 문 한가운데에 앉아
				// 있으므로 대개 그 문 앞이다. 목록의 첫 칸을 쓰면 객차 어디에 앉아 있었든 언제나
				// 같은 문으로 나오고, 그 문이 반대쪽 끝이면 폰이 차체를 가로질러 비스듬히 나온다.
				double BestDistanceSq = TNumericLimits<double>::Max();
				for (const FIntPoint& Cell : Boarding)
				{
					if (!Grid->IsValidCell(Cell))
					{
						continue;
					}

					const double DistanceSq = FVector::DistSquaredXY(Grid->CellToWorld(Cell), Pawn->GetActorLocation());
					if (DistanceSq < BestDistanceSq)
					{
						BestDistanceSq = DistanceSq;
						LandingCell = Cell;
					}
				}
			}

			if (Grid->IsValidCell(LandingCell))
			{
				ExitWorld = Grid->CellToWorld(LandingCell);
			}
			else if (Grid->IsValidCell(Stop.ExitCell))
			{
				// 문 앞 셀이 하나도 없는 역(승강장 없는 정차)이다. 저작된 셀밖에 기댈 곳이 없다.
				ExitWorld = Grid->CellToWorld(Stop.ExitCell);
			}
		}

		// 나올 문은 착지 셀을 마주 보는 문이다. 탄 문의 앞에 이 역의 승강장이 없거나(짧은 승강장)
		// 저작된 ExitCell이 다른 문 앞이면, 차체 안에서 그 문 한가운데로 옮긴 뒤 내린다. 차체
		// 안이라 옮기는 순간은 보이지 않는다.
		if (Grid && Grid->IsValidCell(LandingCell))
		{
			TArray<float> Offsets;
			GetDoorOffsets(Offsets);

			const FVector Centre = GetActorLocation();
			const FVector Axis = GetActorForwardVector();
			const int32 Door = LTTSVehicle::NearestDoorIndex(ExitWorld, Centre, Axis, Offsets);

			if (Offsets.IsValidIndex(Door))
			{
				const double OffDoor = FMath::Abs(LTTSVehicle::AlongAxis(ExitWorld, Centre, Axis) - Offsets[Door]);

				if (OffDoor > DoorWidth * 0.5 + Grid->CellSize)
				{
					UE_LOG(LogLTTSGrid, Warning,
						TEXT("%s: stop %d '%s' exit cell (%d,%d) is %.0f cm from door %d along the track; the pawn will leave at an angle."),
						*GetName(), CurrentStop, *Stop.StopName.ToString(), LandingCell.X, LandingCell.Y, OffDoor, Door);
				}
				else
				{
					// 높이는 바꾸지 않는다. 폰은 지금 실려 있는 높이 그대로 문 한가운데로 간다.
					const FVector ExitSeat = LTTSVehicle::SeatAtDoor(
						Centre, Axis, Offsets[Door], GetSeatHalfExtent(), Pawn->GetActorLocation().Z);

					if (FVector::DistXY(ExitSeat, Pawn->GetActorLocation()) > DoorWidth * 0.5f)
					{
						UE_LOG(LogLTTSGrid, Display,
							TEXT("%s: stop %d '%s' exit cell (%d,%d) faces door %d, not the rider's door; moving inside the car to step off there."),
							*GetName(), CurrentStop, *Stop.StopName.ToString(), LandingCell.X, LandingCell.Y, Door);
						Pawn->SetActorLocation(ExitSeat);
					}
				}
			}
		}

		// 문 앞으로 나온 뒤 걸어갈 곳. PostExitCell이 먼저이고, 없으면 문 앞이 아닌 ExitCell이다
		// (예전 정차역은 승강장 가운데 셀을 ExitCell로 적어 두었다. 맵을 고치지 않고 그 자리로 간다).
		UnloadedGoalCell = FIntPoint(-1, -1);
		if (Grid && Grid->IsValidCell(Stop.PostExitCell))
		{
			UnloadedGoalCell = Stop.PostExitCell;
		}
		else if (Grid && Grid->IsValidCell(LandingCell) && Grid->IsValidCell(Stop.ExitCell) && Stop.ExitCell != LandingCell)
		{
			UnloadedGoalCell = Stop.ExitCell;
		}

		Pawn->WalkOntoGrid(ExitWorld);
		Rider.Reset();
		UnloadedPawn = Pawn;
	}
}

void AGridTrain::EnterDoorsClosing()
{
	Phase = ETrainPhase::DoorsClosing;
	PhaseTimer = DoorCloseSeconds;
	RefreshDoorLook();

	UE_LOG(LogLTTSGrid, Verbose, TEXT("%s: doors closing."), *GetName());

	AnimateDoorsClosing(0.0f);
}

void AGridTrain::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Grid || Stops.Num() < 2)
	{
		return;
	}

	// 하차한 폰이 그리드에 다시 서면 지정된 자리까지 마저 걸어가게 한다. 문 앞에서 바로
	// 멈추면 플레이어가 자기가 어디에 내렸는지 알아차리기 전에 열차가 떠난다.
	if (AGridPawn* Unloaded = UnloadedPawn.Get())
	{
		if (Unloaded->IsOnGrid())
		{
			if (Grid->IsValidCell(UnloadedGoalCell) && Unloaded->GetCurrentCell() != UnloadedGoalCell)
			{
				Unloaded->RequestMoveToCell(UnloadedGoalCell);
			}

			UnloadedPawn.Reset();
			UnloadedGoalCell = FIntPoint(-1, -1);
		}
	}

	switch (Phase)
	{
	case ETrainPhase::Idle:
		PhaseTimer -= DeltaSeconds;
		if (PhaseTimer <= 0.0f && GetNextStopIndex() != INDEX_NONE)
		{
			// 첫 역에는 이미 서 있으므로, 대기가 끝나면 문부터 연다.
			EnterDoorsOpening();
		}
		break;

	case ETrainPhase::Moving:
	{
		// 구간이 없다시피 하면(같은 자리) 곧바로 도착으로 친다.
		if (MoveLength <= UE_KINDA_SMALL_NUMBER)
		{
			SetActorLocation(MoveTo);
			EnterDoorsOpening();
			break;
		}

		const float Alpha = static_cast<float>(FMath::Clamp(MoveDistance / MoveLength, 0.0, 1.0));
		const double CurrentSpeed = Speed * GetSpeedFactor(Alpha);
		MoveDistance += CurrentSpeed * DeltaSeconds;

		// 바퀴가 가감속을 따라 빨라지고 느려진다.
		UpdateWheelPlayRate(CurrentSpeed);

		if (MoveDistance >= MoveLength)
		{
			SetActorLocation(MoveTo);
			EnterDoorsOpening();
			break;
		}

		SetActorLocation(FMath::Lerp(MoveFrom, MoveTo, MoveDistance / MoveLength));
		break;
	}

	case ETrainPhase::DoorsOpening:
		PhaseTimer -= DeltaSeconds;
		AnimateDoorsOpening(GetPhaseAlpha(DoorOpeningSeconds));

		// 문이 다 열려야 행인이 쏟아지고 탑승자가 내린다. 그 전까지는 아무도 드나들지 않는다.
		if (PhaseTimer <= 0.0f)
		{
			EnterDoorsOpen();
		}
		break;

	case ETrainPhase::DoorsOpen:
		PhaseTimer -= DeltaSeconds;

		// 타는 중인 폰이 아직 차체 안에 들어오지 못했으면 문을 닫지 않는다. 그대로 출발하면
		// 폰이 문 앞에 서 있던 자리로 걸어가는 동안 열차가 빠져나가 뒤늦게 허공에서 붙는다.
		if (PhaseTimer <= 0.0f && !(Rider.IsValid() && !Rider->IsRiding()))
		{
			EnterDoorsClosing();
		}
		break;

	case ETrainPhase::DoorsClosing:
		PhaseTimer -= DeltaSeconds;
		AnimateDoorsClosing(GetPhaseAlpha(DoorCloseSeconds));

		if (PhaseTimer <= 0.0f)
		{
			EnterMoving();
		}
		break;

	default:
		break;
	}
}

// ---------------------------------------------------------------------------- 탑승

bool AGridTrain::CanBoard(const AGridPawn* Pawn, FText* OutReason) const
{
	if (!Pawn || !Stops.IsValidIndex(CurrentStop))
	{
		return false;
	}

	// 문이 완전히 열려 있을 때만 탄다. 열리는 중과 닫히는 중은 연출 시간이고, 그 사이에
	// 타면 문이 움직이는 내내 폰이 문틀을 통과해 들어가는 그림이 된다.
	if (!AreDoorsOpen())
	{
		if (OutReason)
		{
			if (!AreDoorsMoving())
			{
				*OutReason = NSLOCTEXT("LTTSTrain", "TrainDoorsClosed", "The doors are closed.");
			}
			else if (Phase == ETrainPhase::DoorsOpening)
			{
				*OutReason = NSLOCTEXT("LTTSTrain", "TrainDoorsOpening", "The doors are still opening.");
			}
			else
			{
				*OutReason = NSLOCTEXT("LTTSTrain", "TrainDoorsClosing", "The doors are closing.");
			}
		}
		return false;
	}

	if (!Pawn->IsOnGrid() || Pawn->IsMoving())
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSTrain", "TrainPawnMoving", "Wait until you have stopped walking.");
		}
		return false;
	}

	TArray<FIntPoint> Boarding;
	GetBoardingCells(CurrentStop, Boarding);
	if (!Boarding.Contains(Pawn->GetCurrentCell()))
	{
		if (OutReason)
		{
			*OutReason = NSLOCTEXT("LTTSTrain", "TrainNotAtDoor", "Stand at a door to board.");
		}
		return false;
	}

	return true;
}

bool AGridTrain::TryBoard(AGridPawn* Pawn, FText* OutReason)
{
	if (!CanBoard(Pawn, OutReason))
	{
		return false;
	}

	// 좌석은 **폰이 선 문의 한가운데**다(두 문짝 사이). 진행축(로컬 X) 위의 자리는 가장 가까운
	// 문 중심에서, 폭 방향은 중심선에서 가져온다.
	//
	// 높이는 **폰이 지금 서 있는 높이 그대로**다. 걸어 들어가는 동안에도, 타고 가는 동안에도
	// 바뀌지 않는다. 예전에는 객차 바닥 + 폰의 높이로 잡아 승강장에서 차체로 들어가며 공이
	// 가라앉았다(2026-09-15 요청).
	//
	// 폰의 자리를 그대로 투영하던 시절에는 문 폭에 걸친 탑승 셀 가운데 가장자리 칸에서 타면
	// 문짝 바깥, 곧 문틀과 벽을 지나 들어갔다(2026-09-15). 차체 중심으로 잡으면 45 m짜리 객차
	// 끝에서 탄 폰이 차체를 따라 비스듬히 미끄러져 들어간다.
	TArray<float> Offsets;
	GetDoorOffsets(Offsets);

	const FVector Centre = GetActorLocation();
	const FVector Axis = GetActorForwardVector();
	const FVector PawnLocation = Pawn->GetActorLocation();
	const double SeatZ = PawnLocation.Z;
	const double CellSize = Grid ? Grid->CellSize : 100.0;

	const int32 Door = LTTSVehicle::NearestDoorIndex(PawnLocation, Centre, Axis, Offsets);
	const double PawnAlong = LTTSVehicle::AlongAxis(PawnLocation, Centre, Axis);
	const bool bInFrontOfDoor = Offsets.IsValidIndex(Door)
		&& FMath::Abs(PawnAlong - Offsets[Door]) <= DoorWidth * 0.5 + CellSize;

	// 들어가기 전에 문 한가운데와 일직선이 되는 자리로 옮겨 선다: 폰의 자리를 진행축 방향으로만
	// 옮긴 점이다. 거기서 좌석까지는 차체 면에 수직인 직선이다.
	//
	// 그 자리가 걸을 수 없는 칸(승강장의 기둥이나 계단)이면 옮기지 않고 지금 자리에서 들어간다.
	// 셀 판정 없이 옆으로 걸어가면 공이 구조물 속으로 파고든다.
	TOptional<FVector> Approach;
	bool bApproachBlocked = false;
	if (bInFrontOfDoor)
	{
		const FVector AxisFlat = FVector(Axis.X, Axis.Y, 0.0).GetSafeNormal();
		FVector InLine = PawnLocation + AxisFlat * (Offsets[Door] - PawnAlong);
		InLine.Z = PawnLocation.Z;

		if (!InLine.Equals(PawnLocation, 1.0))
		{
			const FIntPoint InLineCell = Grid ? Grid->WorldToCell(InLine) : Pawn->GetCurrentCell();
			const bool bSameCell = InLineCell == Pawn->GetCurrentCell();
			const bool bFree = Grid && Grid->IsValidCell(InLineCell)
				&& Grid->IsCellWalkableStatic(InLineCell)
				&& !Grid->IsCellOccupied(InLineCell, Pawn);

			if (bSameCell || bFree)
			{
				Approach = InLine;
			}
			else
			{
				bApproachBlocked = true;
			}
		}
	}

	// 손으로 적은 탑승 셀이 문 위치와 어긋난 맵(그레이박스)에서는 예전처럼 폰을 마주 보는 자리로
	// 들어간다. 먼 문으로 끌고 가 차체를 따라 미끄러지게 하는 것보다 낫다.
	const FVector Seat = bInFrontOfDoor
		? LTTSVehicle::SeatAtDoor(Centre, Axis, Offsets[Door], GetSeatHalfExtent(), SeatZ)
		: LTTSVehicle::SeatFacingRider(Pawn->GetActorLocation(), Centre, Axis, GetSeatHalfExtent(), SeatZ);

	Pawn->BoardVehicle(this, Seat, nullptr, Approach);
	Rider = Pawn;

	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: %s boarded at stop %d '%s' from cell (%d,%d) %s (seat %.0f cm along the track, height %.0f)%s."),
		*GetName(), *Pawn->GetName(), CurrentStop, *Stops[CurrentStop].StopName.ToString(),
		Pawn->GetCurrentCell().X, Pawn->GetCurrentCell().Y,
		bInFrontOfDoor ? *FString::Printf(TEXT("through door %d"), Door) : TEXT("facing the rider (no door in front)"),
		LTTSVehicle::AlongAxis(Seat, Centre, Axis), SeatZ,
		Approach.IsSet()
			? *FString::Printf(TEXT(", lining up %.0f cm first"), Offsets[Door] - PawnAlong)
			: (bApproachBlocked ? TEXT(", the spot in line with the door is blocked so entering at an angle") : TEXT("")));

	// 플레이어가 탔으니 더 기다릴 이유가 없다. 남은 대기 시간을 버리고 문을 닫는다.
	if (bDepartAfterBoarding)
	{
		PhaseTimer = FMath::Min(PhaseTimer, 1.5f);
	}

	return true;
}

// ---------------------------------------------------------------------------- 콘솔용

void AGridTrain::ForceArriveAtNextStop()
{
	if (Stops.Num() < 2)
	{
		return;
	}

	if (Phase != ETrainPhase::Moving)
	{
		EnterMoving();
	}

	if (Phase == ETrainPhase::Moving)
	{
		SetActorLocation(GetStopLocation(CurrentStop));
		EnterDoorsOpening();
	}
}

void AGridTrain::ForceDoors(bool bOpen)
{
	if (!Stops.IsValidIndex(CurrentStop))
	{
		return;
	}

	// 열 때도 연출 단계를 건너뛰지 않는다. 콘솔로 연 문만 애니메이션 없이 벌컥 열리면
	// 시험한 것과 실제로 도는 것이 달라진다.
	if (bOpen)
	{
		EnterDoorsOpening();
	}
	else
	{
		EnterDoorsClosing();
	}
}

// ---------------------------------------------------------------------------- 콘솔

namespace
{
	AGridTrain* FindTrain(UWorld* World, const FString& Filter)
	{
		for (TActorIterator<AGridTrain> It(World); It; ++It)
		{
			AGridTrain* Train = *It;
			if (Train && (Filter.IsEmpty() || Train->GetName().Contains(Filter)))
			{
				return Train;
			}
		}

		return nullptr;
	}

	void TrainArriveCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainArrive: run this in play mode."));
			return;
		}

		const FString Filter = Args.IsValidIndex(0) ? Args[0] : FString();
		if (AGridTrain* Train = FindTrain(World, Filter))
		{
			Train->ForceArriveAtNextStop();
		}
		else
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainArrive: no train matching '%s'."), *Filter);
		}
	}

	void TrainRideCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainRide: run this in play mode."));
			return;
		}

		AGridPlayerController* Controller = Cast<AGridPlayerController>(World->GetFirstPlayerController());
		if (!Controller)
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainRide: no grid player controller."));
			return;
		}

		const FString Filter = Args.IsValidIndex(0) ? Args[0] : FString();
		AGridTrain* Train = FindTrain(World, Filter);
		if (!Train)
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainRide: no train matching '%s'."), *Filter);
			return;
		}

		// 시작 셀을 주면 폰을 거기로 옮긴 뒤 탄다. 문 가장자리 칸에서 타는 경우처럼 폰이 설 자리를
		// 정해 두고 시험할 때 쓴다. 옮기는 것만 시험용이고, 탑승은 클릭과 같은 경로를 탄다.
		if (Args.IsValidIndex(2))
		{
			const FIntPoint StartCell(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2]));
			if (AGridPawn* Pawn = Cast<AGridPawn>(Controller->GetPawn()))
			{
				Pawn->TeleportToCell(StartCell);
				UE_LOG(LogLTTSGrid, Display, TEXT("ltts.TrainRide: moved %s to cell (%d,%d)."),
					*Pawn->GetName(), StartCell.X, StartCell.Y);
			}
		}

		UE_LOG(LogLTTSGrid, Display, TEXT("ltts.TrainRide: asking to board %s."), *Train->GetActorNameOrLabel());
		Controller->RequestTrainBoarding(Train);
	}

	void TrainDoorsCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld())
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainDoors: run this in play mode."));
			return;
		}

		const FString Filter = Args.IsValidIndex(0) ? Args[0] : FString();
		const bool bOpen = Args.IsValidIndex(1) ? (FCString::Atoi(*Args[1]) != 0) : true;

		if (AGridTrain* Train = FindTrain(World, Filter))
		{
			Train->ForceDoors(bOpen);
		}
		else
		{
			UE_LOG(LogLTTSGrid, Warning, TEXT("ltts.TrainDoors: no train matching '%s'."), *Filter);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GTrainArriveCommand(
	TEXT("ltts.TrainArrive"),
	TEXT("Bring a train into its next stop right now: ltts.TrainArrive [name substring]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TrainArriveCommand));

static FAutoConsoleCommandWithWorldAndArgs GTrainRideCommand(
	TEXT("ltts.TrainRide"),
	TEXT("Board a train as if it were clicked: ltts.TrainRide [name substring] [start cell X] [start cell Y]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TrainRideCommand));

static FAutoConsoleCommandWithWorldAndArgs GTrainDoorsCommand(
	TEXT("ltts.TrainDoors"),
	TEXT("Open or close a train's doors: ltts.TrainDoors [name substring] [0|1]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TrainDoorsCommand));
