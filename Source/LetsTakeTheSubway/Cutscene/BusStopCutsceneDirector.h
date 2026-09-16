// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BusStopCutsceneDirector.generated.h"

class ACameraActor;
class AGridPawn;
class ALevelSequenceActor;
class APlayerController;
class ULevelSequencePlayer;
class USkeletalMeshComponent;
class UWorld;

/** 이 맵의 컷씬이 타는 장면인지 내리는 장면인지. */
UENUM(BlueprintType)
enum class EBusStopCutsceneMode : uint8
{
	/** 정류장에 선 폰이 버스에 탄다. */
	Board,

	/** 버스에 타고 온 폰이 정류장에 내린다. */
	Alight
};

/** 버스의 어느 문을 쓰는지. SM_BUS_001의 본 이름 쌍으로 이어진다. */
UENUM(BlueprintType)
enum class EBusDoor : uint8
{
	/** Door_Front_A / Door_Front_B */
	Front,

	/** Door_Middle_A / Door_Middle_B */
	Middle
};

/**
 * 버스 정류장 컷씬 진행자.
 *
 * 연출(버스 이동, 문과 바퀴 애니메이션, 카메라 컷)은 레벨 시퀀스가 갖고, 이 액터는 논리만 맡는다:
 *   1. 검은 화면에서 페이드 인하며 시퀀스를 마크 프레임(PauseMarkLabel)까지 재생한다.
 *   2. 시퀀스가 멈춘 동안 폰을 문으로 태우거나 내린다. 규칙은 지하철(AGridTrain::TryBoard)과 같다:
 *      문 한가운데와 일직선이 되는 자리로 먼저 옮겨 선 뒤 차체 면에 수직으로 들어가고, 높이는 폰이 서
 *      있던 높이 그대로다.
 *   3. 폰이 다 타거나 내리면 시퀀스를 끝까지 재생한다.
 *   4. 검게 페이드 아웃하고 NextLevel을 연다.
 *
 * 문 위치를 시퀀스에 키로 박지 않고 여기서 계산하는 이유: 버스 메시나 문 본이 바뀌어도 탑승 경로가 따라
 * 바뀌어야 하기 때문이다. 아트는 시퀀서에서 타이밍과 카메라만 만지면 된다.
 *
 * 시퀀스 편집 규칙은 Docs/Plans/BusStopCutscene.md 참고(섹션 KeepState, 문 섹션 겹침 금지, 마크 이름 유지).
 */
UCLASS()
class LETSTAKETHESUBWAY_API ABusStopCutsceneDirector : public AActor
{
	GENERATED_BODY()

public:
	ABusStopCutsceneDirector();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** 타는 장면인지 내리는 장면인지. */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene")
	EBusStopCutsceneMode Mode = EBusStopCutsceneMode::Board;

	/**
	 * 레벨에 배치한 시퀀스 액터. 시퀀스 애셋과 재생 설정은 그 액터의 Details에서 고른다.
	 *
	 * 권장 재생 설정: Auto Play 끔, Pause at End 켬, Finish Completion State Override = Force Keep State.
	 * Auto Play가 켜져 있으면 이 액터가 시작하기 전에 시퀀스가 먼저 돌아 버린다.
	 */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene")
	TSoftObjectPtr<ALevelSequenceActor> SequenceActor;

	/**
	 * 시퀀스를 여기까지 재생하고 멈춘 뒤 승하차한다. 시퀀스의 마크 프레임 이름.
	 *
	 * 문 열림 섹션이 끝나고 문 닫힘 섹션이 시작하는 프레임에 찍는다. 그 프레임 직전에서 멈추므로 문이 다
	 * 열린 포즈로 기다린다. 마크가 없으면 시퀀스 끝에서 승하차하고 곧바로 페이드 아웃한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene")
	FName PauseMarkLabel = TEXT("Doors");

	/** 버스 액터. 스켈레탈 메시 컴포넌트(SM_BUS_001)에서 문 본 위치를 읽는다. */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene")
	TSoftObjectPtr<AActor> Bus;

	/** 이 장면에서 쓰는 문. */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene")
	EBusDoor Door = EBusDoor::Front;

	/**
	 * 내린 뒤 걸어갈 곳(Alight 전용). XY만 쓰고 높이는 폰의 높이 그대로다.
	 *
	 * 비워 두면 문 바로 앞에서 멈춘다.
	 */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene", meta = (EditCondition = "Mode == EBusStopCutsceneMode::Alight"))
	TSoftObjectPtr<AActor> ExitPoint;

	/**
	 * 시작할 때 볼 카메라(선택). 시퀀스에 카메라 컷 트랙이 있으면 곧바로 그쪽으로 덮인다.
	 *
	 * 카메라 컷이 없는 시퀀스를 쓸 때 폰 시점으로 보이지 않게 하려는 안전장치다.
	 */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene")
	TSoftObjectPtr<ACameraActor> ViewCamera;

	/** 컷씬이 끝나면 열 레벨. */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene")
	TSoftObjectPtr<UWorld> NextLevel;

	/** 시작할 때 검정에서 밝아지는 시간(초). */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene", meta = (ClampMin = 0.0))
	float FadeInSeconds = 1.0f;

	/** 끝날 때 검정으로 어두워지는 시간(초). 다 어두워진 뒤 NextLevel을 연다. */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene", meta = (ClampMin = 0.0))
	float FadeOutSeconds = 1.0f;

	/** 좌석이 차체 끝에서 떨어져야 하는 거리(cm). 좌석은 차체 반길이에서 이 값을 뺀 범위 안에 선다. */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene", meta = (ClampMin = 0.0))
	float SeatEndMargin = 100.0f;

	/** 내릴 때 문 바로 앞 경유점이 차체 옆면에서 떨어진 거리(cm). 공 반지름보다 커야 차체를 스치지 않는다. */
	UPROPERTY(EditAnywhere, Category = "Bus Stop Cutscene", meta = (ClampMin = 0.0))
	float DoorClearance = 80.0f;

	/** 에디터 버튼: 앞문·중간문의 위치(차체 중심에서 진행축·옆 방향 cm)를 출력 로그에 찍는다. */
	UFUNCTION(CallInEditor, Category = "Bus Stop Cutscene")
	void MeasureDoor();

	/** 대기와 페이드를 건너뛰고 곧바로 NextLevel을 연다. 콘솔 ltts.BusCutscene Skip. */
	void SkipToNextLevel();

private:
	enum class EPhase : uint8
	{
		/** 폰과 컨트롤러가 준비되기를 기다린다. */
		WaitingForPlayer,

		/** 마크 프레임까지 재생 중. */
		Intro,

		/** 시퀀스가 멈춘 채 폰이 타거나 내리는 중. */
		Transfer,

		/** 마크 뒤를 끝까지 재생 중. */
		Outro,

		/** 페이드 아웃 뒤 레벨을 여는 중. */
		Leaving
	};

	/** 버스의 문·차체 치수를 월드 기준으로 모은 것. */
	struct FBusFrame
	{
		FVector Centre = FVector::ZeroVector;
		/** 진행축(수평, 단위). 스켈레탈 메시의 로컬 +Y. */
		FVector Axis = FVector::ForwardVector;
		/** 문이 달린 옆면 쪽(수평, 단위). */
		FVector Side = FVector::RightVector;
		/** 두 문짝 본의 평균(월드). */
		FVector DoorCentre = FVector::ZeroVector;
		/** 문 중심의 진행축 위치(cm, 차체 중심 0). */
		double DoorAlong = 0.0;
		/** 문 중심이 차체 중심선에서 Side 쪽으로 떨어진 거리(cm). */
		double DoorLateral = 0.0;
		double HalfLength = 0.0;
		double HalfWidth = 0.0;
		bool bDoorFound = false;
	};

	bool ResolvePlayer();
	void StartIntro();
	void StartTransfer();
	void StartOutro();
	void StartFadeOut();
	void OpenNextLevel();

	/** 시작할 때 폰을 버스 안 좌석에 앉힌다(Alight). */
	void SeatPawnInBus();

	USkeletalMeshComponent* FindBusMesh() const;
	ULevelSequencePlayer* GetPlayer() const;

	/**
	 * 문 본을 레퍼런스 포즈에서 읽는다. 문이 열려 있든 닫혀 있든 같은 값이 나온다.
	 *
	 * @param SideHint  문 본이 차체 중심선 위에 있어 옆면을 가릴 수 없을 때, 이 점이 있는 쪽을 문 쪽으로 본다.
	 */
	bool ComputeBusFrame(EBusDoor WhichDoor, const FVector& SideHint, FBusFrame& Out) const;

	UFUNCTION()
	void HandleSequenceFinished();

	void LockPlayer(bool bLock);

	EPhase Phase = EPhase::WaitingForPlayer;

	TWeakObjectPtr<AGridPawn> Pawn;
	TWeakObjectPtr<APlayerController> Controller;

	/** 마크까지 재생을 건 뒤 플레이어가 한 번이라도 재생 중이었는지. 재생을 걸기 전의 정지 상태를 멈춤으로 착각하지 않게 한다. */
	bool bSawPlaying = false;

	/** 폰이 GridPawn이 아니라고 이미 알렸는지. */
	bool bWarnedWrongPawn = false;

	/** 시퀀스가 끝(Pause at End)에 닿았는지. */
	bool bSequenceEnded = false;

	/** 마크 프레임이 없어 시퀀스 끝을 멈춤 지점으로 쓰는지. */
	bool bPauseAtEndInsteadOfMark = false;

	FTimerHandle OpenLevelTimer;

};
