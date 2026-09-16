// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundKeys.h"
#include "UI/Guide/GuideType.h"
#include "GridTrain.generated.h"

class AGridActor;
class AGridNPC;
class AGridNPCSpawner;
class AGridPawn;
class UAnimSequenceBase;
class UAudioComponent;
class UAnimationAsset;
class UCurveFloat;
class UMaterialInterface;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * 열차가 서는 곳 하나.
 *
 * 위치를 월드 좌표가 아니라 셀로 적는 이유는 이 프로젝트의 다른 모든 저작과 같다: 셀 번호는
 * 그리드 디버그 오버레이에서 그대로 읽을 수 있지만 월드 좌표는 계산해야 한다. 높이는 열차를
 * 배치한 Z를 그대로 쓰므로 적지 않는다.
 */
USTRUCT(BlueprintType)
struct FGridTrainStop
{
	GENERATED_BODY()

	/** 로그와 피드백에 쓰는 이름. */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	FName StopName;

	/** 열차 중심이 서는 셀. XY만 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	FIntPoint StopCell = FIntPoint::ZeroValue;

	/**
	 * 문 앞 승강장 셀. 폰이 여기 서 있을 때만 열차를 클릭해 탈 수 있다.
	 *
	 * 이 목록은 탑승 조건이면서 동시에 커서 규칙이기도 하다. 폰이 여기 없으면 열차는 커서에
	 * 잡히지 않고 뒤쪽 바닥이 클릭된다. 그러지 않으면 4 m짜리 차체가 승강장 셀을 가려
	 * 영영 클릭할 수 없게 된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	TArray<FIntPoint> BoardingCells;

	/**
	 * 이 역에서 내린 폰이 향할 셀.
	 *
	 * 폰은 언제나 **문 한가운데에서 문 앞 셀로** 똑바로 걸어 나온다. 이 셀이 문 앞 셀(이 역의
	 * 탑승 셀) 가운데 하나면 거기로 나오고, 아니면 가장 가까운 문 앞 셀로 나온 뒤 PostExitCell이
	 * 비어 있을 때 이 셀까지 마저 걸어간다. 예전에는 이 셀로 곧장 걸어 나왔는데, 승강장 가운데
	 * 셀을 적어 둔 정차역에서는 폰이 차체 벽을 뚫고 비스듬히 나왔다(2026-09-15).
	 *
	 * 유효하지 않으면 문 앞 셀 가운데 폰에게 가장 가까운 칸으로 내린다. 정확한 셀을 못
	 * 찾더라도 그리드가 근처에서 걸을 수 있는 셀을 찾아 준다(AGridActor::FindEntryCell).
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	FIntPoint ExitCell = FIntPoint(-1, -1);

	/**
	 * 내린 뒤 이어서 걸어갈 셀. 유효하지 않으면 ExitCell(문 앞 셀이 아닐 때)로, 그것도 없으면
	 * 내린 자리에 선다.
	 *
	 * 기획의 "일정 위치까지 이동한 뒤 정지"다. 문 앞에서 바로 조작이 돌아오면 플레이어가
	 * 자기가 어디에 내렸는지 알아차리기 전에 열차가 떠난다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	FIntPoint PostExitCell = FIntPoint(-1, -1);

	/**
	 * 이 역에서 문이 열릴 때 행인을 내보낼 스포너.
	 *
	 * 스포너를 열차가 아니라 승강장에 두는 이유는 경유 셀이 역마다 다르기 때문이다. 열차에
	 * 붙이면 정차역마다 목록을 갈아 끼워야 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop")
	TObjectPtr<AGridNPCSpawner> DisembarkSpawner;

	UPROPERTY(EditAnywhere, Category = "Train Stop", meta = (ClampMin = 0, ClampMax = 32))
	int32 DisembarkCount = 3;

	/**
	 * 이 역으로 들어올 때 알림음·진입음·정차음을 낼지.
	 *
	 * 알림음은 위치 없이(2D) 나므로 화면 밖 회차역으로 들어올 때도 들린다. 플레이어가 볼 일이 없는
	 * 역은 끈다. 문 소리는 열차에 붙어 있어 거리로 줄어들므로 이 값과 상관없이 난다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop|Sound")
	bool bArrivalSounds = true;

	/**
	 * 플레이어가 이 역에서 내린 뒤 처음으로 이동 입력(이동키, 바닥 클릭)을 했을 때 요청할 가이드 팝업.
	 * None이면 없음(2026-09-16). 내린 뒤 PostExitCell까지 열차가 시키는 자동 걸음은 입력이 아니다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train Stop|UI")
	EGuideType GuideOnFirstMoveAfterExit = EGuideType::None;
};

/**
 * 열차가 내는 소리의 키. 기본값은 표준 키이고, 열차마다 다른 소리를 쓰려면 DA_SoundLibrary에 키를
 * 더한 뒤 여기서 바꾼다. None이면 그 소리를 내지 않는다.
 */
USTRUCT(BlueprintType)
struct FTrainSoundKeys
{
	GENERATED_BODY()

	/** 역으로 들어오기 전 승강장 알림음. 2D. */
	UPROPERTY(EditAnywhere, Category = "Sound")
	FName Notification = LTTSSoundKeys::TrainNotification;

	/** 진입음. 열차에 붙는다. */
	UPROPERTY(EditAnywhere, Category = "Sound")
	FName Approach = LTTSSoundKeys::TrainApproach;

	/** 정차음. 열차에 붙는다. */
	UPROPERTY(EditAnywhere, Category = "Sound")
	FName Stop = LTTSSoundKeys::TrainStop;

	UPROPERTY(EditAnywhere, Category = "Sound")
	FName DoorOpen = LTTSSoundKeys::TrainDoorOpen;

	UPROPERTY(EditAnywhere, Category = "Sound")
	FName DoorClose = LTTSSoundKeys::TrainDoorClose;

	/** 플레이어가 타고 이동하는 동안의 루프. 2D. */
	UPROPERTY(EditAnywhere, Category = "Sound")
	FName Inside = LTTSSoundKeys::TrainInside;

	/** 플레이어가 타고 있을 때 문이 열리기 시작하면 나는 하차 안내 방송. 2D. */
	UPROPERTY(EditAnywhere, Category = "Sound")
	FName ArrivalAnnouncement = LTTSSoundKeys::TrainArrivalAnnouncement;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrainDoorsOpened, AGridTrain*, Train, FName, StopName);

/**
 * 승강장 사이를 오가는 열차.
 *
 * 퍼즐 조각이 아니라서 Puzzle이 아닌 Vehicle에 산다. 그리드 셀을 점유하지도 않는다 --
 * 선로는 애초에 걸을 수 없는 바닥이라 막을 것이 없다.
 *
 * 기획의 두 역할을 함께 한다. 하나는 연출이다: 플레이어가 퍼즐을 푸는 동안에도 열차가
 * 주기적으로 들어와 문을 열고 행인을 쏟아낸 뒤 떠난다. 다른 하나는 이동 수단이다: 문이
 * 열려 있을 때 문 앞에 선 플레이어가 클릭하면 타고, 다음 역에서 자동으로 내린다.
 *
 * 선로는 스플라인이 아니라 정차역 셀들의 직선 구간이다. 지하철 승강장 구간은 곧고, 셀
 * 번호는 디버그 오버레이에서 그대로 읽을 수 있다. 곡선이 필요해지면 그때 경로 액터를 둔다.
 */
UCLASS(HideCategories = (Physics, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API AGridTrain : public AActor
{
	GENERATED_BODY()

public:
	AGridTrain();

	// ---------------------------------------------------------------- 저작

	/** 순서대로 도는 정차역. 최소 둘은 있어야 열차가 움직인다. */
	UPROPERTY(EditAnywhere, Category = "Train")
	TArray<FGridTrainStop> Stops;

	/** 마지막 역 다음에 첫 역으로 돌아간다. 끄면 마지막 역에서 멈춘 채로 있는다. */
	UPROPERTY(EditAnywhere, Category = "Train")
	bool bLoop = true;

	UPROPERTY(EditAnywhere, Category = "Train", meta = (ClampMin = 1.0))
	float Speed = 1200.0f;

	/**
	 * 문이 열리는 연출에 쓰는 시간(초). 이 동안에는 타지도 내리지도 못한다.
	 *
	 * 문 시간은 셋이고 순서대로 이어진다: 열리는 동안(DoorOpeningSeconds), 완전히 열린 채
	 * 기다리는 동안(DoorOpenSeconds), 닫히는 동안(DoorCloseSeconds). 가운데 시간에만
	 * 승하차가 열린다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train", meta = (ClampMin = 0.0))
	float DoorOpeningSeconds = 1.0f;

	/** 문이 완전히 열린 채 기다리는 시간(초). 기획의 5초가 기본값이다. 이 동안에만 타고 내린다. */
	UPROPERTY(EditAnywhere, Category = "Train", meta = (ClampMin = 0.5))
	float DoorOpenSeconds = 5.0f;

	/** 문이 닫히는 연출에 쓰는 시간(초). 이 동안에는 탈 수 없다. */
	UPROPERTY(EditAnywhere, Category = "Train", meta = (ClampMin = 0.0))
	float DoorCloseSeconds = 1.0f;

	/** 플레이어가 타면 남은 대기 시간을 버리고 곧바로 문을 닫는다. */
	UPROPERTY(EditAnywhere, Category = "Train")
	bool bDepartAfterBoarding = true;

	/** 첫 역에 도착하기까지의 대기(초). 여러 열차의 시각을 어긋나게 할 때 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Train", meta = (ClampMin = 0.0))
	float StartDelaySeconds = 2.0f;

	/** 차체 길이(cm), 진행 축 방향. */
	UPROPERTY(EditAnywhere, Category = "Train|Body", meta = (ClampMin = 100.0))
	float BodyLength = 1600.0f;

	/** 차체 폭(cm). */
	UPROPERTY(EditAnywhere, Category = "Train|Body", meta = (ClampMin = 100.0))
	float BodyWidth = 400.0f;

	/** 차체 높이(cm). 액터 원점이 객차 바닥이고 몸체는 그 위로 올라간다. */
	UPROPERTY(EditAnywhere, Category = "Train|Body", meta = (ClampMin = 100.0))
	float BodyHeight = 400.0f;

	/**
	 * 그레이박스 차체와 문 슬랩을 그릴지. 아트 메시를 붙인 블루프린트에서는 끈다.
	 *
	 * **콜리전은 끄지 않는다.** 퍼즐 조각과 같은 규칙이다: 눈에 보이는 것은 아트이고,
	 * 커서가 잡는 것은 언제나 BodyLength x BodyWidth 크기의 이 프록시 상자다. 그래야
	 * 열차를 클릭하는 판정이 아트 메시 여러 장의 실루엣에 좌우되지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Body")
	bool bShowProxyBody = true;

	/**
	 * 문 중심의 위치. 액터 원점에서 진행 축(로컬 X)을 따라 잰 거리(cm)다.
	 *
	 * 탑승 셀을 여기서 유도한다. 손으로 셀 번호를 적으면 아트 문이 실제로 어디 있는지와
	 * 어긋나기 쉽고(그레이박스 프록시 문은 차체 길이의 ±25 % 자리에 있어 아트와 전혀
	 * 겹치지 않는다), 정차역을 하나 늘릴 때마다 같은 목록을 다시 세어야 한다. 문 위치는
	 * 차량의 성질이므로 한 번만 적고 정차역마다 셀을 계산한다.
	 *
	 * 비워 두면 프록시 문 두 짝(±BodyLength/4)을 문으로 친다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Body")
	TArray<float> DoorOffsetsLocal;

	/** 문 하나의 폭(cm). 이 폭에 걸치는 승강장 셀이 모두 탑승 셀이 된다. */
	UPROPERTY(EditAnywhere, Category = "Train|Body", meta = (ClampMin = 50.0))
	float DoorWidth = 200.0f;

	/**
	 * 블루프린트가 붙인 아트 메시의 빈 슬롯에 씌울 머티리얼.
	 *
	 * 임포트된 메시는 슬롯이 비면 엔진 기본(WorldGridMaterial, 회색 격자)으로 그려진다.
	 * 원본 스태틱 메시 액터는 컴포넌트 오버라이드로 툰 머티리얼을 쓰고 있었지만, 메시를
	 * 블루프린트 컴포넌트로 옮기면 그 오버라이드는 따라오지 않는다. 이미 칠해진 슬롯은
	 * 건드리지 않는다. 어느 머티리얼인지는 아트의 결정이라 블루프린트 기본값에서 지정한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Body")
	TObjectPtr<UMaterialInterface> ArtFallbackMaterial;

	// ---------------------------------------------------------------- 아트 애니메이션

	/**
	 * 문과 바퀴가 움직이는 아트 차체. 스켈레탈 메시는 블루프린트 기본값에서 지정한다.
	 *
	 * 메시가 비어 있으면(그레이박스 열차) 아래 애니메이션은 모두 아무것도 하지 않는다.
	 * 콜리전은 없다. 커서가 잡는 것은 여전히 BodyMesh 프록시 상자다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Train|Art")
	TObjectPtr<USkeletalMeshComponent> ArtMesh;

	/**
	 * 문이 열리는 애니메이션. 재생하지 않고 DoorsOpening 단계의 진행도에 맞춰 위치를 짚는다.
	 *
	 * 재생 대신 짚는 이유: 승하차를 막는 것은 단계 타이머다. 애니메이션을 따로 재생하면 프레임이
	 * 튀거나 콘솔로 단계를 건너뛸 때 눈에 보이는 문과 탈 수 있는지가 어긋난다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Art")
	TObjectPtr<UAnimSequenceBase> DoorOpenAnimation;

	/** 문이 닫히는 애니메이션. 비워 두면 DoorOpenAnimation을 거꾸로 짚어 대신한다. */
	UPROPERTY(EditAnywhere, Category = "Train|Art")
	TObjectPtr<UAnimSequenceBase> DoorCloseAnimation;

	/** 달리는 동안 반복 재생하는 바퀴 애니메이션. 비워 두면 바퀴는 멈춰 있다. */
	UPROPERTY(EditAnywhere, Category = "Train|Art")
	TObjectPtr<UAnimSequenceBase> WheelAnimation;

	/**
	 * 문 애니메이션의 길이로 DoorOpeningSeconds와 DoorCloseSeconds를 덮어쓴다(BeginPlay).
	 *
	 * 아트가 정한 속도 그대로 문이 움직이고, 탑승을 막는 시간도 그만큼 늘어난다. 끄면 위의 두
	 * 시간에 맞춰 애니메이션을 늘이거나 줄인다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Art")
	bool bMatchDoorTimingToAnimation = true;

	/**
	 * 바퀴 애니메이션을 PlayRate 1로 돌릴 때 열차가 초당 나아가야 할 거리(cm/s).
	 *
	 * 0이 아니면 달리는 동안 PlayRate를 지금 속도 / 이 값으로 맞춰 바퀴가 가감속을 따라간다.
	 * 0이면 PlayRate에 손대지 않는다. AGridEscalator::AnimStepSpeedAtRate1과 같은 규약이다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Art", meta = (ClampMin = 0.0))
	float WheelAnimSpeedAtRate1 = 0.0f;

	// ---------------------------------------------------------------- 탑승 셀 자동 계산

	/**
	 * 문 앞에서 승강장 쪽으로 몇 칸까지 탑승 셀을 찾을지.
	 *
	 * 차체 반폭 바로 바깥 칸부터 센다. 승강장과 선로 사이에 턱이나 틈이 있어 승강장이
	 * 몇 칸 떨어져 있으면 이 값을 늘린다. Subway_Stage2의 입구·서쪽 승강장은 5번째 칸에
	 * 있다(2026-09-14 노선 연장). BoardingCells를 손으로 적은 정차역에는 쓰이지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Boarding", meta = (ClampMin = 1, ClampMax = 16))
	int32 BoardingSearchCells = 6;

	/**
	 * 탑승 셀로 인정할 바닥 높이 범위(cm). 선로 높이(배치 Z) 기준 위아래로 이만큼.
	 *
	 * 선로 옆의 낮은 턱도 걸을 수 있는 셀로 구워지므로, 높이를 보지 않으면 폰이 갈 수 없는
	 * 턱 위에 탑승 셀이 잡힌다. Stage1 승강장은 +98, Stage2 승강장은 +132, Stage2 턱은
	 * -224 cm다. 0 이하면 높이를 보지 않는다(이전 동작).
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Boarding", meta = (ClampMin = 0.0))
	float BoardingFloorTolerance = 150.0f;

	// ---------------------------------------------------------------- 가감속

	/**
	 * 구간 진행도(X, 0~1)에 대한 속도 배율(Y, 0~1) 커브.
	 *
	 * 비워 두면 아래 AccelFraction/DecelFraction로 만드는 기본 곡선을 쓴다. 커브를 지정하면
	 * 그쪽이 이긴다 -- 출발과 도착의 손맛은 디자이너가 커브 에디터에서 만지는 편이 낫다.
	 *
	 * Speed는 이제 정속이 아니라 **최고 속도**다. 배율 1인 구간에서만 그 속도가 나온다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Motion")
	TObjectPtr<UCurveFloat> SpeedCurve;

	/** 커브가 없을 때 최고 속도까지 올리는 데 쓰는 구간 비율. */
	UPROPERTY(EditAnywhere, Category = "Train|Motion", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float AccelFraction = 0.30f;

	/** 커브가 없을 때 멈추는 데 쓰는 구간 비율. 가속보다 짧으면 도착이 급하게 느껴진다. */
	UPROPERTY(EditAnywhere, Category = "Train|Motion", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DecelFraction = 0.15f;

	/** 가감속 곡선의 날카로움. 1이면 직선, 클수록 시작과 끝이 부드럽다. */
	UPROPERTY(EditAnywhere, Category = "Train|Motion", meta = (ClampMin = 1.0, ClampMax = 5.0))
	float EaseExponent = 2.0f;

	/**
	 * 속도 배율의 하한.
	 *
	 * 0이 되는 순간 열차는 영영 도착하지 못한다. 커브를 잘못 그려도 멈춰 서지 않도록
	 * 배율을 여기까지만 낮춘다.
	 *
	 * 안전장치이면서 동시에 손맛 값이다. 구간을 지나는 데 걸리는 시간은 배율의 **조화평균**에
	 * 좌우되므로, 양 끝에서 배율이 0에 가까워지면 그 짧은 구간이 전체 시간을 삼킨다. 0.05로
	 * 두었더니 50 m 구간이 4.2초가 아니라 17초가 걸렸다. 0.25면 약 1.9배(8초)로, 출발과
	 * 도착이 눈에 띄게 부드러우면서도 열차가 기어가지 않는다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Motion", meta = (ClampMin = 0.001, ClampMax = 1.0))
	float MinSpeedFactor = 0.25f;

	// ---------------------------------------------------------------- 사운드

	UPROPERTY(EditAnywhere, Category = "Train|Sound")
	FTrainSoundKeys SoundKeys;

	/**
	 * 도착까지 남은 거리가 이 값(cm) 이하가 되면 알림음을 낸다. 0이면 출발하자마자.
	 *
	 * 세 거리 값은 모두 구간의 도착점에서 거꾸로 잰다. 구간 길이보다 크면 출발 순간에 난다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Sound", meta = (ClampMin = 0.0))
	float NotificationDistance = 0.0f;

	/**
	 * 남은 거리가 이 값(cm) 이하가 되면 진입음을 낸다. 0이면 출발하자마자.
	 *
	 * 진입음(WAV_01)은 10초 가까운 원샷이다. Stage1 구간(50 m, 약 8초)에는 0이 맞고, 열차가 화면
	 * 밖 먼 곳에서 출발하는 긴 구간은 값을 올려 화면에 잡히는 지점에 맞춘다. 문이 열리기 시작할 때
	 * 아직 나고 있으면 짧게 페이드 아웃한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Sound", meta = (ClampMin = 0.0))
	float ApproachSoundDistance = 0.0f;

	/**
	 * 남은 거리가 이 값(cm) 이하가 되면 정차음("고오오오 착", 약 2.3초)을 낸다.
	 *
	 * 끝의 "착"이 멈추는 순간에 맞도록 감속 구간보다 조금 앞에서 시작한다. 속도·감속 비율을 바꾸면
	 * 함께 맞춘다.
	 */
	UPROPERTY(EditAnywhere, Category = "Train|Sound", meta = (ClampMin = 0.0))
	float StopSoundDistance = 1500.0f;

	/** 문이 열렸을 때 발생한다. 하차 연출을 여기에 매달 수 있다. */
	UPROPERTY(BlueprintAssignable, Category = "Train")
	FOnTrainDoorsOpened OnDoorsOpened;

	// ---------------------------------------------------------------- 조회

	/** 문이 완전히 열려 있어 타고 내릴 수 있으면 true. 열리는 중과 닫히는 중은 false다. */
	bool AreDoorsOpen() const { return Phase == ETrainPhase::DoorsOpen; }

	/** 문이 움직이는 중(열리는 중이거나 닫히는 중)이면 true. 이 동안에는 승하차가 막힌다. */
	bool AreDoorsMoving() const
	{
		return Phase == ETrainPhase::DoorsOpening || Phase == ETrainPhase::DoorsClosing;
	}

	/** 지금 서 있는(또는 향해 가는) 정차역의 인덱스. */
	int32 GetCurrentStopIndex() const { return CurrentStop; }

	/**
	 * 정차역에서 문 앞이 되는 승강장 셀.
	 *
	 * 저작된 BoardingCells가 있으면 그대로 돌려준다. 비어 있으면 DoorOffsetsLocal과
	 * 정차 위치에서 계산한다.
	 */
	void GetBoardingCells(int32 StopIndex, TArray<FIntPoint>& OutCells) const;

	/**
	 * 정차역을 가리지 않고, 이 열차의 문 앞이 되는 승강장 셀 전부.
	 *
	 * 플레이어가 멀리서 열차를 클릭하면 폰이 여기 중 가장 가까운 칸으로 걸어가 기다린다.
	 * 정차역을 가리지 않는 이유는 클릭한 순간 열차가 터널 한가운데 있을 수도 있기 때문이다 --
	 * 그때 "지금 서 있는 역"은 아직 아무 데도 아니다. 승강장이 없는 역(터널 정차)은 탑승
	 * 셀이 비므로 저절로 빠진다.
	 */
	void GetApproachCells(TArray<FIntPoint>& OutCells) const;

	/**
	 * 정차역을 가리지 않고, 문마다 문 한가운데에 가장 가까운 문 앞 칸 하나씩.
	 *
	 * 플레이어가 열차를 누르면 폰은 먼저 여기로 가서 기다린다. 문 폭에 걸친 칸 가운데 가장자리에
	 * 서 있으면 탈 때 문을 따라 옆으로 한참 걸어야 하므로, 문 한가운데와 거의 일직선인 칸에서
	 * 기다리게 한다. 문 폭 안에 칸이 없는 문은 빠진다.
	 */
	void GetDoorFrontCells(TArray<FIntPoint>& OutCells) const;

	/** 폰이 지금 이 열차에 탈 수 있으면 true. 아니면 이유를 알려준다. */
	bool CanBoard(const AGridPawn* Pawn, FText* OutReason = nullptr) const;

	// ---------------------------------------------------------------- 사용

	/** 폰을 태우거나, 왜 못 타는지 설명한다. */
	bool TryBoard(AGridPawn* Pawn, FText* OutReason = nullptr);

	/** 지금 즉시 다음 역에 도착시킨다(콘솔 시험용). */
	void ForceArriveAtNextStop();

	/** 문을 강제로 열거나 닫는다(콘솔 시험용). */
	void ForceDoors(bool bOpen);

	/**
	 * 스켈레탈 메시의 문 본 위치로 문 중심을 구해 DoorOffsetsLocal과 비교해 로그로 찍는다.
	 *
	 * 아트 메시의 배치나 문 위치가 바뀌었을 때 탑승 셀이 여전히 문 앞인지 확인하는 용도다.
	 * 본 이름에 Door가 들어간 것을 L/R, front/back을 뗀 이름으로 묶어 평균을 낸다. 본의
	 * 피벗이 문짝 가운데가 아니면 값이 틀릴 수 있으니 참고로만 쓴다.
	 */
	UFUNCTION(CallInEditor, Category = "Train|Art", meta = (DisplayName = "Log Door Bone Offsets"))
	void LogDoorBoneOffsets();

	// ---------------------------------------------------------------- 생명주기

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	// ---------------------------------------------------------------- 연출 훅

	/**
	 * 문이 열리는 동안 매 틱 불린다. Alpha는 0(닫힘)에서 1(완전히 열림)까지 간다.
	 *
	 * 기본 구현은 ArtMesh의 DoorOpenAnimation을 Alpha 위치로 짚는다. 스켈레탈 메시나
	 * 애니메이션이 없으면 아무것도 하지 않는다.
	 *
	 * 승하차를 막는 것은 이 훅이 아니라 DoorsOpening 단계 자체다. 훅을 비워 두든 갈아 끼우든
	 * 규칙이 흔들리지 않는다 -- CanBoard와 자동 하차는 단계만 본다.
	 *
	 * BlueprintNativeEvent다. 블루프린트가 이 이벤트를 구현하면 **부모 호출을 넣어야** 기본
	 * 구현(애니메이션 짚기)이 계속 돈다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Train")
	void AnimateDoorsOpening(float Alpha);
	virtual void AnimateDoorsOpening_Implementation(float Alpha);

	/**
	 * 문이 닫히는 동안 매 틱 불린다. Alpha는 0(열림)에서 1(완전히 닫힘)까지 간다.
	 *
	 * 기본 구현은 DoorCloseAnimation을 짚고, 그것이 없으면 DoorOpenAnimation을 거꾸로 짚는다.
	 * 열리는 쪽과 같은 규칙이다: 승하차 차단은 단계가 맡는다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Train")
	void AnimateDoorsClosing(float Alpha);
	virtual void AnimateDoorsClosing_Implementation(float Alpha);

private:
	/** 열차가 무엇을 하고 있는지. */
	enum class ETrainPhase : uint8
	{
		/** 시작 대기. 첫 역에 아직 들어오지 않았다. */
		Idle,

		/** 다음 역으로 이동 중. */
		Moving,

		/** 정차했고 문이 열리는 중. 아직 타거나 내릴 수 없다. */
		DoorsOpening,

		/** 정차해 문이 완전히 열려 있다. 타고 내릴 수 있는 유일한 단계다. */
		DoorsOpen,

		/** 문이 닫히는 중. 탈 수 없다. */
		DoorsClosing
	};

	void EnterMoving();
	void EnterDoorsOpening();
	void EnterDoorsOpen();
	void EnterDoorsClosing();

	/** 지금 단계가 얼마나 진행됐는지 0~1로. Duration이 0이면 곧바로 1이다. */
	float GetPhaseAlpha(float Duration) const;

	/** 정차역의 월드 위치. XY는 셀에서, Z는 배치된 높이에서 온다. */
	FVector GetStopLocation(int32 StopIndex) const;

	int32 GetNextStopIndex() const;

	/** 문 위치에서 승강장 쪽 탑승 셀을 계산한다. 저작 값이 없을 때만 쓴다. */
	void ComputeBoardingCells(int32 StopIndex, TArray<FIntPoint>& OutCells) const;

	/** 문 중심의 로컬 X 오프셋. 저작 값이 없으면 프록시 문 두 짝 자리를 돌려준다. */
	void GetDoorOffsets(TArray<float>& OutOffsets) const;

	/** 구간 진행도 0~1에서의 속도 배율. 커브가 있으면 커브, 없으면 기본 가감속. */
	float GetSpeedFactor(float Alpha) const;

	/** 좌석이 차체 중심에서 진행축으로 벗어날 수 있는 최대 거리(cm). */
	double GetSeatHalfExtent() const;

	/** ArtMesh에 스켈레탈 메시가 지정돼 있어 애니메이션을 돌릴 수 있으면 true. */
	bool HasArtMesh() const;

	/** 문 애니메이션을 Alpha(0~1) 위치로 짚는다. bReverse면 끝에서부터 짚는다. */
	void ScrubDoorAnimation(UAnimSequenceBase* Animation, float Alpha, bool bReverse);

	/** 닫힘에 쓸 애니메이션. 닫힘이 없으면 열림을 돌려주고 bOutReverse를 켠다. */
	UAnimSequenceBase* GetDoorClosingAnimation(bool& bOutReverse) const;

	/** 애니메이션이 RateScale을 반영해 실제로 걸리는 시간(초). */
	static float GetAnimationDuration(const UAnimSequenceBase* Animation);

	/** 바퀴 애니메이션을 반복 재생한다. 달리기 시작할 때 부른다. */
	void StartWheelAnimation();

	/** 지금 속도에 맞춰 바퀴 애니메이션의 PlayRate를 맞춘다. */
	void UpdateWheelPlayRate(double CurrentSpeed);

	/** 블루프린트가 붙인 아트 메시의 빈 슬롯에 ArtFallbackMaterial을 씌운다. */
	void ApplyArtFallbackMaterial();

	void RefreshVisual();

	/** 문 슬랩 색으로 열림·닫힘을 보여 준다. */
	void RefreshDoorLook();

	UPROPERTY(VisibleAnywhere, Category = "Train")
	TObjectPtr<USceneComponent> SceneRoot;

	/**
	 * 차체. Visibility를 막아 커서에 잡힌다.
	 *
	 * 폰과는 부딪히지 않는다. 폰에는 콜리전이 전혀 없고 위치는 전적으로 그리드가 정한다.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Train")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** 승강장 쪽 면에 붙는 문 표시. 열림·닫힘에 따라 머티리얼이 바뀐다. */
	UPROPERTY(VisibleAnywhere, Category = "Train")
	TArray<TObjectPtr<UStaticMeshComponent>> DoorMeshes;

	UPROPERTY(Transient)
	TObjectPtr<AGridActor> Grid;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DoorClosedMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DoorOpenMaterial;

	TWeakObjectPtr<AGridPawn> Rider;

	/**
	 * 방금 내린 폰. 그리드에 다시 설 때까지만 들고 있는다.
	 *
	 * 하차는 두 걸음이다: 차체에서 승강장으로 걸어 나오고, 그다음 지정된 자리까지 걸어간다.
	 * 두 번째 걸음은 폰이 셀 위에 다시 서기 전에는 시킬 수 없다.
	 */
	TWeakObjectPtr<AGridPawn> UnloadedPawn;

	/** 방금 내린 폰이 그리드에 다시 선 뒤 걸어갈 셀. 없으면 (-1,-1). */
	FIntPoint UnloadedGoalCell = FIntPoint(-1, -1);

	/** 방금 내린 역의 GuideOnFirstMoveAfterExit. 폰이 그리드에 다시 서면 가이드 서브시스템에 걸어 둔다. */
	EGuideType UnloadedGuide = EGuideType::None;

	/**
	 * ArtMesh에 마지막으로 지정한 애니메이션.
	 *
	 * SetAnimation은 재생 위치를 0으로 되돌린다. 매 틱 부르면 문이 짚은 위치에서 매번 처음으로
	 * 튀므로, 애니메이션이 바뀔 때만 부르기 위해 기억한다.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimationAsset> CurrentArtAnimation;

	/**
	 * 지금 구간의 출발점과 도착점, 그리고 지금까지 온 거리.
	 *
	 * 정속 이동이던 시절에는 매 프레임 현재 위치에서 목표로 조금씩 가면 그만이었지만,
	 * 가감속은 "구간의 몇 퍼센트를 왔는가"를 알아야 하므로 구간을 기억해야 한다.
	 */
	FVector MoveFrom = FVector::ZeroVector;
	FVector MoveTo = FVector::ZeroVector;
	double MoveLength = 0.0;
	double MoveDistance = 0.0;

	ETrainPhase Phase = ETrainPhase::Idle;

	/** 향해 가는(또는 서 있는) 정차역. */
	int32 CurrentStop = 0;

	/** 열차가 서는 높이. BeginPlay에서 배치된 Z를 기록한다. */
	double TrackZ = 0.0;

	// ---------------------------------------------------------------- 사운드 상태

	/** 남은 거리를 보고 알림음·진입음·정차음을 구간마다 한 번씩 낸다. Moving 틱마다 부른다. */
	void UpdateArrivalSounds();

	/** 플레이어가 탄 채 이동하는 동안의 루프를 멈춘다. */
	void StopInsideSound(float FadeOutSeconds);

	TWeakObjectPtr<UAudioComponent> ApproachAudio;
	TWeakObjectPtr<UAudioComponent> InsideAudio;

	/** 이번 구간에서 도착 소리를 이미 냈는지. EnterMoving에서 비운다. */
	bool bNotificationPlayed = false;
	bool bApproachPlayed = false;
	bool bStopSoundPlayed = false;

	float PhaseTimer = 0.0f;

	bool bDoorsLookOpen = false;
};
