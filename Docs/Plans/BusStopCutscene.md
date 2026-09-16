# 스테이지 1→2 버스 정류장 컷씬

작성일 2026-09-15. 대상: `Source/LetsTakeTheSubway/Cutscene/`(신규), `AGridPawn`, `LetsTakeTheSubway.Build.cs`,
`Content/Core/Cutscene/LS_BusStop_Board.uasset`·`LS_BusStop_Alight.uasset`(신규),
`Content/Maps/Main/L_ToonSample_BusStop_Start.umap`·`L_ToonSample_BusStop_Arrived.umap`.
선행 문서: `TrainArtDoors.md`(문 중심 정렬·높이 고정 규칙), `../StageClearElevator.md`(Stage1 끝 연출 훅).

---

## 0. 왜

| 요청 | 이전 상태 |
|---|---|
| 흐름: Start 맵 승차 컷씬 → 암전 → Arrived 맵 하차 컷씬 → 암전 → 다음 스테이지 | 두 맵은 툰 샘플 맵 복사본. `BP_Bus`가 BeginPlay에서 스스로 달리고, 폰은 아트용 드래그 더미(`BP_PlayerFX_Test`)였다. 게임 모드가 빈 `BP_gamemode_test`라 `AGridPawn`이 스폰되지 않았다. 레벨 전환·페이드·시퀀스 코드는 프로젝트에 없었다 |
| 1. 지하철처럼 문 한가운데와 일직선이 되는 자리로 먼저 선 뒤, 높이 그대로 승차·하차 | 규칙은 `AGridTrain::TryBoard`에만 있었다. 폰의 하차 API(`WalkOntoGrid`)는 그리드가 있어야 동작한다 |
| 2. 시퀀스 사용(다른 아이디어 환영) | 3절 |

## 1. 결정

1. **하이브리드.** 레벨 시퀀스가 연출(버스 이동, 문·바퀴 애니메이션, 카메라 컷)을 갖고, C++ 디렉터(`ABusStopCutsceneDirector`)가 논리(승하차 좌표, 페이드, 레벨 전환)를 맡는다. 폰까지 시퀀스로 키를 잡으면 "문 한가운데·높이 그대로" 규칙이 키프레임에 묻혀 문 위치가 바뀔 때마다 손으로 다시 잡아야 한다. 순수 C++는 아트가 타이밍과 카메라를 만질 수 없다.
2. **시퀀스는 마크 프레임에서 멈추고, 폰이 다 타거나 내리면 이어서 재생한다**(`PlayTo`). 대기 시간을 시퀀스에 박지 않으므로 폰이 걷는 시간이 달라도 문이 먼저 닫히지 않는다.
3. **버스는 `SkeletalMeshActor`(`SK_Bus`, `SM_BUS_001`)로 교체.** 사용자 결정. `BP_Bus`의 타임라인 주행이 시퀀스와 버스 위치를 두고 싸우기 때문이다. 아트의 `BP_Bus` 애셋은 손대지 않았다.
4. **앞문 승차, 중간문 하차.** 사용자 결정. 디렉터의 `Door` 프로퍼티로 바꾼다.
5. **문 중심은 스켈레탈 메시의 레퍼런스 포즈 본에서 계산한다**(`Door_Front_A/B`, `Door_Middle_A/B` 평균). 문이 열려 있든 닫혀 있든 같은 값이 나온다.
6. **페이드는 C++**(`StartCameraFade`). 두 맵 모두 검정에서 시작해 밝아지고, 끝에서 검게 어두워진 뒤 `OpenLevel`한다. 앞 레벨의 끝 암전과 다음 레벨의 시작 암전이 이어진다.
7. Stage1 → Start 맵 연결은 이번 범위에서 제외(사용자 결정). 연결 방법은 6절.

## 2. 실측 (에디터 MCP + PIE, 2026-09-15)

| 항목 | 값 |
|---|---|
| `SM_BUS_001` 로컬 바운드 | X ±299(폭), Y ±880(진행축, +Y가 앞), Z 0→490 |
| `SK_Bus` 배치 | (−4000, −409, 0), yaw −90, 스케일 1.2 — 기존 `BP_Bus_C_2`와 같다. 진행축 = 월드 +X, 문 = +Y(인도) 쪽 |
| 문 중심(진행축, 차체 중심 0) | 앞문 **+826 cm**, 중간문 **+46 cm** (PIE 로그) |
| 애니메이션 길이 | `Anim_Bus_Door_Open` 1.467 s, `Anim_Bus_DoorClose` 1.467 s, `Anim_Bus_WheelRotate` 2.0 s |
| 승차(Start) | 폰 (−685, 230, 50) → 진행축으로 **14 cm** 정렬 → 좌석 (−671, −409, **50**). 폰 Z 50 유지 |
| 하차(Arrived) | 좌석 (−3952, −409, 50)에서 출발 → 정차 후 문 앞 (−1451, 30, **50**) → `TP_BusStop` (−685, 230, **50**) |
| 전체 흐름 | Start → Arrived → `Subway_Stage2` 자동 전환, 우리 코드 Error 0건. Stage2에서 `standing on cell (252,244)` |
| PIE 캡처 | 마크에서 멈춘 순간 앞문·중간문이 다 열린 채 정지, 카메라는 `CAM_ToonIso` |

## 3. 설계

### 3.1 흐름

```
Board (Start 맵)
  BeginPlay  화면 검정, 폰·컨트롤러 대기
  → 폰 준비  입력·HUD 잠금, 1초 페이드 인, 시퀀스를 마크까지 재생
  → 마크 도달(버스 정차, 문 다 열림)  폰: 진행축 정렬 → 좌석으로 직진 (BoardVehicle)
  → 폰 착석(Riding)  시퀀스 재개: 문 닫힘 → 출발 (폰은 버스에 실려 감)
  → 시퀀스 끝  1초 페이드 아웃 → OpenLevel(L_ToonSample_BusStop_Arrived)

Alight (Arrived 맵)
  BeginPlay  화면 검정
  → 폰 준비  폰을 중간문 한가운데 좌석에 곧바로 앉힘(SitInVehicle), 페이드 인, 마크까지 재생
  → 마크 도달  폰: 문 앞으로 수직 하차 → 하차 목표로 걷기 (LeaveVehicleTo)
  → 폰 도착(OnGrid)  시퀀스 재개: 문 닫힘 → 출발
  → 시퀀스 끝  페이드 아웃 → OpenLevel(Subway_Stage2)
```

### 3.2 `ABusStopCutsceneDirector` (`Cutscene/BusStopCutsceneDirector.h/.cpp`)

| 프로퍼티 | Start 맵 | Arrived 맵 | 뜻 |
|---|---|---|---|
| `Mode` | Board | Alight | 타는 장면 / 내리는 장면 |
| `SequenceActor` | `LS_BusStop` | `LS_BusStop` | 레벨의 시퀀스 액터 |
| `PauseMarkLabel` | Doors | Doors | 멈출 마크 이름(3.4 참고) |
| `Bus` | `SK_Bus` | `SK_Bus` | 문 본을 읽을 버스 |
| `Door` | Front | Middle | 쓸 문 |
| `ExitPoint` | — | `TP_BusStop` | 내린 뒤 걸어갈 곳(XY만) |
| `ViewCamera` | `CAM_ToonIso` | `CAM_Isometric1` | 시퀀스에 카메라 컷이 없을 때의 안전장치 |
| `NextLevel` | `L_ToonSample_BusStop_Arrived` | `Subway_Stage2` | 끝나면 열 레벨 |
| `FadeInSeconds` / `FadeOutSeconds` | 1.0 / 1.0 | 1.0 / 1.0 | |
| `SeatEndMargin` / `DoorClearance` | 100 / 80 | 100 / 80 | 좌석이 차체 끝에서 떨어질 거리 / 문 앞 경유점이 옆면에서 떨어질 거리 |

- 좌표 계산은 `AGridTrain::TryBoard`와 같은 식이고 `LTTSVehicle::AlongAxis`·`SeatAtDoor`(`Vehicle/VehicleSeat.h`)를 그대로 쓴다. 그리드 셀 판정만 없다.
- 일시정지는 `IsPaused()`를 틱에서 본다. `OnPause` 방송은 러너 평가 뒤로 미뤄질 수 있어서다. 끝은 `OnFinished`로 받는다.
- 마크 이름이 없어도 시퀀스의 마크가 하나뿐이면 그것을 쓰고 Warning을 남긴다. 마크가 아예 없으면 시퀀스 끝에서 승하차하고 곧바로 페이드 아웃한다.
- 에디터 버튼 **Measure Door**: 앞문·중간문의 진행축·옆 방향 위치를 출력 로그에 찍는다.
- 콘솔 **`ltts.BusCutscene Skip`**: 대기·페이드 없이 곧바로 `NextLevel`을 연다.

### 3.3 `AGridPawn` 추가

| 함수 | 뜻 |
|---|---|
| `SitInVehicle(Vehicle, Seat)` | 걷지 않고 즉시 착석(Riding). 시퀀스는 액터 틱보다 먼저 평가되므로 `BoardVehicle`의 Entering 한 틱 사이에 버스가 움직이면 좌석 오프셋이 어긋난다 |
| `LeaveVehicleTo(Exit, Through)` | 그리드 없이 월드 점으로 내린다. `Through`(문 앞)를 거쳐 `Exit`로. 도착하면 OnGrid, 셀 진입 알림은 보내지 않는다 |

- 경유 목표 멤버 `PendingSeat` → `PendingWalkTarget`(Entering·Leaving 공용)으로 이름을 넓혔다.
- Leaving 끝에서 그리드가 없으면 `NotifyPawnEnteredCell`을 부르지 않는다(널 가드).
- 그리드가 없을 때 `BeginPlay` 로그를 Error → Warning으로 낮췄다. 지하철 쪽 동작은 바뀌지 않는다.

### 3.4 `ABusStopGameMode` (`Cutscene/BusStopGameMode.h/.cpp`)

`AGridTestGameMode`와 같은 폰·컨트롤러·HUD. 그리드 바인딩만 건너뛰어 "그리드 없음" Error가 뜨지 않는다. 두 맵의 World Settings → GameMode Override에 지정했다.

## 4. 시퀀스와 편집 규칙

`LS_BusStop_Board`(Start 맵)와 `LS_BusStop_Alight`(Arrived 맵)는 내용이 같다. 30 fps, 재생 범위 0–373.

| 프레임 | 버스 트랜스폼(Location.X) | 버스 스켈레탈 애니메이션 |
|---|---|---|
| 0 → 120 | −4000 → −1499 (도착, 감속) | `WheelRotate` |
| 120 → 164 | −1499 정차 | `Door_Open` |
| **164** | **마크(현재 라벨 `A`)** | 열림 끝 = 닫힘 시작 |
| 164 → 208 | −1499 정차 | `DoorClose` |
| 208 → 358 | −1499 → 3000 (출발, 가속) | `WheelRotate` (208 → 373) |
| 전체 | 카메라 컷 = 맵의 아트 카메라 | |

시퀀스 액터 `LS_BusStop` 재생 설정: Auto Play 끔, Pause at End 켬, Finish Completion State Override = Force Keep State, Disable Movement Input·Hide HUD 켬.

**아트가 시퀀스를 고칠 때 지킬 것**

1. **모든 섹션 Completion Mode = Keep State.** 새 섹션의 기본값(Project Default)은 Restore State라서, 섹션이 끝나는 프레임에 버스가 원래 자리로 튀거나 문이 닫힌 포즈로 튄다.
2. **스켈레탈 애니메이션 섹션을 겹치지 않는다.** 겹치면 섞여서 문이 반만 열린다(바퀴 클립은 문 본을 닫힘 포즈로 둔다).
3. **문 열림 섹션 끝 = 문 닫힘 섹션 시작, 그 프레임에 마크.** 틈이 있으면 틈에서 문이 닫힌 포즈로 튄다. 디렉터는 마크 바로 앞 프레임에서 멈추므로 문이 다 열린 채 기다린다.
4. **문 섹션을 애니메이션 길이보다 늘리지 않는다.** 늘리면 루프가 돌아 열렸다 닫혔다를 반복한다. "열린 채 기다리기"는 디렉터의 일시정지가 만든다.
5. **마크는 하나만 두거나, 이름을 `Doors`로 바꾼다.** 시퀀서 타임라인 위 마크를 우클릭 → 이름 바꾸기. 지금 라벨은 MCP가 붙인 자동 이름 `A`이고, 마크가 하나뿐이라 그대로 동작한다(Warning 한 줄).
6. 버스·카메라 바인딩을 지우지 않는다. 다른 맵에 시퀀스를 복제해 쓰면 바인딩을 그 맵의 액터로 다시 잡아야 한다.

## 5. 검증 방법

1. `L_ToonSample_BusStop_Start`에서 Play. 검정 → 밝아짐 → 버스 도착 → 문 열림 → 공이 앞문 쪽으로 옆으로 조금 옮긴 뒤 곧장 들어감 → 문 닫힘·출발 → 암전.
2. 곧바로 `L_ToonSample_BusStop_Arrived`: 검정 → 버스(공 탑승) 도착 → 문 열림 → 공이 중간문에서 수직으로 나와 정류장 표식 쪽으로 걸어감 → 문 닫힘·출발 → 암전 → `Subway_Stage2`.
3. 출력 로그 `LogLTTSGrid`에서 `boarding through the front door … height kept at 50`, `alighting through the middle door … height kept at 50`을 확인한다.
4. 반복 확인은 콘솔 `ltts.BusCutscene Skip`.

## 6. Stage1에 연결하는 방법

Start 맵은 스스로 검은 화면에서 시작하므로, **Stage1 쪽은 끝날 때 검게 어두워진 뒤 Start 맵을 열기만 하면 된다.**

### 6.1 엔딩 엘리베이터로 끝낼 때 (권장, 이미 있는 훅)

1. `Subway_Stage1`에서 출구로 쓸 `PuzzleElevatorDock`를 고르고 Detail → Elevator Dock | Stage Clear → **Is Clear** 켬(`../StageClearElevator.md` 1절).
2. 그 Dock을 선택한 채 **Open Level Blueprint** → 우클릭 `Add Event for <Dock 이름>` → **On Clear Cutscene**.
3. 이어서 노드를 단다.

```
On Clear Cutscene
  → Get Player Camera Manager (Player Index 0)
  → Start Camera Fade   From 0 / To 1 / Duration 1.0 / Color 검정 / Should Fade Audio 체크 / Hold When Finished 체크
  → Delay 1.0
  → Open Level (by Object Reference)   Level = L_ToonSample_BusStop_Start
```

4. Stage1 저장 후 Play로 엘리베이터 클리어 → 암전 → 버스 컷씬이 이어지는지 본다.

### 6.2 다른 사건으로 끝낼 때

트리거 볼륨 오버랩, 열차 도착 이벤트 등 **어떤 블루프린트 이벤트에서든 3번과 같은 네 노드**를 달면 된다. 핵심은 `Hold When Finished`를 켜서 검은 화면을 유지한 채 `Open Level`하는 것이다.

### 6.3 주의

- `Open Level`은 **by Object Reference**를 쓴다. 맵 이름이 바뀌어도 링크가 끊기지 않는다.
- 두 버스 맵과 `Content/Core/Cutscene/`는 아직 git에 올라가지 않았다. 커밋할 때 함께 추가해야 다른 팀원 PC에서 연결이 끊기지 않는다.
- 패키징할 때는 Project Settings → Packaging → List of maps to include에 Stage1, 두 버스 맵, Stage2가 들어 있는지 확인한다.

## 7. 남은 것

- 마크 라벨 `A` → `Doors` 이름 바꾸기(4절 5번). MCP에 라벨을 바꾸는 기능이 없어 하지 못했다.
- 공의 높이를 정류장 높이(Z 50)로 고정했으므로, 저상버스 바닥이 그보다 높으면 버스 안에서 공 아랫부분이 바닥에 파묻혀 보일 수 있다. 차체에 가려 컷씬 카메라에서는 보이지 않았다.
- 중간문 위치(+46 cm)는 본 데이터에서 계산했고 PIE 캡처에서 열린 문 사이로 나오는 것까지는 확인하지 못했다(하차 순간 카메라 거리가 멀다). 어긋나 보이면 Measure Door 버튼으로 값을 보고 알려 주면 된다.
- 바퀴 애니메이션 재생 속도는 버스 속도와 맞추지 않았다(시퀀스 섹션 Play Rate로 조정 가능).
- 에디터가 백그라운드일 때 PIE가 초당 3프레임으로 느려져, 검증 중 타이밍이 실제보다 느리게 찍혔다. 논리 순서에는 영향이 없다.
- 원본 맵 백업: 수정 전 두 맵은 이번 세션 스크래치패드에만 있다. 필요하면 `Content/Shader/Toon/Sample/Maps/L_ToonSample_BusStop(_2)`가 원래 샘플이다.
