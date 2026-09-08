// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleRotatingPillar.h"

#include "LetsTakeTheSubway.h"
#include "Grid/GridActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** 상속받은 면 패널 배열의 인덱스 순서. 부모 클래스의 FaceWest/East/South/North 이름과 같다. */
	constexpr int32 FaceIndexWest = 0;
	constexpr int32 FaceIndexEast = 1;
	constexpr int32 FaceIndexSouth = 2;
	constexpr int32 FaceIndexNorth = 3;

	constexpr double FaceThickness = 12.0;
}

APuzzleRotatingPillar::APuzzleRotatingPillar()
{
	FootprintSize = FIntPoint(1, 1);
	Height = 400.0f;

	// 홈은 없다. 크기 0 사각형은 어떤 셀도 담지 않으므로 부모의 홈 관련 조회가 전부
	// "없음"으로 떨어진다.
	ChannelOffset = FIntPoint(0, 0);
	ChannelSize = FIntPoint(0, 0);

	// 본체는 첫 번째 슬랩 하나로 그리되, 상자 대신 원기둥을 쓴다. 모서리가 없어야 제자리
	// 회전이 이웃 셀을 침범하지 않는다는 규칙이 눈으로도 읽힌다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded() && SlabMeshes.IsValidIndex(0) && SlabMeshes[0])
	{
		SlabMeshes[0]->SetStaticMesh(CylinderFinder.Object);
	}
}

// ---------------------------------------------------------------------------- 저작

void APuzzleRotatingPillar::NormaliseAuthoring()
{
	// 부모의 정규화는 부르지 않는다: 홀짝 규칙은 1x1이 이미 만족하고, 홈 클램프는 홈을
	// 최소 1x1로 되살려 버린다.
	FootprintSize = FIntPoint(1, 1);
	ChannelOffset = FIntPoint(0, 0);
	ChannelSize = FIntPoint(0, 0);
	MoveAxis = EPuzzleMoveAxis::None;
}

// ---------------------------------------------------------------------------- 부착 면

bool APuzzleRotatingPillar::IsLocalAttachFace(EGridDirection LocalDir) const
{
	switch (LocalDir)
	{
	case EGridDirection::North:	return bAttachNorth;
	case EGridDirection::East:	return bAttachEast;
	case EGridDirection::South:	return bAttachSouth;
	default:					return bAttachWest;
	}
}

bool APuzzleRotatingPillar::IsWorldAttachFace(EGridDirection WorldDir) const
{
	// 로컬 → 월드는 엘리베이터 문과 같은 변환이다. 역변환 대신 네 로컬 면을 돌려 보고
	// 맞는 것을 찾는다: 4개뿐이라 값싸고, 회전 부호 규약을 한 곳(RotateDirection)에만 둔다.
	static const EGridDirection Locals[4] = {
		EGridDirection::North, EGridDirection::East, EGridDirection::South, EGridDirection::West };

	for (const EGridDirection Local : Locals)
	{
		if (LTTSGrid::RotateDirection(Local, GetQuarterTurns()) == WorldDir)
		{
			return IsLocalAttachFace(Local);
		}
	}
	return false;
}

void APuzzleRotatingPillar::GetWorldAttachDirections(TArray<EGridDirection>& OutDirs) const
{
	static const EGridDirection Locals[4] = {
		EGridDirection::North, EGridDirection::East, EGridDirection::South, EGridDirection::West };

	for (const EGridDirection Local : Locals)
	{
		if (IsLocalAttachFace(Local))
		{
			OutDirs.Add(LTTSGrid::RotateDirection(Local, GetQuarterTurns()));
		}
	}
}

bool APuzzleRotatingPillar::IsAttachedToInnerFace(const APuzzleBlock& Block) const
{
	const FIntPoint Pillar = GetPillarCell();

	TArray<EGridDirection> Dirs;
	GetWorldAttachDirections(Dirs);
	if (Dirs.IsEmpty())
	{
		return false;
	}

	TArray<FIntPoint> Cells;
	Block.GatherOccupiedCells(Cells);

	// 큰 장애물과 같은 기준이다: 블록 셀 하나가 부착 면과 변을 공유하면 블록 전체가 함께
	// 돈다. 기둥에서 멀리 뻗어 나간 나머지 셀은 그대로 실려 간다.
	for (const EGridDirection Dir : Dirs)
	{
		if (Cells.Contains(Pillar + LTTSGrid::DirOffset(Dir)))
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------- 점유

void APuzzleRotatingPillar::GatherOccupiedCells(TArray<FIntPoint>& OutCells) const
{
	// 비워 둘 홈이 없으므로 풋프린트 전체(셀 하나)를 점유한다.
	GetRect().GatherCells(OutCells);
}

// ---------------------------------------------------------------------------- 비주얼

void APuzzleRotatingPillar::RefreshVisual()
{
	// 일부러 부모를 부르지 않는다: 부모는 홈 둘레 슬랩 네 장을 배치한다.
	const double CellSize = Grid ? Grid->CellSize : 100.0;
	const double Half = CellSize * 0.5;

	for (int32 Index = 0; Index < SlabMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Slab = SlabMeshes[Index];
		if (!Slab)
		{
			continue;
		}

		const bool bUsed = (Index == 0);
		Slab->SetVisibility(bUsed);
		Slab->SetCollisionEnabled(bUsed ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

		if (bUsed)
		{
			Slab->SetRelativeLocation(FVector(0.0, 0.0, Height * 0.5));
			Slab->SetRelativeScale3D(FVector(CellSize / 100.0, CellSize / 100.0, Height / 100.0));
		}
	}

	// 부착 면 패널: 켜진 면마다 기둥 바깥쪽에 얇은 판을 붙인다. 큰 장애물의 빗금 면과 같은
	// 머티리얼이라 "여기에 붙으면 같이 돈다"가 같은 언어로 읽힌다.
	const double FaceHeight = FMath::Max(Height * InnerFaceHeightRatio, 1.0);

	const bool bFaceUsed[4] = {
		bAttachWest,
		bAttachEast,
		bAttachSouth,
		bAttachNorth
	};

	for (int32 Index = 0; Index < InnerFaceMeshes.Num() && Index < 4; ++Index)
	{
		UStaticMeshComponent* Face = InnerFaceMeshes[Index];
		if (!Face)
		{
			continue;
		}

		Face->SetVisibility(bFaceUsed[Index]);
		if (!bFaceUsed[Index])
		{
			continue;
		}

		FVector Location(0.0, 0.0, FaceHeight * 0.5);
		FVector Scale(1.0, 1.0, FaceHeight / 100.0);

		switch (Index)
		{
		case FaceIndexWest:
			Location.X = -Half - FaceThickness * 0.5;
			Scale.X = FaceThickness / 100.0;
			Scale.Y = CellSize / 100.0;
			break;

		case FaceIndexEast:
			Location.X = Half + FaceThickness * 0.5;
			Scale.X = FaceThickness / 100.0;
			Scale.Y = CellSize / 100.0;
			break;

		case FaceIndexSouth:
			Location.Y = -Half - FaceThickness * 0.5;
			Scale.X = CellSize / 100.0;
			Scale.Y = FaceThickness / 100.0;
			break;

		default:	// North
			Location.Y = Half + FaceThickness * 0.5;
			Scale.X = CellSize / 100.0;
			Scale.Y = FaceThickness / 100.0;
			break;
		}

		Face->SetRelativeLocation(Location);
		Face->SetRelativeScale3D(Scale);
	}
}

// ---------------------------------------------------------------------------- 생명주기

void APuzzleRotatingPillar::BeginPlay()
{
	// 부모(장애물)의 BeginPlay는 홈 셀 검사와 홈 로그뿐이라 건너뛰고, 블록의 BeginPlay로
	// 바로 간다: 그리드 찾기, 바닥 높이, 셀 점유, 서브시스템 등록이 거기 있다.
	APuzzleBlock::BeginPlay();

	if (!Grid)
	{
		return;
	}

	TArray<EGridDirection> Dirs;
	GetWorldAttachDirections(Dirs);

	FString DirNames;
	for (const EGridDirection Dir : Dirs)
	{
		if (!DirNames.IsEmpty())
		{
			DirNames += TEXT(", ");
		}
		DirNames += StaticEnum<EGridDirection>()->GetNameStringByValue(static_cast<int64>(Dir));
	}

	const FIntPoint Cell = GetPillarCell();
	UE_LOG(LogLTTSGrid, Display,
		TEXT("%s: pillar at cell (%d,%d), attach faces [%s]."),
		*GetName(), Cell.X, Cell.Y, DirNames.IsEmpty() ? TEXT("none") : *DirNames);

	if (Dirs.IsEmpty())
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: no attach face is enabled; turning it will carry nothing."), *GetName());
	}
}

#if WITH_EDITOR

bool APuzzleRotatingPillar::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (!InProperty)
	{
		return true;
	}

	// 크기는 1x1로 고정이고 홈은 없다. 셋 다 값을 바꿔도 정규화가 되돌린다.
	const FName Name = InProperty->GetFName();
	return Name != GET_MEMBER_NAME_CHECKED(APuzzleBlock, FootprintSize)
		&& Name != GET_MEMBER_NAME_CHECKED(APuzzleRotatingObstacle, ChannelOffset)
		&& Name != GET_MEMBER_NAME_CHECKED(APuzzleRotatingObstacle, ChannelSize);
}

#endif
