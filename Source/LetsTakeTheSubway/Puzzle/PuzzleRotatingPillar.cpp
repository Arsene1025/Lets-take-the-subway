// Copyright Epic Games, Inc. All Rights Reserved.

#include "Puzzle/PuzzleRotatingPillar.h"

#include "LetsTakeTheSubway.h"
#include "Art/ArtMaterialUtil.h"
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
	// 부모의 정규화는 부르지 않는다: 홈 클램프가 홈을 최소 1x1로 되살려 버리고, 부모가
	// 강제하는 최소 2x2도 기둥에는 맞지 않는다.
	//
	// 기둥은 **정사각형**이면 된다. 두 변이 같으면 홀짝 규칙(양변의 홀짝이 같아야 한다)이
	// 저절로 만족되고, GetRegion()이 돌려주는 회전 정사각형이 풋프린트와 정확히 겹쳐
	// 제자리 회전이 성립한다. 1x1로 못박아 두면 2 m짜리 아트 기둥이 셀 하나에 담기지
	// 않아 이웃 셀을 시각적으로 침범한다.
	const int32 Side = FMath::Max(1, FMath::Max(FootprintSize.X, FootprintSize.Y));
	if (FootprintSize.X != FootprintSize.Y)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: a pillar must be square; footprint %dx%d widened to %dx%d."),
			*GetName(), FootprintSize.X, FootprintSize.Y, Side, Side);
	}
	FootprintSize = FIntPoint(Side, Side);
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
	TArray<EGridDirection> Dirs;
	GetWorldAttachDirections(Dirs);
	if (Dirs.IsEmpty())
	{
		return false;
	}

	TArray<FIntPoint> Cells;
	Block.GatherOccupiedCells(Cells);

	const FGridRect Rect = GetRect();
	TArray<FIntPoint> Own;
	Rect.GatherCells(Own);

	// 큰 장애물과 같은 기준이다: 블록 셀 하나가 부착 면과 변을 공유하면 블록 전체가 함께
	// 돈다. 기둥에서 멀리 뻗어 나간 나머지 셀은 그대로 실려 간다.
	//
	// 면 전체를 훑는다. 기둥이 2x2 이상이면 한 면이 여러 셀에 걸치므로, 모서리 셀의
	// 이웃 하나만 보면 그 면 가운데에 붙은 블록을 놓친다.
	for (const EGridDirection Dir : Dirs)
	{
		const FIntPoint Step = LTTSGrid::DirOffset(Dir);
		for (const FIntPoint& Cell : Own)
		{
			const FIntPoint Outside = Cell + Step;
			if (Rect.Contains(Outside))
			{
				// 안쪽 이웃이다 -- 이 셀은 그 면에 접해 있지 않다.
				continue;
			}
			if (Cells.Contains(Outside))
			{
				return true;
			}
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
	//
	// 부모의 아트 처리(ArtMesh 컴포넌트 갱신)는 여기서 직접 한다. 부모의 RefreshVisual을
	// 통째로 부를 수 없어서다.
	if (ArtMeshComponent)
	{
		if (ArtMeshComponent->GetStaticMesh() != ArtMesh)
		{
			ArtMeshComponent->SetStaticMesh(ArtMesh);
		}
		ArtMeshComponent->SetRelativeTransform(ArtMeshOffset);
		ArtMeshComponent->SetVisibility(ArtMesh != nullptr);
		LTTSArt::ReplaceDefaultMaterials(*ArtMeshComponent, ArtFallbackMaterial);
	}

	// 아트가 붙으면 그레이박스 원기둥은 숨긴다. 콜리전은 남긴다 -- 커서가 잡는 것은 언제나
	// 셀 하나 크기의 이 프록시이지 아트 기둥의 실루엣이 아니다.
	const bool bArt = IsUsingArtVisual();

	const double CellSize = Grid ? Grid->CellSize : 100.0;

	// 기둥은 정사각형이므로 한 변의 길이 하나로 비주얼이 정해진다.
	const double Span = FMath::Max(1, FootprintSize.X) * CellSize;
	const double Half = Span * 0.5;

	for (int32 Index = 0; Index < SlabMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Slab = SlabMeshes[Index];
		if (!Slab)
		{
			continue;
		}

		const bool bUsed = (Index == 0);
		Slab->SetVisibility(bUsed && !bArt);
		Slab->SetCollisionEnabled(bUsed ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

		if (bUsed)
		{
			Slab->SetRelativeLocation(FVector(0.0, 0.0, Height * 0.5));
			Slab->SetRelativeScale3D(FVector(Span / 100.0, Span / 100.0, Height / 100.0));
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

		// 아트 위에는 빗금 패널을 그리지 않는다. 부착 면은 레버 피드백과 실제 회전으로
		// 읽히며, 아트 기둥 표면에 그레이박스 판이 떠 있으면 그 편이 더 혼란스럽다.
		Face->SetVisibility(bFaceUsed[Index] && !bArt);
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
			Scale.Y = Span / 100.0;
			break;

		case FaceIndexEast:
			Location.X = Half + FaceThickness * 0.5;
			Scale.X = FaceThickness / 100.0;
			Scale.Y = Span / 100.0;
			break;

		case FaceIndexSouth:
			Location.Y = -Half - FaceThickness * 0.5;
			Scale.X = Span / 100.0;
			Scale.Y = FaceThickness / 100.0;
			break;

		default:	// North
			Location.Y = Half + FaceThickness * 0.5;
			Scale.X = Span / 100.0;
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

	// 홈은 없다. 값을 바꿔도 정규화가 되돌린다. 풋프린트는 편집할 수 있다 -- 정사각형이기만
	// 하면 되며, 정사각형이 아니면 정규화가 넓은 쪽으로 맞춘다.
	const FName Name = InProperty->GetFName();
	return Name != GET_MEMBER_NAME_CHECKED(APuzzleRotatingObstacle, ChannelOffset)
		&& Name != GET_MEMBER_NAME_CHECKED(APuzzleRotatingObstacle, ChannelSize);
}

#endif
