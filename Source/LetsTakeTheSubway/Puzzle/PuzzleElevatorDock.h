// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Grid/GridTypes.h"
#include "Puzzle/PuzzleFloorTile.h"
#include "PuzzleElevatorDock.generated.h"

class AGridPawn;
class APuzzleElevatorBlock;
class UMaterialInterface;

/** 엔딩 승강기가 멈춰 섰다. 연출을 여기에 매단다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDockClearCutscene,
	APuzzleElevatorDock*, Dock, APuzzleElevatorBlock*, Elevator, APawn*, Pawn);

/**
 * 엘리베이터를 올려놓으면 층을 오갈 수 있게 되는 바닥 구조물.
 *
 * 기획의 "엘리베이터 구조물"이다. 엘리베이터 자체는 어디서나 밀 수 있지만 Z축 이동은 오직
 * 이 위에서만 일어난다. 그래서 퍼즐의 목표가 분명해진다: 차체를 여기까지 밀어 오는 것.
 *
 * **어디로 가는가**는 숫자도 셀 번호도 아닌 **다른 구조물**이다. 층마다 구조물을 하나씩,
 * 샤프트의 같은 XY에 놓고 서로를 TargetDock으로 가리킨다. 그러면 목표 높이도 내릴 자리도
 * 그 구조물에서 나오고, 올라가는 것과 내려가는 것이 같은 규칙 하나가 된다. 레벨에서 할 일은
 * 구조물을 층에 하나씩 놓는 것뿐이라 셀 번호를 셀 일도, 층고를 손으로 적을 일도 없다.
 *
 * 구조물 하나가 왕복을 담당하던 구버전 저작(TargetFloorCell·TravelHeight)은 TargetDock이
 * 비어 있을 때의 예비로 남아 있다.
 *
 * 같은 XY에 구조물이 여럿이므로 **도킹 판정에 층이 들어간다**: 자기 층에 있는 차체만 자기
 * 것으로 친다(FullyContains). 구조물 자신의 층은 배치된 Z와 둘레 셀의 바닥에서 정한다
 * (ResolveFloorZ) -- 샤프트 셀은 아래층 높이로 구워지므로 셀에서는 위층을 알 수 없다.
 *
 * 단일 그리드로 충분한 이유: 샤프트 셀은 아래층 바닥으로, 출구 셀은 위층 바닥으로 구워지고
 * 둘은 단차가 커서 서로 이어지지 않는다. 즉 걸어서는 층을 오갈 수 없고 차체만이 통로가 된다.
 * 전제는 위층 바닥이 아래층 걸을 수 있는 바닥과 XY로 겹치지 않는 것이다.
 */
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))
class LETSTAKETHESUBWAY_API APuzzleElevatorDock : public APuzzleFloorTile
{
	GENERATED_BODY()

public:
	APuzzleElevatorDock();

	// ---------------------------------------------------------------- 저작

	/**
	 * 이 구조물에서 출발한 차체가 도착할 구조물.
	 *
	 * 층마다 구조물을 하나씩 놓고 서로를 가리키게 하는 것이 이 시스템의 저작 방식이다.
	 * 목표 높이도, 내릴 자리도, 올라가는지 내려가는지도 전부 여기서 나온다 -- 레벨에서
	 * 할 일은 구조물 둘을 층에 하나씩 놓고 이 칸을 채우는 것뿐이다.
	 *
	 * **한쪽만 채워도 된다.** 비어 있으면 나를 가리키는 구조물을 찾아 그것을 목표로 삼는다.
	 * 왕복은 대칭이므로 양쪽에 같은 내용을 적게 하는 것은 틀릴 기회만 늘린다.
	 *
	 * 비어 있고 나를 가리키는 것도 없으면 아래 Legacy 값으로 물러선다.
	 */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock")
	TObjectPtr<APuzzleElevatorDock> TargetDock;

	/**
	 * 이 구조물 층에 도착했을 때, 차체 문 앞에 설 자리가 없으면 내려놓을 방향.
	 *
	 * 마지막 수단이다. 보통은 문 앞 셀 중 좌석에 가장 가까운 칸으로 내린다.
	 */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock")
	EGridDirection ExitDirection = EGridDirection::East;

	UPROPERTY(EditAnywhere, Category = "Elevator Dock", meta = (ClampMin = 1.0))
	float TravelSpeed = 200.0f;

	/** 도착한 뒤 폰이 내리기 시작하기까지의 시간(초). 문이 열리는 사이다. */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock", meta = (ClampMin = 0.0))
	float DoorDwellSeconds = 0.5f;

	// ---------------------------------------------------------------- 스테이지 클리어
	//
	// 엔딩 승강기는 층을 오가는 승강기가 아니다. 갈 층도, 내릴 바닥도 없다. 정해진 높이만큼
	// 움직이다 멈추고, 그 순간부터 연출이 화면을 가져간다. 그래서 엘리베이터 클래스를 따로
	// 만들지 않고 구조물에 스위치 하나를 두는 쪽을 골랐다 -- 같은 차체, 같은 조작, 같은 퍼즐이고
	// 달라지는 것은 "이 구조물에 올려놓고 타면 어떻게 되는가"뿐이다.

	/**
	 * 이 구조물이 **엔딩 승강기**인지.
	 *
	 * 켜지면 짝 Dock도, 구버전 목표 셀도 필요 없다. 차체가 이 구조물 위에 완전히 겹쳐 있기만
	 * 하면 탈 수 있고, ClearTravelHeight만큼 움직인 뒤 연출로 넘어간다.
	 *
	 * BlueprintReadWrite인 이유는 클리어 판정이 기획마다 다르기 때문이다. 아래
	 * bClearOnStageClear로 그리드에 맡길 수도 있고, 블루프린트가 직접 켤 수도 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Elevator Dock|Stage Clear")
	bool bIsClear = false;

	/**
	 * 그리드의 StageClear 셀을 밟으면 bIsClear를 자동으로 켤지.
	 *
	 * 블루프린트를 하나도 만들지 않고 "클리어 셀 -> 엔딩 승강기"를 잇는 가장 짧은 길이다.
	 * 레벨에 구조물이 여럿이어도 이것을 켠 구조물만 반응하므로, 어느 승강기가 출구인지가
	 * 레벨에서 그대로 읽힌다.
	 */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock|Stage Clear")
	bool bClearOnStageClear = false;

	/** 연출이 시작되기까지 차체가 움직일 거리(cm). */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock|Stage Clear", meta = (ClampMin = 1.0))
	float ClearTravelHeight = 1500.0f;

	/** 엔딩 승강기가 올라갈지 내려갈지. */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock|Stage Clear")
	bool bClearTravelUp = true;

	/**
	 * 엔딩 승강기의 속도(cm/s). 0이면 TravelSpeed를 쓴다.
	 *
	 * 따로 두는 이유는 연출의 속도가 이동의 속도와 다르기 때문이다. 퍼즐을 푸는 승강기는
	 * 빨라야 답답하지 않지만, 엔딩은 천천히 올라가는 편이 낫다.
	 */
	UPROPERTY(EditAnywhere, Category = "Elevator Dock|Stage Clear", meta = (ClampMin = 0.0))
	float ClearTravelSpeed = 0.0f;

	/**
	 * 엔딩 승강기가 멈춰 섰다. 페이드·시퀀스·씬 전환을 여기에 구현한다.
	 *
	 * 네이티브 기본 구현은 로그만 남긴다. 열차의 문 연출 훅과 같은 규칙이다: 규칙은 C++가
	 * 쥐고 있고 연출은 비어 있어도 게임이 성립한다.
	 *
	 * **차체와 폰을 파괴하거나 언포제스하지 말 것.** 폰은 차체가 사라지면 스스로 그리드로
	 * 내려서고, 차체는 승객이 사라지면 승강을 끝낸다. 레벨을 통째로 여는 씬 전환은 월드가
	 * 함께 사라지므로 상관없다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Elevator Dock|Stage Clear")
	void OnStageClearCutscene(APuzzleElevatorBlock* Elevator, APawn* Rider);
	virtual void OnStageClearCutscene_Implementation(APuzzleElevatorBlock* Elevator, APawn* Rider);

	/**
	 * 위와 같은 순간에 함께 발생한다. 구조물을 블루프린트로 파생하지 않고도 레벨 블루프린트가
	 * 연출을 붙일 수 있게 한다. 둘 다 한 번만 발생한다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Elevator Dock|Stage Clear")
	FOnDockClearCutscene OnClearCutscene;

	// ---------------------------------------------------------------- 구버전 저작
	//
	// 구조물 하나가 왕복을 담당하던 시절의 값들이다. TargetDock이 비어 있을 때만 쓴다.
	// 지우지 않는 이유는 테스트 맵(FlowTest_1, GreyBoxTest_1)이 이 방식으로 저작돼 있고,
	// 구조물 하나로 충분한 배치에서는 여전히 가장 짧은 저작이기 때문이다.

	/**
	 * 도착 층에서 폰이 내릴 셀. 이 셀의 바닥 높이가 곧 목표 Z다.
	 *
	 * 유효하지 않은 셀(기본값 -1,-1)이면 TravelHeight로 물러선다. Detect Target Floor
	 * 버튼이 트레이스로 채워 준다.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Elevator Dock|Legacy")
	FIntPoint TargetFloorCell = FIntPoint(-1, -1);

	/** TargetFloorCell이 없을 때 쓰는 이동 거리(cm). 기획의 8 m가 기본값이다. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Elevator Dock|Legacy", meta = (ClampMin = 0.0))
	float TravelHeight = 800.0f;

	/** TravelHeight로 물러섰을 때 올라갈지 내려갈지. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Elevator Dock|Legacy")
	bool bTravelUp = true;

	/**
	 * 목적 층 셀을 트레이스로 찾아 TargetFloorCell에 채운다.
	 *
	 * 구조물 중심에서 ExitDirection으로 한 칸 바깥 지점을 위아래로 훑어, 지금 바닥에서
	 * 가장 가까운 다른 층 바닥을 고른다. 손으로 셀 번호를 세는 것보다 정확하다.
	 */
	UFUNCTION(CallInEditor, Category = "Elevator Dock|Legacy", meta = (DisplayName = "Detect Target Floor"))
	void DetectTargetFloor();

	// ---------------------------------------------------------------- 조회

	/** 지금 이 구조물 위에 완전히 올라와 있는 엘리베이터. 없으면 null. */
	APuzzleElevatorBlock* GetDockedElevator() const { return DockedElevator.Get(); }

	/**
	 * 이 구조물의 짝. 지정된 TargetDock이 있으면 그것, 없으면 나를 가리키는 구조물.
	 *
	 * 둘 다 없으면 null이고, 그때는 구버전 저작(TargetFloorCell·TravelHeight)으로 돈다.
	 */
	APuzzleElevatorDock* GetTargetDock() const;

	/** 이 구조물이 엔딩 승강기로 동작하는 중이면 true. */
	UFUNCTION(BlueprintPure, Category = "Elevator Dock|Stage Clear")
	bool IsClearRide() const { return bIsClear; }

	/**
	 * 목표 Z.
	 *
	 * 짝이 있으면 그 구조물의 층이다. 없으면 구버전 규칙으로 왕복한다: 구조물 층에 있으면
	 * 반대편 층으로, 반대편 층에 있으면 구조물 층으로.
	 */
	double ComputeTargetZ(const APuzzleElevatorBlock& Elevator) const;

	/**
	 * 폰과 무관하게, 지금 이 구조물이 차체를 보낼 수 있는지.
	 *
	 * 컨트롤러가 먼저 묻는다. 탈 수 없는 엘리베이터라면 폰을 문 앞까지 걸어 보내 놓고
	 * 거기서 거절하는 것보다, 누른 자리에서 바로 이유를 알려 주는 편이 낫다.
	 */
	bool CanLaunch(FText* OutReason = nullptr) const;

	// ---------------------------------------------------------------- 사용

	/**
	 * 도킹된 차체에 폰을 태워 층을 옮긴다. 못 하면 이유를 알려준다.
	 *
	 * 컨트롤러가 엘리베이터 클릭(TryBoard)에 성공한 뒤 이어서 부른다. 구조물 위가 아닌
	 * 엘리베이터를 클릭하면 여기까지 오지 않고 "구조물 위로 먼저"라는 안내만 뜬다.
	 */
	bool TryLaunch(AGridPawn* Pawn, FText* OutReason = nullptr);

	/**
	 * 이 구조물이 담당하는 층에 있는 차체만 자기 것으로 친다.
	 *
	 * 짝을 이룬 구조물은 같은 XY를 층마다 나눠 가지므로(샤프트), XY만 보면 아래층 구조물이
	 * 위층에 걸린 차체를 자기 것이라 주장해 플레이어를 엉뚱한 층으로 보낸다.
	 */
	virtual bool FullyContains(const APuzzleBlock& Block) const override;

	virtual bool IsBusy() const override;
	virtual void OnBlockCameToRest(APuzzleBlock& Block) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual FString DescribeTile() const override;
	virtual void OnTileReady() override;
	virtual double ResolveFloorZ(const AGridActor& InGrid, FIntPoint MinCell, double PlacedZ) const override;
	virtual bool CanCoexistWith(const APuzzleFloorTile& Other) const override;

private:
	/** 구조물 반대편 층의 바닥 높이. TargetFloorCell이 있으면 그 셀에서, 없으면 TravelHeight로. */
	double GetFarFloorZ() const;

	/**
	 * 목표 높이에서 폰이 내릴 월드 위치.
	 *
	 * SeatWorld는 폰이 차 안에서 서 있는 자리다. 문 앞에 설 자리가 여럿이면 그중 좌석에
	 * 가장 가까운 칸을 고른다 -- 탈 때와 마찬가지로 폰이 문으로 똑바로 걸어 나가게 한다.
	 */
	bool FindExitWorld(const APuzzleElevatorBlock& Elevator, double TargetZ,
		const FVector& SeatWorld, FVector& OutWorld) const;

	/** 도착 층(이 구조물 쪽)에서 차체 문 앞의 내릴 자리. 없으면 false. */
	bool FindArrivalExit(const APuzzleElevatorBlock& Elevator, double TargetZ,
		const FVector& SeatWorld, FVector& OutWorld) const;

	/** 지금 완전히 올라와 있는 엘리베이터를 다시 찾는다. */
	APuzzleElevatorBlock* FindElevatorOnTop() const;

	/** 도킹 여부에 따라 패드 머티리얼을 바꾼다. */
	void RefreshDockedLook();

	/**
	 * 첫 틱에 짝과 이동 거리를 한 줄로 남기고, 짝이 같은 층이면 경고한다.
	 *
	 * BeginPlay가 아니라 첫 틱인 이유는 액터의 BeginPlay 순서가 정해져 있지 않기 때문이다.
	 * 먼저 도는 구조물은 아직 층을 정하지 못한 짝을 Z 0으로 읽는다.
	 */
	void LogPairingOnce();

	/** 엔딩 승강기가 목표 높이에 멈춰 섰다. 연출을 한 번만 발동한다. */
	UFUNCTION()
	void HandleHoldReached(APuzzleElevatorBlock* Elevator, APawn* Pawn);

	/** bClearOnStageClear일 때 그리드의 StageClear 셀 이벤트를 받는다. */
	UFUNCTION()
	void HandleStageClear(APawn* Pawn, FIntPoint Cell);

	TWeakObjectPtr<APuzzleElevatorBlock> DockedElevator;

	/**
	 * TargetDock이 비어 있을 때 역으로 찾아낸 짝. 게임 중에 한 번만 훑는다.
	 *
	 * 에디터에서는 캐시하지 않는다 -- 디자이너가 방금 반대쪽에 값을 넣었을 수 있다.
	 */
	mutable TWeakObjectPtr<APuzzleElevatorDock> ReverseTargetDock;

	mutable bool bReverseTargetResolved = false;

	bool bPairingLogged = false;

	/** 연출은 한 번뿐이다. 같은 차체를 다시 태워도 두 번 부르지 않는다. */
	bool bClearCutsceneFired = false;

	/** 비어 있을 때의 패드 머티리얼. 생성자에서 잡아 둔다. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> IdleMaterial;

	/** 차체가 올라와 있을 때의 패드 머티리얼. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ReadyMaterial;

	bool bLookIsReady = false;
};
