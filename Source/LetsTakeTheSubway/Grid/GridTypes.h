// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GridTypes.generated.h"

/**
 * 이동 관점에서 셀이 무엇인지.
 *
 * NoFloor는 생성 전용 상태다 -- 아래로 쏜 트레이스가 설 수 있는 것을 찾지 못해 셀에
 * 유효한 Z가 없다는 뜻이다. 마커로는 절대 지정할 수 없다(AGridCellMarkerBase::CellType의
 * InvalidEnumValues 메타 참고).
 */
UENUM(BlueprintType)
enum class EGridCellType : uint8
{
	NoFloor			UMETA(DisplayName = "No Floor"),
	Walkable		UMETA(DisplayName = "Walkable"),
	Blocked			UMETA(DisplayName = "Blocked"),
	StageClear		UMETA(DisplayName = "Stage Clear"),
	Conditional		UMETA(DisplayName = "Conditional")
};

/**
 * 셀을 걸을 수 없는 이유. HUD에 표시해 거부 사유를 설명할 수 있게 한다.
 *
 * FGridCellData 안에 uint8로 직렬화되므로 새 값은 끝에만 추가해야 한다 -- 중간에 끼워
 * 넣으면 이미 저장된 모든 셀이 조용히 다른 값으로 해석된다.
 *
 * Object는 이 저장 규칙의 예외다: 런타임 점유자(퍼즐 블록)를 나타내는데, 이는
 * 블록이 슬라이드할 때마다 바뀌므로 질의 결과로만 돌려주고 셀 데이터에는 절대
 * 쓰지 않는다.
 */
UENUM(BlueprintType)
enum class EGridBlockReason : uint8
{
	None			UMETA(DisplayName = "Open"),
	NoFloorHit		UMETA(DisplayName = "No floor"),
	Slope			UMETA(DisplayName = "Too steep"),
	Clearance		UMETA(DisplayName = "Not enough headroom"),
	Marker			UMETA(DisplayName = "Blocked by designer"),
	Object			UMETA(DisplayName = "Blocked by object")
};

/**
 * 편집 가능한 프로퍼티 형태의 4방향.
 *
 * 아래의 EGridDir는 FGridCellData::NeighborMask에 담기는 비트마스크 형태로, 일반
 * namespace enum이라 UPROPERTY가 될 수 없다. 이것은 그 저작용 짝으로, 디자이너가 디테일
 * 패널에서 고르는 것들(예: 엘리베이터의 문 방향)에 쓴다.
 */
UENUM(BlueprintType)
enum class EGridDirection : uint8
{
	North	UMETA(DisplayName = "North (+Y)"),
	East	UMETA(DisplayName = "East (+X)"),
	South	UMETA(DisplayName = "South (-Y)"),
	West	UMETA(DisplayName = "West (-X)")
};

/** FGridCellData::NeighborMask에 저장되는 이웃 비트. 그리드는 4방향이다. */
namespace EGridDir
{
	enum Type : uint8
	{
		North	= 1 << 0,	// +Y
		East	= 1 << 1,	// +X
		South	= 1 << 2,	// -Y
		West	= 1 << 3	// -X
	};
}

namespace LTTSGrid
{
	/** 저작된 방향에 대응하는 이웃 비트. */
	inline uint8 ToDirBit(EGridDirection Dir)
	{
		switch (Dir)
		{
		case EGridDirection::North:	return EGridDir::North;
		case EGridDirection::East:	return EGridDir::East;
		case EGridDirection::South:	return EGridDir::South;
		default:					return EGridDir::West;
		}
	}

	/** 한 방향으로 한 걸음 갔을 때의 셀 오프셋. */
	inline FIntPoint DirOffset(EGridDirection Dir)
	{
		switch (Dir)
		{
		case EGridDirection::North:	return FIntPoint(0, 1);
		case EGridDirection::East:	return FIntPoint(1, 0);
		case EGridDirection::South:	return FIntPoint(0, -1);
		default:					return FIntPoint(-1, 0);
		}
	}

	/**
	 * 방향을 90도 단위로 회전한다.
	 *
	 * 양의 한 번 회전은 요 +90도이며, East를 North로 보낸다(+X가 위, +Y가 오른쪽인
	 * 탑다운 뷰에서는 시계 방향으로 보인다). enum 순서 North, East, South, West에서는
	 * 인덱스가 하나 *뒤로* 가는 것이다.
	 */
	inline EGridDirection RotateDirection(EGridDirection Dir, int32 QuarterTurns)
	{
		const int32 Index = static_cast<int32>(Dir);
		const int32 Turned = ((Index - QuarterTurns) % 4 + 4) % 4;
		return static_cast<EGridDirection>(Turned);
	}

	/**
	 * 이 태그가 붙은 액터는 그리드 생성 트레이스에서 건너뛴다.
	 *
	 * 퍼즐 블록은 바닥 위에 서 있으므로, 그냥 두면 자기 자신을 Blocked 셀로 구워 넣어
	 * 우연히 배치된 그 위치로 레벨 레이아웃을 굳혀 버린다. 함수 지역 static을 써서 FName
	 * 생성을 정적 초기화 밖으로 뺀다.
	 */
	inline const FName& GenerationIgnoreTag()
	{
		static const FName Tag(TEXT("GridTraceIgnore"));
		return Tag;
	}

	/**
	 * 이 태그가 붙은 폰은 셀 점유자를 통과한다.
	 *
	 * 통과 여부는 질의마다 정할 값이 아니라 이동체 자신의 성질이다. 함수 인자로 두면 같은
	 * 폰의 길찾기, 매 걸음 재검사, 진입 셀 탐색이 서로 다른 답을 낼 수 있다. 태그로 두면
	 * CanPawnEnter를 지나가는 모든 경로가 자동으로 일치하고, 그리드는 NPC 클래스를 몰라도
	 * 된다. 행인 NPC가 플레이어와 퍼즐 블록을 뚫고 지나가는 데 쓴다.
	 */
	inline const FName& PassThroughOccupantsTag()
	{
		static const FName Tag(TEXT("GridPassThrough"));
		return Tag;
	}
}

/**
 * 그리드 셀 하나. 트레이스로 생성된 뒤 마커 오버라이드로 덮어써질 수 있다.
 *
 * NeighborMask는 생성 시점에 단차 높이 규칙으로 구워지므로, 길찾기는 FloorZ나 어떤 생성
 * 설정도 볼 필요가 없다.
 */
USTRUCT()
struct FGridCellData
{
	GENERATED_BODY()

	/** 이 셀 중심 아래 바닥의 월드 Z. Type == NoFloor일 때는 의미 없다. */
	UPROPERTY()
	float FloorZ = 0.0f;

	/**
	 * 마커 오버라이드를 적용하기 전, 트레이스가 판정한 결과.
	 *
	 * 최종 타입과 나란히 보관해 두면 다시 트레이스하지 않고도 오버라이드를 처음부터 다시
	 * 적용할 수 있다: 마커를 지우면 생성이 찾아낸 상태가 정확히 복원되는데, BlockReason만으로
	 * 재구성해서는 그렇게 할 수 없다.
	 */
	UPROPERTY()
	EGridCellType GeneratedType = EGridCellType::NoFloor;

	UPROPERTY()
	EGridBlockReason GeneratedReason = EGridBlockReason::NoFloorHit;

	/** 오버라이드 적용 후의 최종 타입. 게임플레이가 읽는 값이다. */
	UPROPERTY()
	EGridCellType Type = EGridCellType::NoFloor;

	UPROPERTY()
	EGridBlockReason BlockReason = EGridBlockReason::NoFloorHit;

	/** 단차 높이 기준으로 갈 수 있는 4방향 이웃의 EGridDir 비트. 항상 대칭이다. */
	UPROPERTY()
	uint8 NeighborMask = 0;

	/** AGridActor::ConditionalRules의 인덱스, 없으면 INDEX_NONE. Type == Conditional일 때만 쓴다. */
	UPROPERTY()
	int32 RuleIndex = INDEX_NONE;

	/** 바닥 경사(도). 디버그/통계 용도로만 쓴다. */
	UPROPERTY()
	float SlopeDeg = 0.0f;
};

/**
 * 레벨의 마커에서 구운, 저작된 셀 오버라이드 하나.
 *
 * 그리드 액터에 직렬화해 두므로, 런타임 재생성(bRegenerateOnPlay) 시 에디터 전용이라
 * 쿡에서 제거되는 마커 없이도 디자이너의 의도를 다시 적용할 수 있다.
 */
USTRUCT()
struct FGridCellOverride
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint Cell = FIntPoint::ZeroValue;

	UPROPERTY()
	EGridCellType Type = EGridCellType::Walkable;

	UPROPERTY()
	int32 RuleIndex = INDEX_NONE;
};
