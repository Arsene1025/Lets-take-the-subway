# UI 연결 계획 — PathUI 구역 인덱스 · GuidePopUpUI 트리거

작성일 2026-09-16. 출처: 사용자 요청(채팅) + 기획 스프레드시트 「가이드 팝업 트리거 표 / 레벨 리스트」
(Google Sheets `1OlSYHXugTgG3lVUdvQL2Xj7Dzp6yFrOW`, gid 414237740) + `GuideType.h`.
선행 문서: `Docs/StageZone.html` 4절(UI 매니저가 스테이지 서브시스템에 바인딩하는 계약),
`StageRailExtension.md`(Stage2 구역 8개), `Subway_Stage1.md` 9.9절(구역 카메라).

---

## 0. 요청 해석과 현재 상태

| 요청 | 지금 코드 | 해야 할 일 |
|---|---|---|
| `ShowOrRefreshPathUI`에 0~9 정수를 넘기고 위젯은 Switch On Int로 그림 선택 | 인자 없음. `Execute_Initialize`만 부른다. **호출하는 곳이 없다.** | 시그니처 변경 + 구역→인덱스 변환 + 구역 변경 때 자동 호출 |
| Stage1 카메라 1~5 → 0~4, Stage2 카메라 1·3·4·6·8 → 5~9 | 카메라는 `AStageInfo::ZoneCameras`(배열 0번 = 구역 1)로 구역 번호와 1:1 | 구역 번호 → PathUI 인덱스 표를 데이터로 둔다 |
| 스프레드시트 트리거대로 `ShowGuidePopUpUI(EGuideType)` 호출 | 함수는 있고 **호출하는 곳이 없다.** `EGuideType`은 Tutorial1·2, Stage1_1~3, Stage2_1 | 트리거 6개 중 튜토리얼 2개는 비워 두고 4개를 연결. 확장 지점을 남긴다 |

확인한 사실(2026-09-16, 에디터에 Subway_Stage1이 열린 상태에서 조회):

- `WBP_PathUI`는 이미 `InitializeInt`를 구현하고 `Switch On Int` 노드를 갖고 있다. C++만 바뀌면 된다.
- `WBP_GuidePopUpUI`는 `InitializeGuide`를 구현하고 `GuideDataSubsystem::GetEntry`로 제목·본문·이미지를 읽는다.
  `OpenGuidePopUp`/`CloseGuidePopUp` 함수가 있으나 `CallUIOpened`/`CallUIClosed`는 부르지 않는다(4.3절).
- Subway_Stage1의 `StageInfo_1`: StageIndex 1, **ZoneCount 4, ZoneCameras 4개**(CameraActor_2·1·3·4 순).
  요청은 "1~5번 카메라"라 **구역/카메라가 하나 부족하다.** 표를 데이터로 두므로 5번째 구역이 추가되면 값만 채우면 된다(6절 Q1).
- Subway_Stage2: 구역 볼륨 `StageZoneVolume`~`StageZoneVolume8`, 카메라 `CAM_Isometric1`~`8`, PIE 로그 `ZoneCount 8`.
  `StageInfo.ZoneCameras`에 8개가 들어 있는지는 그 맵을 열어 확인해야 한다(6절 Q2).
- `DA_GuideDatabase`의 항목이 **시트와 어긋나 있다**: `Tutorial2`에 [엘리베이터 회전], `Stage1_1`에 [올라가기 - 내려가기]가
  들어 있다. 시트는 Tutorial 2번째 행이 [올라가기 - 내려가기], "1스테이지 (1번 퍼즐)"이 [엘리베이터 회전]이다(2.1절 표).
  이 계획은 **시트 순서 = enum 순서**를 정본으로 보고, 데이터 애셋의 두 항목을 맞바꾸는 것을 작업에 포함한다.

---

## 1. PathUI — 구역 번호를 0~9 인덱스로

### 1.1 시그니처

```cpp
// UIManagerSubsystem.h
UFUNCTION(BlueprintCallable, Category = "UI")
void ShowOrRefreshPathUI(int32 PathIndex);
```

`uint32`가 아니라 **`int32`**로 한다. `UFUNCTION`/블루프린트는 `uint32`를 받지 못하고, `IUIInitializable::InitializeInt`도
`int32`다. 음수는 "그릴 그림 없음"(1.2절)으로 쓴다. 본문은 요청한 코드 그대로: 위젯을 만들거나 꺼내 Visible로 바꾸고
`IUIInitializable::Execute_InitializeInt(W, PathIndex)`를 부른다.

### 1.2 구역 → 인덱스 표는 `AStageInfo`에 둔다

세 가지를 비교했다.

| 안 | 내용 | 판단 |
|---|---|---|
| A. C++에 `if (Stage==1) return Zone-1; if (Stage==2) switch(Zone)…` | 가장 짧다 | 구역이 하나 늘거나 카메라 번호가 바뀔 때마다 재빌드. Stage1의 5번째 구역이 아직 없다는 점에서 이미 어긋난다 |
| B. `AStageZoneVolume`에 `PathUIIndex` | 플레이어가 들어가는 볼륨에 바로 적는다 | 한 구역을 박스 여러 개로 덮으면(Stage1) 같은 값을 여러 번 적어야 하고, 어긋나면 어느 것이 맞는지 알 수 없다 |
| C. **`AStageInfo::ZonePathUIIndices`** (`TArray<int32>`, 배열 n번 = 구역 n+1, `-1` = 갱신 안 함) | `ZoneCameras`와 나란히 놓여 "카메라 N번 = PathUI M번"이 디테일 패널에서 그대로 읽힌다 | **권장.** 요청이 카메라 번호로 표현된 것과 같은 자리 |

```cpp
// StageInfo.h — Category "Stage Info|Path UI"
/**
 * 구역마다 PathUI가 보여 줄 그림 번호. 배열 0번이 구역 1이다(ZoneCameras와 같은 규칙).
 * -1이면 그 구역에 들어가도 PathUI를 갱신하지 않는다(직전 그림 유지).
 * 비어 있거나 범위 밖이면 -1로 본다.
 */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Info|Path UI")
TArray<int32> ZonePathUIIndices;

UFUNCTION(BlueprintPure, Category = "Stage Info|Path UI")
int32 GetPathUIIndex(int32 ZoneIndex) const;   // 범위 밖·빈 칸이면 -1
```

레벨에 적을 값:

| 레벨 | 구역 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| Subway_Stage1 | 0 | 1 | 2 | 3 | 4 (구역 5가 생기면) | | | |
| Subway_Stage2 | 5 | −1 | 6 | 7 | −1 | 8 | −1 | 9 |

`ValidateLayout`(StageSubsystem)에 "배열 길이 ≠ ZoneCount"와 "같은 인덱스가 두 구역에" 경고를 한 줄씩 추가한다. 판정은 막지 않는다.

### 1.3 누가 언제 부르나 — UI 매니저가 스테이지 서브시스템에 바인딩

`Docs/StageZone.html` 4절의 계약을 그대로 구현한다. `UUIManagerSubsystem`은 `ULocalPlayerSubsystem`이라 레벨을 넘어 살아남고,
`UStageSubsystem`은 레벨마다 새로 생기므로 **`PlayerControllerChanged`마다 풀고 다시 바인딩**한다.

```cpp
// UIManagerSubsystem.h
virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
private:
    void BindStage(UStageSubsystem* Stage);
    void UnbindStage();
    UFUNCTION() void HandleStageStateChanged(const FStageState& State);
    TWeakObjectPtr<UStageSubsystem> BoundStage;
    int32 LastPathIndex = -1;          // 같은 그림을 두 번 그리지 않기 위해. HandleMapLoaded에서 -1로
    bool  bStageEntryHandled = false;  // 2.3절 S1. HandleMapLoaded에서 false로
```

`HandleStageStateChanged`:

1. `!State.bResolved`면 아무것도 안 한다(레벨 시작 판정 전).
2. `Info = Stage->GetStageInfo()`, `Index = Info ? Info->GetPathUIIndex(State.CurrentZoneIndex) : -1`.
3. `Index >= 0 && Index != LastPathIndex`면 `ShowOrRefreshPathUI(Index)`, `LastPathIndex = Index`.
4. 처음 Resolved를 본 순간이면 진입 가이드(2.3절 S1)를 요청한다.

레벨 시작 때 `OnStageStateChanged`가 반드시 한 번 오므로(`StageTypes.h` 주석) 첫 그림도 여기서 그려진다. `BindStage`는
이미 Resolved면 즉시 한 번 부른다(PIE 재시작 뒤 컨트롤러 교체 순서 대비). 튜토리얼 맵처럼 `AStageInfo`가 없는 레벨은
`StageIndex 0`, `Info == nullptr`라 PathUI를 건드리지 않는다.

`HandleMapLoaded`(이미 `PostLoadMapWithWorld`에 붙어 있다)에서 위젯을 지울 때 `LastPathIndex`·`bStageEntryHandled`도 초기화한다.

### 1.4 블루프린트 쪽(UI 담당)

`WBP_PathUI`의 `InitializeInt(paramInt)` → 기존 `Switch On Int`에 0~9 핀을 채우고 각 그림을 켠다. 이미 노드가 있으니
핀과 이미지 연결만 남았다. Default 핀은 아무것도 바꾸지 않는다(−1은 C++에서 걸러지므로 오지 않는다).

### 1.5 테스트용 콘솔 명령

`ltts.PathUI <Index>` — 구역을 걷지 않고 특정 그림을 띄운다. `ltts.ZoneCamera`와 같은 형식으로 `GridPlayerController.cpp`
하단의 콘솔 명령 묶음에 추가한다.

---

## 2. GuidePopUpUI — 스프레드시트 트리거

### 2.1 시트 ↔ enum ↔ 트리거 ↔ 구현

| # | 시트 행(배치 위치) | 제목 | 시트의 트리거 | `EGuideType` | 구현 (2.3절) |
|---|---|---|---|---|---|
| T1 | 튜토리얼 | [엘리베이터 이동] | 엘리베이터와 마주친 후 | `Tutorial1` | **비움.** 확장 지점: `AGuideCellTrigger`(S4)를 엘리베이터 문 앞 셀에 놓으면 된다 |
| T2 | 튜토리얼 | [올라가기 - 내려가기] | 1스테이지 진입 후 | `Tutorial2` | **비움.** 확장 지점: Stage1 `StageInfo.EntryGuides`에 추가(S1). 같은 순간에 둘이 뜨므로 큐(2.2절)가 먼저 필요 |
| S1 | 1스테이지 (1번 퍼즐) | [엘리베이터 회전] | 1스테이지 진입 후 | `Stage1_1` | `AStageInfo::EntryGuides` — 레벨 시작 판정 첫 방송 때 |
| S2 | 1스테이지 (지하철 승강장) | [지하철 탑승] | 지하철 승강장에 엘리베이터가 닿은 순간 | `Stage1_2` | `APuzzleElevatorDock::ArrivalGuide` — 승강장 도크에 차체가 도착했을 때 |
| S3 | 1스테이지 (2번 퍼즐) | [이동이 가능한 장애물] | 지하철에서 공이 내리고 플레이어가 이동키를 눌렀을 때 | `Stage1_3` | `FGridTrainStop::GuideOnFirstMoveAfterExit` — 하차 완료 뒤 첫 이동 입력 |
| S4 | 2스테이지 (1번 퍼즐) | [회전 기둥과 레버] | 개찰구 앞으로 이동했을 때 (어려우면 스테이지 진입 시) | `Stage2_1` | 신규 `AGuideCellTrigger`를 개찰구 앞 셀에 배치. 예비: Stage2 `EntryGuides` |

「레벨 리스트」시트는 1-1에 팝업 3개(밀기·회전판·주차), 2-1에 2개(회전기둥·장애물 걸림)라고 적혀 있어 트리거 표(6행)와
개수가 다르다. **enum이 6개이므로 트리거 표를 정본으로 한다.** 나중에 항목이 늘면 enum과 DB에 추가하고 같은 방식으로 건다.

### 2.2 공통 진입점 — `UGuideDataSubsystem::RequestGuide`

트리거는 액터(도크·열차·셀 트리거·StageInfo)에서 나오고 팝업은 로컬 플레이어 서브시스템이 띄운다. 액터마다 로컬 플레이어를
찾는 코드를 반복하지 않도록 **게임 인스턴스 서브시스템(`UGuideDataSubsystem`, 이미 있음)에 입구를 하나** 둔다.

```cpp
// GuideDataSubsystem.h
/** 가이드 팝업을 요청한다. 이미 보여 준 종류면 무시한다(세션 동안 한 번). None·MAXVALUE는 무시. */
UFUNCTION(BlueprintCallable, Category = "Guide", meta = (WorldContext = "WorldContext"))
void RequestGuide(const UObject* WorldContext, EGuideType Type);

/** 보여 준 기록을 지운다. 타이틀에서 새 게임을 시작할 때 부른다. */
UFUNCTION(BlueprintCallable, Category = "Guide")
void ResetShownGuides();

/** 다음 이동 입력 때 띄울 가이드를 걸어 둔다(S3). 이미 걸린 것이 있으면 덮어쓴다. */
void ArmGuideOnNextMoveInput(EGuideType Type);
/** 이동 입력이 왔다. 걸린 것이 있으면 RequestGuide하고 푼다. 컨트롤러가 부른다. */
void NotifyMoveInput(const UObject* WorldContext);
/** 레벨이 바뀌면 걸린 것을 푼다. */
void ResetArmedMoveGuide();

private:
    TSet<EGuideType> ShownGuides;
    EGuideType ArmedMoveGuide = EGuideType::None;
```

`RequestGuide` 본문: `ShownGuides`에 있으면 return → 추가 → `GetGameInstance()->GetFirstGamePlayer()`의
`UUIManagerSubsystem::ShowGuidePopUpUI(Type)`. 싱글 플레이 게임이라 첫 로컬 플레이어로 충분하다.
"한 번만"을 게임 인스턴스에 두는 이유: 같은 스테이지를 다시 열어도 이미 본 설명이 또 뜨지 않게 하려는 것이다. 다시 보고 싶으면
사이드 메뉴의 가이드 패널(`WBP_SideMenuUI`가 이미 `GetEntry`로 그린다)이 있다. 스테이지 재시작 때 다시 띄우고 싶으면
`ResetShownGuides`를 그때 부르면 된다(6절 Q4).

**큐(2차, 지금은 만들지 않음).** 같은 순간에 두 가이드가 요청되면(T2+S1) 뒤의 것이 앞의 것을 덮어쓴다(위젯 하나를 재사용하므로).
필요해지면 `UUIManagerSubsystem`에 `TArray<EGuideType> PendingGuides`를 두고 `OnUIClosed`에서 다음 것을 띄운다.
그러려면 `WBP_GuidePopUpUI`가 닫힐 때 `CallUIClosed`를 불러야 한다(4.3절).

### 2.3 트리거별 구현

**S1. 스테이지 진입 — `AStageInfo::EntryGuides`**

```cpp
// StageInfo.h — Category "Stage Info|Guide"
/** 레벨 시작 판정 직후 순서대로 요청할 가이드. 지금은 첫 항목만 보이고 나머지는 덮어쓴다(큐는 2차). */
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Info|Guide")
TArray<EGuideType> EntryGuides;
```

`UUIManagerSubsystem::HandleStageStateChanged`에서 처음 `bResolved`를 본 순간(`!bStageEntryHandled`) `Info->EntryGuides`를
차례로 `RequestGuide`. Stage1에 `[Stage1_1]`. 카메라가 먼저 붙고(`AGridPlayerController::HandleStageStateChanged`가 같은 방송을
받는다) 그다음 팝업이 뜬다. 순서는 바인딩 순서에 달렸으니 신경 쓰지 않아도 되도록, 팝업은 다음 틱에 띄운다
(`GetWorld()->GetTimerManager().SetTimerForNextTick`). 레벨 첫 프레임에 위젯을 만들면서 뷰포트가 아직 없을 때를 피하는 뜻도 있다.

**S2. 승강장 도착 — `APuzzleElevatorDock::ArrivalGuide`**

```cpp
// PuzzleElevatorDock.h — Category "Elevator Dock|Guide"
/** 다른 구조물에서 출발한 차체가 이 구조물 층에 도착해 폰이 내렸을 때 요청할 가이드. None이면 없음. */
UPROPERTY(EditAnywhere, Category = "Elevator Dock|Guide")
EGuideType ArrivalGuide = EGuideType::None;
```

흐름: `TryLaunch` 성공 → 출발 도크가 `Elevator->OnArrived`에 `HandleArrived`를 `AddUniqueDynamic` → `FinishTravel`이
`OnArrived(Elevator, Pawn)`를 방송 → 출발 도크가 `GetTargetDock()->ArrivalGuide`를 `RequestGuide`하고 바인딩을 푼다.
"닿은 순간"을 `OnArrived`(차체 정지 + 폰이 내려 조작 복귀)로 읽는다. `OnHoldReached`는 클리어 승강(허공 정지) 전용이라
쓰지 않는다. 출발 도크가 도착 도크의 값을 읽는 이유: 짝 도크는 `TargetDock` 하나로 이어져 있고, 도착 도크는 자기한테 차체가
"내려왔는지"를 매 틱 `FindElevatorOnTop`으로만 알 수 있어 순간을 잡기 어렵다.
Stage1에서는 B2 승강장 층의 도크(PuzzleElevatorDock·2·3 중 승강장 층 Z인 것)에 `Stage1_2`를 적는다. 구현 때 세 도크의 `GetFloorZ`를
찍어 고른다.

**S3. 하차 뒤 첫 이동 입력 — `FGridTrainStop::GuideOnFirstMoveAfterExit`**

```cpp
// GridTrain.h — FGridTrainStop
/** 이 정차역에서 플레이어가 내린 뒤 처음 이동 입력을 했을 때 요청할 가이드. None이면 없음. */
UPROPERTY(EditAnywhere, Category = "Guide")
EGuideType GuideOnFirstMoveAfterExit = EGuideType::None;
```

- `AGridTrain::Tick`의 "하차한 폰이 그리드에 다시 서면" 블록(`UnloadedPawn` 처리, GridTrain.cpp 1082행 부근)에서
  `Stops[UnloadedStopIndex].GuideOnFirstMoveAfterExit != None`이면 `GuideSubsystem->ArmGuideOnNextMoveInput(Type)`.
  하차 정차역 번호는 `UnloadedPawn`을 적을 때 함께 적어 둔다(`UnloadedStopIndex`).
  열차가 시키는 `RequestMoveToCell(UnloadedGoalCell)`은 입력이 아니므로 세지 않는다.
- `AGridPlayerController::OnMoveKeyPressed`와 클릭 이동(`OnReleased`에서 바닥 셀로 `RequestMoveToCell`하는 690행 경로)
  두 곳에서 `GuideSubsystem->NotifyMoveInput(this)`. 시트는 "이동키"라고 했지만 마우스로만 조작하는 플레이어가 설명을 못 보게 되므로
  **클릭 이동도 이동 입력으로 친다.** 키보드만 원하면 클릭 쪽 한 줄을 빼면 된다.
- 하차 전에 걸려 있던 값은 `ArmGuideOnNextMoveInput`이 덮어쓴다. 레벨이 바뀌면 `ResetArmedMoveGuide`(HandleMapLoaded에서 호출)로 푼다.
- Stage1 열차(`BP_Train_Subway`)의 1-2 승강장 정차역에 `Stage1_3`을 적는다.

**S4. 개찰구 앞 — 신규 `AGuideCellTrigger`**

`ASoundCellTrigger`와 같은 구조로 만든다. 폰에게 콜리전이 없어 볼륨 오버랩으로는 알 수 없고, 그리드의 `OnPawnEnteredCell`을
듣는다(같은 이유, 같은 코드 형태). 위치: `Source/LetsTakeTheSubway/UI/Guide/GuideCellTrigger.h/.cpp`.

```cpp
UCLASS(HideCategories = (Physics, Collision, Networking, Input, LOD, Cooking, HLOD, Replication))
class AGuideCellTrigger : public AActor
{
    /** 플레이어 폰이 이 박스가 덮는 셀에 처음 들어서면 요청할 가이드. */
    UPROPERTY(EditAnywhere, Category = "Guide Trigger") EGuideType Guide = EGuideType::None;
    /** 켜면 한 번 울린 뒤 더 듣지 않는다. RequestGuide 자체도 한 번만 띄우지만, 여기서 끊으면 방송마다 셀 비교를 안 한다. */
    UPROPERTY(EditAnywhere, Category = "Guide Trigger") bool bOnce = true;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Box;   // 에디터 표시용, 충돌 없음
    // BeginPlay: Grid = AGridActor::FindGrid, 박스 XY → Cells, OnPawnEnteredCell 바인딩 (SoundCellTrigger.cpp 26~72행과 같은 형태)
    // HandlePawnEnteredCell: Cast<AGridPawn>인 폰만, Cells에 있으면 RequestGuide, bOnce면 바인딩 해제
};
```

Stage2 개찰구 앞(`SoundTrigger_TicketGate_A`·`B`가 덮는 셀의 **바깥쪽 한 줄**)에 배치하고 `Stage2_1`을 적는다. 개찰구 셀 자체에
놓으면 통과하면서 팝업이 떠서 조작을 끊는다. 배치 좌표는 구현 때 두 사운드 트리거의 박스 범위를 읽어 정한다.
시트의 예비안("스테이지 진입 시")을 원하면 Stage2 `StageInfo.EntryGuides`에 `Stage2_1`을 넣고 셀 트리거를 지우면 된다.

**T1·T2 튜토리얼 — 비움.** 코드 변경 없음. 튜토리얼 맵(`L_ToonSample_Tutorial001`)에 `AStageInfo`가 없으므로 지금은 `EntryGuides`도
못 쓴다. 나중에 (1) 맵에 `AStageInfo`(StageIndex 0)를 놓고 `EntryGuides`에 `Tutorial2`, (2) 엘리베이터 문 앞 셀에
`AGuideCellTrigger`(`Tutorial1`)를 놓으면 코드 없이 붙는다. 큐(2.2절)는 T2를 Stage1 진입에 얹을 때 필요하다.

### 2.4 데이터 애셋 정정

`DA_GuideDatabase`에서 `Tutorial2`와 `Stage1_1`의 Title/Body를 맞바꾼다(0절). 시트 본문이 DB 본문보다 길게 다듬어져 있는
항목(`Stage1_1` "슬라이드를 통해…", `Stage1_2` "플랫폼에 도착한 지하철을 클릭해…")은 UI 담당이 시트 문구로 갱신하는지 확인한다.
`None`·`MAXVALUE` 항목은 `RequestGuide`가 걸러내므로 남아 있어도 해가 없다.

### 2.5 테스트용 콘솔 명령

`ltts.Guide <Tutorial1|Stage1_2|…>` — 트리거를 거치지 않고 팝업을 띄운다(`ShownGuides` 무시). `ltts.GuideReset` — 기록 초기화.

---

## 3. 변경 파일

| 파일 | 변경 |
|---|---|
| `UI/UIManagerSubsystem.h/.cpp` | `ShowOrRefreshPathUI(int32)`, `PlayerControllerChanged`·`BindStage`·`UnbindStage`·`HandleStageStateChanged`, `LastPathIndex`·`bStageEntryHandled`, HandleMapLoaded 초기화 |
| `UI/Guide/GuideDataSubsystem.h/.cpp` | `RequestGuide`, `ResetShownGuides`, `ArmGuideOnNextMoveInput`, `NotifyMoveInput`, `ResetArmedMoveGuide` |
| `UI/Guide/GuideCellTrigger.h/.cpp` (신규) | S4 |
| `Stage/StageInfo.h/.cpp` | `ZonePathUIIndices`, `GetPathUIIndex`, `EntryGuides` |
| `Stage/StageSubsystem.cpp` | `ValidateLayout`에 PathUI 인덱스 경고 |
| `Puzzle/PuzzleElevatorDock.h/.cpp` | `ArrivalGuide`, `HandleArrived` 바인딩 |
| `Vehicle/GridTrain.h/.cpp` | `FGridTrainStop::GuideOnFirstMoveAfterExit`, `UnloadedStopIndex`, 하차 완료 때 Arm |
| `Player/GridPlayerController.cpp` | 이동키·클릭 이동에서 `NotifyMoveInput`, 콘솔 명령 `ltts.PathUI`·`ltts.Guide`·`ltts.GuideReset` |
| `Content/Maps/Main/Subway_Stage1.umap` | StageInfo: `ZonePathUIIndices [0,1,2,3]`, `EntryGuides [Stage1_1]`. 승강장 도크 `ArrivalGuide Stage1_2`. 열차 1-2 정차역 `Stage1_3` |
| `Content/Maps/Main/Subway_Stage2.umap` | StageInfo: `ZonePathUIIndices [5,-1,6,7,-1,8,-1,9]`. 개찰구 앞 `AGuideCellTrigger(Stage2_1)` |
| `Content/UI/GuideData/DA_GuideDatabase.uasset` | Tutorial2 ↔ Stage1_1 맞바꿈 |
| `Content/UI/WBP_PathUI.uasset` (UI 담당) | Switch On Int 0~9 핀 ↔ 그림 |

맵·데이터 애셋은 에디터(unreal-mcp)로 값을 넣고 저장한다. `AStageInfo`·`AStageZoneVolume`은 이동 잠금 상태지만 속성 편집은 막지 않는다.

## 4. 검증

### 4.1 PathUI (PIE, Subway_Stage1 → Subway_Stage2)
- [ ] 레벨 시작 프레임에 PathUI가 보이고 그림 0(Stage1)이다. 로그 `[UIManager] PathUI index 0 (stage 1 zone 1)`.
- [ ] 구역 2·3·4로 걸어가면 1·2·3으로 바뀐다. 같은 구역 안에서 볼륨 경계를 오가도 다시 그리지 않는다(`LastPathIndex`).
- [ ] Stage2 구역 2·5·7에 들어가면 그림이 그대로다. 1·3·4·6·8은 5·6·7·8·9.
- [ ] 레벨 전환 뒤 PathUI가 이전 레벨 위젯을 들고 있지 않다(`HandleMapLoaded`).
- [ ] `ltts.PathUI 7`로 아무 그림이나 띄울 수 있다.

### 4.2 Guide
- [ ] Stage1 시작 직후 [엘리베이터 회전] 팝업. 닫고 나서 다시 뜨지 않는다.
- [ ] 엘리베이터를 타고 승강장 층에 내리면 [지하철 탑승]. 반대 방향(승강장→위층)에서는 뜨지 않는다.
- [ ] 열차에서 내려 자동으로 지정 자리까지 걷는 동안에는 뜨지 않고, WASD 또는 클릭으로 처음 움직일 때 [이동이 가능한 장애물].
- [ ] Stage2 개찰구 앞 한 칸에 서면 [회전 기둥과 레버]. 셀 트리거를 지나 되돌아와도 다시 뜨지 않는다.
- [ ] `ltts.Guide Tutorial1`로 비워 둔 항목도 띄울 수 있다(DB 문구 확인용).
- [ ] 팝업 6종 제목·본문이 시트와 같다(2.4절 정정 뒤).

### 4.3 UI 담당에게 확인할 것
- `WBP_GuidePopUpUI`가 닫힐 때 `CallUIClosed`를 부르는지. 큐(2.2절)와 "팝업 열린 동안 입력 막기"(6절 Q5)의 전제다.
- `WBP_PathUI`의 Switch On Int 핀 0~9와 그림 대응. 4번(Stage1 구역 5)은 지금 오지 않는다.

## 5. 순서

1. C++: StageInfo·UIManager(PathUI 바인딩) → 빌드 → Stage1·2 StageInfo 값 입력 → 4.1.
2. C++: GuideDataSubsystem 입구 + 콘솔 명령 → DB 정정 → `ltts.Guide`로 6종 확인.
3. C++: EntryGuides(S1) → 도크(S2) → 열차+컨트롤러(S3) → 셀 트리거(S4) → 맵 배치 → 4.2.
4. 문서 갱신: 이 파일 6절에 결정 기록, `Docs/StageZone.html` 4절에 "구현됨" 표시.

## 6. 열린 질문 / 가정

- **Q1. Stage1 구역 5.** 요청은 카메라 1~5인데 StageInfo는 구역 4개·카메라 4개다. 아트/기획이 구역·카메라를 하나 더 놓으면
  `ZonePathUIIndices`에 4를 추가하는 것으로 끝난다. 그 전까지 PathUI 4번 그림은 쓰이지 않는다. **가정: 구조는 5개를 받도록 만들고
  값은 지금 있는 4개만 넣는다.**
- **Q2. Stage2 ZoneCameras.** `StageRailExtension.md`(09-14)에는 "ZoneCameras 없음"이지만 그 뒤 "스테이지 2 카메라 재세팅" 커밋이
  있다. PathUI는 카메라가 아니라 구역 번호를 쓰므로 카메라 배열이 비어 있어도 동작한다. 구현 때 맵을 열어 확인만 한다.
- **Q3. 이동 입력의 범위.** 클릭 이동도 포함(2.3절 S3). 키보드만 원하면 한 줄 제거.
- **Q4. "한 번만"의 범위.** 세션(게임 인스턴스) 단위. 스테이지를 다시 열 때 다시 보여 주려면 레벨 로드 때 `ResetShownGuides`를
  부르도록 바꾼다. 타이틀 → 새 게임 진입점이 정해지면 거기서 부른다.
- **Q5. 팝업이 열린 동안 입력.** 지금 `OnUIOpened`/`OnUIClosed`를 듣는 코드가 없어 팝업 뒤에서도 조작이 된다. 요청 범위 밖이라
  이번에는 건드리지 않는다. 막고 싶으면 `AGridPlayerController`가 `OnUIOpened`에 바인딩해 `HeldMoveKeys`를 비우고 클릭을 무시하면 된다.
- **Q6. Tutorial2의 트리거.** 시트는 "1스테이지 진입 후"라 Stage1_1과 같은 순간이다. 큐 없이는 하나만 보이므로 튜토리얼 항목은
  비워 두라는 요청과도 맞는다. 큐를 만들 때 함께 켠다.

---

## 7. 구현 기록 (2026-09-16)

사용자 결정: **Stage1 구역 카메라 1~4 = PathUI 0~3, 번호 4는 클리어 엘리베이터에 탈 때.** MainUI는 `L_TitleTest`가 띄우고,
`L_TitleTest`를 시작 레벨로 둔다.

### 7.1 계획과 달라진 점
- **클리어 엘리베이터 PathUI.** `APuzzleElevatorDock::ClearPathUIIndex`(기본 -1)를 추가했다. 엔딩 승강기 `TryLaunch`가
  `StartHoldingTravel`에 성공한 순간 `UUIManagerSubsystem::ShowOrRefreshPathUI`를 부른다. Stage1 `PuzzleElevatorDock_5`에 4.
- **Stage1은 이미 구역 5·카메라 5였다**(디스크 저장본, ZoneCount 5). 구역 5는 번호 4를 쓰지 않으므로 `ZonePathUIIndices [0,1,2,3,-1]`.
- **`DA_GuideDatabase`는 바꾸지 않았다.** 0절에서 시트와 어긋난다고 본 `Tutorial2`/`Stage1_1`은 시트 2·3행의 트리거가 둘 다
  "1스테이지 진입 후"라 어느 쪽이 맞는지 시트만으로 정할 수 없다. UI 담당이 최근 개편한 배치(튜토리얼 = 이동·회전, Stage1 진입 =
  올라가기·내려가기)를 존중한다. Stage1 진입 팝업은 DB의 `Stage1_1` 문구가 뜬다.
- 가이드 입구 시그니처: `RequestGuide(EGuideType, bool bForce = false)`. 월드 문맥은 받지 않는다(게임 인스턴스 서브시스템).
  `NotifyMoveInput()`도 인자 없음. 레벨이 바뀌면 걸어 둔 가이드를 서브시스템이 스스로 푼다(`PostLoadMapWithWorld`).
- 셀 트리거는 `bOnce` 옵션 없이 항상 한 번 뒤 바인딩을 푼다.
- `UIManagerSubsystem.cpp`, `GuideDataSubsystem.cpp`는 CP949였던 것을 UTF-8(BOM)으로 바꿨다(한글 로그 문자열 보존).

### 7.2 레벨에 넣은 값
| 레벨 | 액터 | 값 |
|---|---|---|
| Subway_Stage1 | `StageInfo_1` | `ZonePathUIIndices [0,1,2,3,-1]`, `EntryGuides [Stage1_1]` |
| Subway_Stage1 | `PuzzleElevatorDock_5`(엔딩 승강기) | `ClearPathUIIndex 4` |
| Subway_Stage1 | `PuzzleElevatorDock_1`(B2 승강장 층, Dock_0의 짝) | `ArrivalGuide Stage1_2` |
| Subway_Stage1 | `BP_Train_Subway_C_0` 정차역 1 `Tunnel_North` | `GuideOnFirstMoveAfterExit Stage1_3` |
| Subway_Stage2 | `StageInfo_0` | `ZonePathUIIndices [5,-1,6,7,-1,8,-1,9]` |
| Subway_Stage2 | `GuideCellTrigger_0`(신규) | `Guide Stage2_1`, 위치 (510, −3574, 8420), 박스 X 726 / Y 45 — 개찰구 A(`SoundCellTrigger_0`) 바로 앞 한 줄, 15칸 |
| Config/DefaultEngine.ini | `GameDefaultMap` | `/Game/Maps/Main/L_TitleTest` (EditorStartupMap은 Subway_Stage1 유지) |

### 7.3 검증 결과
- 빌드 `LetsTakeTheSubwayEditor Win64 Development` 성공, 경고 0.
- PIE Subway_Stage2: `GuideCellTrigger_0: guide Stage2_1 on 15 cell(s)`, `[UIManager] PathUI index 5`.
- PIE Subway_Stage1: `[UIManager] PathUI index 0`, `[Guide] Showing Stage1_1`.
- 아직 손으로 확인 안 함: 구역 이동에 따른 번호 변화, 엔딩 승강기 탑승 때 4, 승강장 도착 팝업, 하차 뒤 첫 이동 팝업, 개찰구 앞 팝업,
  위젯 화면 표시(`WBP_PathUI` Switch On Int 핀 0~9 연결은 UI 담당).

## 8. UI 입력 차단 · MainUI 시작 버튼 (2026-09-16)

사용자 결정: 입력을 막는 UI가 열리면 UI 클릭만 받고 나머지는 막는다. 가이드 팝업은 저절로 뜨고 닫히므로 막지 않는다.
MainUI 플레이 버튼은 임시로 Stage1을 열되, 나중에 바꾸기 쉽게 블루프린트로 구현한다.

### 8.1 입력 차단
- `UUIManagerSubsystem::CallUIOpened`/`CallUIClosed`가 열린 UI 개수(`OpenUICount`)를 센다. 0→1이면 `FInputModeUIOnly`로 바꾸고
  `FlushPressedKeys`, 1→0이면 `AGridPlayerController::BeginPlay`와 같은 `FInputModeGameAndUI`로 되돌린다. 커서는 항상 보인다.
  닫기가 더 많이 불려도 음수가 되지 않는다. 맵이 바뀌면 0으로 되돌린다. `IsUIOpen()`(BlueprintPure) 추가.
- `AGridPlayerController`가 `OnUIOpened`/`OnUIClosed`에 바인딩한다. 열리는 순간 누름 판정·레버/블록 드래그·이동키·호버를 정리하고,
  열려 있는 동안 클릭·이동키·틱 조작을 무시한다. 이미 걸어가고 있는 탑승 예약만 마저 진행한다.
- 지금 `CallUIOpened`/`CallUIClosed`를 부르는 위젯은 `WBP_SideMenuUI` 하나다(열릴 때 `EventInitialize`, 닫기 버튼 두 개).
  `WBP_GuidePopUpUI`와 `WBP_PathUI`는 부르지 않는다.

### 8.2 MainUI 시작 버튼
- `WBP_MainUI`에 `Name` 변수 `PlayLevelName`(카테고리 Main Menu, 기본값 `Subway_Stage1`)을 추가했다.
- `OnClicked(startBtn)` → `Open Level (by Name)`(`PlayLevelName`). 시작 레벨을 바꾸려면 변수 기본값만 고치면 된다.

### 8.3 확인
- 빌드 성공, 경고 0. 에디터 재시작 뒤 `PlayLevelName` 기본값과 시작 버튼 그래프가 유지되는 것을 확인했다.
- 입력 차단은 PIE에서 확인하지 못했다. 게임 안에서 `ShowSideMenuUI`를 부르는 곳이 아직 없어 사이드 메뉴를 열 방법이 없다.

### 8.4 발견한 문제 (UI 담당 확인 필요)
- `WBP_PathUI`의 `InitializeInt`에서 Switch On Int의 **0번 핀이 비어 있고 Default 핀에 point1_1이 연결**돼 있다.
  그래서 번호 0(Stage1 구역 1)을 넘기면 모든 그림이 꺼진 채로 남는다. point1_1을 0번 핀으로 옮겨야 한다.

## 9. 타이틀 레벨 교체: L_TitleTest → Subway_Title (2026-09-16)

사용자 결정: 타이틀 레벨은 아트가 만든 `Subway_Title`이다. MainUI도 그 레벨이 띄우고, 시작 레벨과 쿡 목록도
`Subway_Title`로 바꾼다. 카메라는 레벨에 놓인 `CAM_ToonIso`로 고정하고, 배경음악은 타이틀 전용 곡을 무한 루프로 튼다.

### 9.1 바뀐 것
- **`ATitleGameMode`(신규, `Source/LetsTakeTheSubway/Player/TitleGameMode.h/.cpp`).** 폰을 만들지 않는다
  (`DefaultPawnClass`/`SpectatorClass` 비움, `bStartPlayersAsSpectators` 켬 -- 폰 없이 `RestartPlayer` 재시도 루프에 빠지지
  않게). BeginPlay에서 `AutoActivateForPlayer = Player0`인 카메라를 찾아 `SetViewTarget`으로 고정하고,
  `UUISettings::MainUIWidget`을 만들어 뷰포트에 붙인 뒤 커서를 보이며 `FInputModeUIOnly`로 바꾼다.
  포커스 위젯은 주지 않는다(`WBP_MainUI`는 Is Focusable이 꺼져 있어 주면 "Non-Focusable widget" 오류가 남는다).
  `AGridTestGameMode`를 쓰지 않는 이유: `AGridPlayerController`가 구역 카메라 없는 레벨에서 폰 카메라를 켜 고정 카메라를
  덮어쓴다(`ShouldFollowPawn`).
- **`UUISettings::MainUIWidget`(신규 Config).** `Config/DefaultGame.ini`에 `/Game/UI/WBP_MainUI.WBP_MainUI_C`.
  L_TitleTest는 레벨 블루프린트(BeginPlay → Create Widget → Add to Viewport)로 띄웠는데, 그 그래프는 옮기지 않고 게임 모드가
  같은 일을 한다. L_TitleTest의 레벨 블루프린트는 그대로 남아 있다(레벨 자체는 이제 어디서도 참조하지 않는다).
- **Subway_Title 월드 세팅.** GameMode Override `BP_gamemode_test`(아트 테스트용) → `TitleGameMode`.
- **배경음악.** `C:/Users/User/Downloads/BGM.wav`를 `/Game/Art/Sound/BGM/SW_BGM_Title`로 임포트(ImportAssets 커맨드렛,
  48 kHz 스테레오 16비트, 16.17초). `bLooping = true`, SoundClass `SC_Music`(SC_Master 자식이라 마스터 볼륨이 먹는다).
  레벨에 `AmbientSound` 액터 `BGM_Title`(위치 (0, 0, 200), 감쇠 없음 → 2D 재생, Auto Activate)로 놓았다.
  타이틀 전용이라 `DA_SoundLibrary`에는 넣지 않았다. 레벨을 떠나면 액터와 함께 멈춘다.
- **카메라.** `CAM_ToonIso`(`CameraActor_0`)는 이미 `AutoActivateForPlayer = Player0`였다. 다른 카메라 `CameraActor_1`은
  Disabled. 게임 모드가 BeginPlay에서 한 번 더 뷰 타깃으로 고정한다.
- **Config.** `DefaultEngine.ini` `GameDefaultMap` → `/Game/Maps/Main/Subway_Title.Subway_Title`(EditorStartupMap은 Subway_Stage1
  유지). `DefaultGame.ini` `MapsToCook`의 `L_TitleTest` → `Subway_Title`.

### 9.2 확인 결과 (PIE, Subway_Title)
- 로그: `TitleGameMode_0: view locked to CAM_ToonIso.`, `TitleGameMode_0: WBP_MainUI_C shown.`,
  `LogAudio: Playing AudioComponent ... AmbientSound_0.AudioComponent0 with Sound: 'SW_BGM_Title'. OneShot?: 'NO'`.
- 화면: CAM_ToonIso 시점 위에 MainUI(지하철을 타자 / 플레이·설정·도움·종료) 표시.
- 아직 확인 안 함: 플레이 버튼으로 Subway_Stage1 진입 뒤 입력 모드 복귀(GridPlayerController::BeginPlay가 GameAndUI로 되돌린다),
  패키징(Live Coding으로 새 클래스를 넣었으니 패키징 전에 에디터를 껐다 켜서 전체 빌드).
- Subway_Title에는 아트 레벨에서 온 `BP_LEVER001`·`BP_ESCALATOR_*`가 있어 PIE 시작 때 "no AGridActor" 경고/오류가 찍힌다.
  타이틀에는 그리드가 없으니 무해하지만, 로그가 거슬리면 그 액터들을 빼거나 정적 메시로 바꾸면 된다.

## 10. 타이틀 배경 지하철 왕복 (2026-09-16)

사용자 요청: 타이틀 배경의 지하철이 왕복하게. `L_ToonSample_S2_Jimin` 참고.

### 10.1 조사
- Jimin 레벨과 Subway_Title은 같은 구성이다. 움직이는 열차는 `BP_SubwayAnim`(아트, `/Game/Art/JM_BluePrints`) 하나이고,
  BeginPlay에서 문 열림 → 3초 → 문 닫힘 → 1.5초 → 바퀴 회전 루프 → 타임라인 `TL_SubwayDeparture`(5초)로 `ExitLocation`까지
  **한 번** 가고 끝난다.
- 두 레벨 모두 그 인스턴스가 (−20590, 7235, 8320)에 있어 `CAM_ToonIso`(두 레벨 카메라 동일) 화면 밖이었다.
  타이틀 화면에 보이던 열차는 정적 메시 30개(`StaticMeshActor_427`~`456`, 위치 (1980, 13042, 8930), 회전 −90, 스케일
  (1.4, 1.68, 1.4))로 만든 복사본이라 움직이지 않았다.

### 10.2 바뀐 것
- **`BP_SubwayAnim`에 왕복 옵션 추가.** 변수 카테고리 `Round Trip`:
  `bRoundTrip`(인스턴스 편집 가능, 기본 false), `TurnaroundWaitSeconds`(인스턴스 편집 가능, 기본 2초), `bReturning`(내부 상태).
  커스텀 이벤트 `StationCycle`이 기존 문 열림 노드로 들어간다(BeginPlay 연결은 그대로).
  타임라인 Finished → `bRoundTrip`가 꺼져 있으면 아무것도 안 한다(기존 동작 그대로 → Jimin 레벨은 변화 없음).
  켜져 있으면: 종점 도착 시 바퀴 정지 → 대기 → `bReturning = true` → 바퀴 재생속도 −1로 루프 → 타임라인 `ReverseFromEnd`.
  역에 돌아오면 바퀴 정지 → `bReturning = false` → 재생속도 1 → `StationCycle`(문 열림부터 반복).
  역재생이라 출발 때 가속한 곡선이 돌아올 때는 감속으로 들어온다. 출발 위치는 매 주기 `SetDepartureStartLocation`이 다시
  읽지만 역재생이 알파 0에서 끝나므로 같은 값이다.
- **Subway_Title 배치.** `BP_SubwayAnim_C_3`을 보이는 열차 자리로 옮겼다: 위치 (1980, 13042, 8930), 회전 −90,
  스케일 (1.4, 1.68, 1.4). `ExitLocation` (1980, 20042, 8930) — +Y로 70 m. 직교 카메라(OrthoWidth 4500) 투영으로 계산하면
  +Y 약 60 m부터 열차 전체가 화면 밖이다. `bRoundTrip = true`, `TurnaroundWaitSeconds = 2`. 아웃라이너 폴더 `Title/SubwayRoundTrip`.
- **정적 열차 30개**는 지우지 않고 `bHidden`(Actor Hidden In Game)만 켰다. 에디터 뷰포트에서는 보이고 블루프린트 열차와
  정확히 겹친다. 아웃라이너 폴더 `Title/StaticSubway_HiddenInGame`. 되돌리려면 `bHidden`을 끄고 블루프린트 열차를 원래
  위치 (−20590, 7235, 8320), 스케일 1, `ExitLocation` (−20590, 10000, 8320)으로 돌리면 된다.
- 선로 위 반대쪽 정적 열차 복사본(`StaticMeshActor_709`~`738`, (−23443, 16568, 8930))은 화면 밖이라 건드리지 않았다.

### 10.3 확인 (PIE, Subway_Title)
- 위치 기록: 13042(정차·문 열림) → 20042(화면 밖) → 역재생으로 13042 복귀 → 문 열림부터 다시. 약 34초 동안 두 바퀴 반복.
- 캡처: 정차 중 문 열림, 출발, 빈 선로(화면 밖), 복귀 중 모두 확인. 블루프린트 열차가 정적 열차와 같은 자리·크기로 보인다.
- 확인 안 함: Jimin 레벨 PIE(옵션 기본값이 false라 그래프상 기존 흐름과 같다), 복귀 때 바퀴가 반대로 도는지 눈으로 확인.
