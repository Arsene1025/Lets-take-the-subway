# Subway_Stage1 플레이 가능화 계획 — 아트 배치 맵에 그리드·퍼즐·열차·에스컬레이터 연결

작성일 2026-09-10. 출처: 사용자 요청 8건(채팅). 대상 레벨: `Content/Maps/Main/Subway_Stage1.umap`
(아트가 스태틱 메시만 배치한 상태, World Partition 아님 — 액터가 umap 안에 직렬화됨).
선행 문서: `RushHourPuzzle.md`(블록·회전판), `RotatingObstacle.md`(회전 장애물·레버·기둥),
`StationFlow.md`(퍼즐 구간·엘리베이터 구조물·열차·에스컬레이터·아트 자식 액터), `GridNPC.md`(행인),
`GreyBoxTest_1.md`(그리드 배치 절차).

---

## 0. 한 장 요약

| # | 요청 | 판정 | 핵심 |
|---|---|---|---|
| 1 | 벤치·L벤치·자판기를 이동 가능 BP로, 같은 자리에 배치 | **완료** | BP 3종 + 블록의 `ArtMesh` 슬롯. 16개를 원래 자리(셀 스냅)로 교체, 겹침 0 |
| 2 | `SM_Pillar`를 회전 기둥 BP로 | **완료** | 기둥 9개를 `BP_RotatingPillar_Station`으로 교체. 아트 기둥이 2 m라 1x1 고정을 **정사각형 허용**으로 완화했다. **레버는 아직 하나도 없다** — 레버 없이는 돌지 않는다(9.6-4) |
| 3 | 이동·회전 가능 여부를 디테일 패널에서 | **완료** | `bCanMove`, `bCanRotate` 두 체크박스. 클래스 기본값과 인스턴스 양쪽에서 설정된다 |
| 4 | 그리드 시스템으로 이동 가능 레벨화 | **완료** | `StationGrid` 140x125셀 하나로 두 층을 모두 덮는다. B1 바닥이 꽉 찬 블록이라 겹침 문제가 없었다(8.1). walkable 3917칸 |
| 5 | 구조물 위로 엘리베이터 드래그 → 하강 | **완료(마우스 확인 남음)** | B1의 엘리베이터를 서쪽 샤프트로 밀어 넣고 타면 B2로 내려간다. 샤프트 진입을 허용하는 코드 예외 하나 추가(8.3). 한 방향이며 올라올 때는 에스컬레이터 |
| 6 | 카드기·시계는 스태틱 메시로 | **완료** | 손대지 않았다. 콜리전대로 Blocked 셀이 된다 |
| 7 | Subway 메시들로 `AGridTrain` 상속 BP | **완료** | `BP_Train_Subway`에 메시 30개를 컴포넌트로 넣고 액터 하나로 배치. 선로는 Y축이지만 액터를 yaw −90으로 놓으면 기존 전제가 그대로 맞아 축 프로퍼티는 필요 없었다 |
| 8 | NPC가 에스컬레이터 이용 | **미착수** | 설계만 2.5절에 있다. 탑승자 인터페이스 + 다중 탑승 + 행인 경로 단계가 필요하다 |

요청 밖: 스테이지 클리어 조건·컷신·UI. 맵의 나머지 장식 메시(포스터·안내판·청소도구·매점)는 그대로
둔다(콜리전에 따라 Blocked가 되거나 무시됨).

## 1. 현재 맵 상태 (umap 문자열 분석, 2026-09-10)

에디터 없이 umap의 이름 테이블을 읽은 결과다. 트랜스폼은 못 읽었으므로 0단계에서 잰다.

| 항목 | 있음 |
|---|---|
| 게임플레이 액터 | `PuzzleElevatorDock` 1, `PlayerStart` 1, `BP_escalator_up_s` 1, `BP_escalator_down_s` 2(`_s`, `_s2`), `BP_RailTrack` 1 |
| **없음** | `GridActor`, 셀 마커, `PuzzleRegion`, `PuzzleElevatorBlock`, `PuzzleLever`, `GridTrain`, `GridNPCSpawner` |
| 요청 1 대상 라벨 | `SM_B2_Bench001~007`(7), `SM_B1_LBench001~003`(3), `SM_B2_VendingMachine001~005`(5) |
| 같은 종류의 다른 라벨 | `SM_B1_Bench001/002`, `SM_B2_LBench001`, `SM_B1_VendingMachine001/002/004`, `SM_VendingMachine_Small1`, `SM_Bench_002`, `SM_LBench_001` — **요청에 없음. 7절 Q1** |
| 요청 2 대상 | `SM_B1_Pillar001/002/3`, `SM_Pillar01~04_B2`, `SM_Pillar03_B3/B4`, `SM_Pillar_005`, `SM_Pillar_6` (11). 메시는 모두 `SM_Pillar_005`로 보임 |
| 요청 6 | `SM_B1_CardMachine001~005`(`SM_CardMachine01`), `SM_Clock`, `SM_Clock2~4` |
| 요청 7 | `SM_Subway1_*`·`SM_Subway2_*` 각 14개(Body·Interior·Wheel01~04·Door L1/L2/R1/R2 × front/back) + `SM_Subway_Head` + `SM_Subway_joint` = **30개** |
| 개찰구 | `SM_TicketGate001~015_B1` + `SM_TicketGateDoor001~015_B1`, `SM_Ticket_Gate_Lane`(2) |
| 층 구조 단서 | 라벨 접미사 `_B1`/`_B2`/`_B3`/`_B4`/`_Rail`. `SM_Floor01_B1/B2/B3`, `SM_Floor02_B1/B2`. B3·B4는 "세 번째 조각"인지 층인지 불명 |
| 라이팅 | `DirectionalLight` 있음. 포스트 프로세스(툰) 볼륨은 0단계에서 확인 |

메시 자산: `Art/JM_StaticMesh/SM_Bench/SM_Bench_002`, `SM_LBench_001`,
`SM_VendingMachine/VM_1/SM_VendingMachine_Small(1)`, `SM_Pillar/SM_Pillar_005`,
`SM_Subway_001/*`. 아트 BP: `Art/blueprint/BP_elevator`(엘리베이터 비주얼, FlowTest_1에서 검증됨),
`BP_LEVER001`(부모 `Actor`, 미연결), `BP_escalator_up_s/down_s`(부모 `AGridEscalator`, 연결됨).

## 2. 설계 결정

### 2.1 아트 메시를 블록에 붙이는 방법 — **블록에 스태틱 메시 슬롯을 추가**한다

| 안 | 내용 | 판단 |
|---|---|---|
| A. `VisualActorClass`(자식 액터) | 벤치마다 아트 액터 BP를 만들어 자식으로 품는다 | 지금 엘리베이터 방식. 스태틱 메시 하나를 위해 액터 BP를 하나씩 더 만들어야 하고(3종), 자식 액터는 에디터에서 트랜스폼 미리보기가 번거롭다 |
| B. BP 자식에서 `StaticMeshComponent`를 손으로 추가 | 코드 무변경 | 프록시 큐브가 계속 보인다(`IsUsingArtVisual`은 `VisualActorClass`만 본다). 콜리전 끄기·`GridTraceIgnore`를 BP마다 손으로 반복 |
| C. **`APuzzleBlock`에 `ArtMesh` 슬롯** | `UStaticMesh* ArtMesh` + 네이티브 `UStaticMeshComponent ArtMeshComponent` + `FTransform ArtMeshOffset`. 값이 있으면 큐브를 숨기고 아트 컴포넌트 콜리전을 끈다 | **권장.** BP 3종은 기본값만 다르다(메시·풋프린트·높이·오프셋). 자식 액터 방식과 같은 규칙: **보이는 것은 아트, 잡히는 것은 풋프린트 크기 프록시** |

C로 간다. `IsUsingArtVisual()`은 `VisualActorClass != nullptr || ArtMesh != nullptr`가 된다.
`README` 규칙(Core → Art 하드 참조 지양)은 BP 기본값이 메시를 가리키는 것이므로 C++에는 참조가
없다. 기둥(`APuzzleRotatingPillar::RefreshVisual`)은 부모를 부르지 않으므로 거기서도 아트가 있으면
원기둥과 부착 면 패널을 숨기는 코드를 넣는다. **부착 면 패널은 아트 위에서도 보이게 둘지**는 7절 Q3.

### 2.2 이동·회전 토글 — `bCanMove` / `bCanRotate`

- `APuzzleBlock::bCanMove`(기본 true). false면 `CanSlide`가 "This object is fixed in place."로
  거부하고 `GetWorldMoveAxis`가 `None`을 돌려준다. 컨트롤러는 잡는 순간 사유를 띄운다(지금
  `MoveAxis==None`일 때 "nowhere" 문구가 있는데, 그 경로를 재사용). **셀 점유는 유지**된다 —
  고정 벤치는 폰과 다른 블록의 길을 막는다.
- `MoveAxis`는 그대로 축 제한용으로 남긴다. 기존 `Immovable(None)` 값도 남긴다(직렬화 호환).
- `APuzzleBlock::bCanRotate`(기본 true). 두 곳이 읽는다: 회전판 `APuzzleRotationTile::TryRotate`는
  안에 있는 블록 중 하나라도 false면 거부, 회전 장애물·기둥 `APuzzleRotatingObstacle::TryRotate`는
  자기 `bCanRotate`가 false면 "This pillar does not turn." 거부(레버 피드백에 그대로 뜬다).
  동승 블록의 `bCanRotate=false`는 **회전을 막는 것으로** 처리한다(붙어 있는데 안 도는 물건은
  회전 경로를 막는 것과 같다).
- 두 프로퍼티 모두 `EditAnywhere`라 클래스 기본값과 인스턴스 값 둘 다 된다.

### 2.3 그리드 — 한 개, 층 겹침 여부가 배치를 정한다

그리드는 셀당 바닥 하나다(`FGridCellData::FloorZ`). 트레이스는 `ActorZ + RegionHeight`에서
`ActorZ`로 내려오며 위에 있는 것이 먼저 맞는다. `FindGrid`는 첫 그리드만 돌려주므로 그리드는 한 개다.

| 측정 결과 | 배치 |
|---|---|
| B1과 B2 바닥이 XY로 **안 겹침** (GreyBoxTest_1처럼 선로를 사이에 두고 갈림) | 그리드 하나가 두 층을 덮는다. 계단·에스컬레이터·엘리베이터가 층을 잇는다 |
| **겹침** (실제 역처럼 B1 대합실이 B2 승강장 위에 있음) | **1차:** 그리드 원점 Z를 B2 바닥 아래, `RegionHeight`를 B1 바닥 **아래**까지만 잡아 B2만 굽고, B1은 겹치지 않는 부분만 넣는다. 겹치는 B1 구역은 걸을 수 없다 → 엘리베이터·에스컬레이터의 도착 셀이 거기에 있으면 5·8번이 막힌다. **2차:** 층별 그리드(`GreyBoxTest_1.md` 6절 설계 스케치, 미구현) |

0단계에서 바닥 메시(`SM_Floor01_B1/B2/B3`, `SM_Floor02_B1/B2`, `SM_Floor_001`, `SM_Floor_Rail`)의
바운딩 박스를 재서 표로 남기고 어느 쪽인지 정한다. 겹치면 **작업을 멈추지 않고** 1차 배치로
진행하되 막히는 항목을 보고한다.

### 2.4 열차 — 새 BP `BP_Train_Subway : AGridTrain`

`AGridTrain`은 생성자에서 `SceneRoot`·`BodyMesh`·`DoorMesh0/1`을 만든다. 네이티브 루트가 있는
클래스로 아트 BP를 리페어런트하면 BP의 `DefaultSceneRoot` 아래 계층이 통째로 사라진다
(`StationFlow.md` 3.10, 2026-09-08). 그래서 **`BP_Subway`를 건드리지 않고** `Content/Core/Vehicle/`에
새 BP를 만들어 `SceneRoot` 아래에 메시 30개를 컴포넌트로 넣는다. 맵의 SM 액터 30개는 그 BP 하나로
바뀐다(상대 트랜스폼은 스크립트로 옮긴다: 각 메시 월드 트랜스폼 × 열차 원점 역행렬).

코드 변경: `bShowProxyBody`(false면 큐브·문 슬랩 숨김, 콜리전은 남겨 클릭 프록시 유지),
`AnimateDoorsOpening/Closing`을 `BlueprintNativeEvent`로 열어 BP가 `SM_Subway*Door_*`를 밀어
연출한다. 승하차 차단은 단계(`ETrainPhase`)가 맡으므로 연출이 비어 있어도 규칙은 그대로다.

전제 확인: 열차는 **X축으로 달리고 문은 +Y 면**이다(`RefreshVisual` 주석). Stage1 선로가 Y축이면
정차 셀은 그대로 쓰되 `BodyLength/BodyWidth`를 바꿔 끼우고 문 면을 고르는 `DoorSide` 프로퍼티가
필요하다(코드 변경 소). 0단계에서 `BP_RailTrack` 방향을 잰다.

### 2.5 에스컬레이터 행인 — 탑승자 인터페이스 + 다중 탑승 + 경로 단계

현재: `AGridEscalator::Rider`는 `TWeakObjectPtr<AGridPawn>` 하나, 좌석 컴포넌트 하나. `AGridNPC`는
`APawn` 파생이고 걷기만 한다(`ERideState` 없음). 경사 구간은 `NoFloor`라 A*가 건너지 못한다.

- **탑승자 인터페이스** `IGridRider`(`BoardVehicle`, `WalkOntoGrid`, `IsRiding`, `IsOnGrid`,
  `GetHeightAboveFloor`)를 `AGridPawn`과 `AGridNPC`가 구현. 에스컬레이터·엘리베이터·열차는
  `AGridPawn*` 대신 이 인터페이스로 말한다. 플레이어 쪽 동작은 바뀌지 않는다.
- **다중 탑승**: `TArray<FEscalatorRide>{Rider, Seat, Distance}`. 새 탑승은 마지막 탑승자가
  `MinRiderSpacing`(기본 150 cm)만큼 나아간 뒤에만 받는다. 플레이어와 행인이 섞여 탄다.
- **행인 경로 단계**: `FGridNPCRouteStep { FIntPoint Cell; TObjectPtr<AGridEscalator> Escalator; }`.
  `Escalator`가 있으면 셀 대신 그 에스컬레이터의 타는 셀로 걸어가 `TryBoard`, 내리면 다음 단계로.
  스포너·NPC의 기존 `Waypoints`(`TArray<FIntPoint>`)는 남겨 두고 `Route`가 비어 있을 때만 쓴다
  (GreyBoxTest_1·FlowTest_1 데이터 호환).
- 행인은 콜리전이 없고 점유자를 통과하므로 에스컬레이터 위에서 플레이어와 겹쳐도 문제없다.

## 3. 코드 변경 목록

| 파일 | 변경 |
|---|---|
| `Puzzle/PuzzleBlock.h/.cpp` | `bCanMove`, `bCanRotate`, `ArtMesh`, `ArtMeshOffset`, `ArtMeshComponent`; `IsUsingArtVisual` 확장; `RefreshVisual`에서 아트 컴포넌트 메시·오프셋·콜리전(None) 적용; `CanSlide`/`GetWorldMoveAxis`가 `bCanMove` 반영 |
| `Puzzle/PuzzleRotatingObstacle.cpp` | `TryRotate` 첫 검사에 `bCanRotate`; 동승 블록 `bCanRotate=false`는 회전 거부 |
| `Puzzle/PuzzleRotatingPillar.cpp` | `RefreshVisual`: 아트가 있으면 원기둥·패널 숨김(콜리전 유지) |
| `Puzzle/PuzzleRotationTile.cpp` | `TryRotate`: 안의 블록 중 `bCanRotate=false`가 있으면 거부 |
| `Player/GridPlayerController.cpp` | 고정 블록을 잡으면 "fixed in place" 사유 출력(드래그 시작 안 함) |
| `Vehicle/GridTrain.h/.cpp` | `bShowProxyBody`; `AnimateDoorsOpening/Closing` → `BlueprintNativeEvent`; (선로 축이 Y면) `DoorSide` |
| **Phase 6** `Grid/GridRider.h`(신규), `Player/GridPawn`, `NPC/GridNPC`, `NPC/GridNPCSpawner`, `Vehicle/GridEscalator` | 2.5절 |

새 BP(모두 `Content/Core/`, 프로그래머 소유): `Puzzle/BP_Block_Bench`, `BP_Block_LBench`,
`BP_Block_VendingMachine`, `BP_RotatingPillar`; `Vehicle/BP_Train_Subway`. 아트 폴더의 에셋은
한 개도 수정하지 않는다(`BP_LEVER001`을 레버 비주얼로 붙이는 건 7절 Q4).

## 4. 단계별 작업

에디터 자동화는 unreal-mcp로 한다. 이번 세션은 연결이 거부됐는데(에디터가 꺼져 있었던 것으로 보임),
에디터가 켜져 있으면 HTTP JSON-RPC로 직접 부를 수 있다. 안 되면 에디터 Python 스크립트를
`Tools/`에 두고 돌린다.

### Phase 0 — 측정 (에디터, 코드 무변경)

1. 바닥 메시별 월드 바운딩 박스(XY 범위, 상면 Z) → **층 겹침 판정**(2.3).
2. `SM_Bench_002`, `SM_LBench_001`, `SM_VendingMachine_Small`, `SM_Pillar_005` 로컬 바운즈와 피벗 위치
   → 풋프린트(셀)·`Height`·`ArtMeshOffset`. L벤치는 직사각 풋프린트(예: 2x2)로 시작한다(7절 Q2).
3. 대상 액터 15 + 11개의 월드 트랜스폼(회전 yaw가 90도 배수인지).
4. `PuzzleElevatorDock` 위치·`SizeInCells`, 주변 바닥과 아래층 바닥 Z, 샤프트 구멍(`EV_*` 메시) 유무.
5. `BP_RailTrack` 방향(X/Y), `SM_Subway1_Body` 길이·폭, 두 량과 Head·joint의 상대 위치, 승강장 면.
6. 에스컬레이터 3대의 위치와 양 끝 층, `Log Boarding Cells` 결과(그리드 생성 후 재실행).
7. 개찰구 15개의 통로 셀, `PlayerStart` 위치, 포스트 프로세스 볼륨·월드 세팅 게임모드 유무.
8. 결과를 이 문서 8절 표에 적는다.

### Phase 1 — 코드 (3절 표, Phase 6 제외) → 에디터·게임 타깃 빌드

### Phase 2 — BP 5종 생성 (`Content/Core`)

블록 3종은 기본값만: `ArtMesh`, `FootprintSize`, `Height`, `ArtMeshOffset`, `MoveAxis=Both`,
`bCanMove=true`, `bCanRotate=true`. 기둥 BP: `ArtMesh=SM_Pillar_005`, `Height`, 부착 면 기본 북.
열차 BP: 메시 30개 컴포넌트, `bShowProxyBody=false`, `BodyLength/Width/Height`를 실측에 맞춤,
문 연출 이벤트 구현(front/back 문을 좌우로 슬라이드).

### Phase 3 — 그리드 배치와 생성

1. `AGridActor`를 정수 m 원점에, `SizeInCells`는 측정 범위 + 여유. `MaxStepHeight`·`MaxSlopeAngle`은
   GreyBoxTest_1 값(100 / 45)에서 시작해 계단 실측으로 조정. `RegionHeight`는 2.3 결정대로.
2. `GridTraceIgnore` 태그: `BP_RailTrack`, 열차 BP(클래스에서 이미 붙음), 에스컬레이터 BP 3대
   (발판이 바닥으로 구워지면 안 됨 — `AGridEscalator`가 태그를 스스로 붙이는지 확인, 아니면 인스턴스
   태그), `SM_SkySphere`, `SM_Escalator_B1`(정적 에스컬레이터 메시라면 그 구간은 `NoFloor`가 돼야 함).
3. 개찰구: 통로 셀마다 `GridCellMarker` Conditional(`PawnHasTag HasTicket`) 15개(또는 통로가
   한 줄이면 `GridBoxMarker` 하나). 개찰구 몸체는 콜리전으로 자동 Blocked.
4. `Generate Grid` → 통계·`Log Debug Report`로 승강장↔대합실 경로가 계단으로 이어지는지 확인.
   장식 메시(카드기·매점·쓰레기통·청소도구)가 원치 않는 셀을 막으면 `GridTraceIgnore` 또는 마커.

### Phase 4 — 액터 교체와 퍼즐 배치

1. 교체 스크립트: 라벨 접두사(`SM_B2_Bench`, `SM_B1_LBench`, `SM_B2_VendingMachine`, `SM_*Pillar*`)별로
   BP를 원본 위치·yaw로 스폰 → 블록이 셀 중심으로 스냅(원본과 최대 반 셀 어긋남) → 원본 SM 액터는
   `_Replaced` 폴더로 옮기고 숨김(`bHidden`, 에디터 전용 표시). PIE 확인 뒤 삭제한다(맵은 git으로
   되돌릴 수 있다).
2. `APuzzleRegion`: 퍼즐 구간마다 하나(블록이 구간 밖으로 못 나감). 구간이 없으면 경고만 나고 제한
   없음 — 1차는 엘리베이터 구간 하나로 시작.
3. 기둥마다 `APuzzleLever`(`Target`=기둥, `Direction`) 배치. 돌지 않을 기둥은 레버 없이
   `bCanRotate=false`. 고정 벤치·자판기는 인스턴스에서 `bCanMove=false`.
4. 엘리베이터: `APuzzleElevatorBlock`(`VisualActorClass=BP_elevator`, 4x4, `DoorDirection`)을 구조물과
   같은 구간 안 같은 층에 배치. 구조물: `Detect Target Floor`로 `TargetFloorCell` 채우기, 안 되면
   `bTravelUp=false` + `TravelHeight`=층 간격, `ExitDirection`. 도착 층 출구 셀이 Walkable인지 확인.
5. `PlayerStart`를 Walkable 셀 위로. 월드 세팅 게임모드가 `GridTestGameMode`(전역 기본)인지, 툰 포스트
   프로세스 볼륨이 있는지(FlowTest_1 12절과 같은 설정).

### Phase 5 — 열차 배치

SM 액터 30개 제거 → `BP_Train_Subway` 1대. `Stops` 2개(정차 셀·`BoardingCells`·`ExitCell`·
`PostExitCell`), 하차 스포너(`OnDemand`) 1개. 맵 밖 대기 정차역이 필요하면 선로 끝 셀을 쓴다.

### Phase 6 — 에스컬레이터 행인 (독립, 뒤로 미룰 수 있음)

2.5절 코드 → 빌드 → 스포너 `Route`에 에스컬레이터 단계 넣어 대합실↔승강장 왕복 행인 배치.
FlowTest_1의 기존 행인이 그대로 도는지 회귀 확인.

### Phase 7 — 검증 (5절) · 문서 갱신 · 커밋은 지시가 있을 때

## 5. 검증 체크리스트 (PIE, `ltts.GridDebug 2`)

자동(로그·통계):
- [ ] 빌드 성공(에디터·게임). 기존 맵(GreyBoxTest_1·FlowTest_1) 그리드 통계 변화 없음.
- [ ] Stage1 `Generate Grid` 통계: walkable/blocked/nofloor 수, 오버라이드 무시 경고 0.
- [ ] `Log Debug Report`: PlayerStart 셀 → 승강장 셀 경로 있음, 선로 셀 NoFloor.
- [ ] 교체된 블록 15 + 기둥 11이 서브시스템에 등록되고 점유 셀이 원본 메시 자리와 일치.
- [ ] 아트 블록의 프록시 큐브 `bVisible=false`, 아트 컴포넌트 콜리전 `NoCollision`.
- [ ] 엘리베이터 `FindDockUnder` 성공, `TargetFloorCell` 바닥 Z가 아래층과 일치.
- [ ] 열차: 정차·문 개폐 단계 전이 로그, 하차 스포너 호출.
- [ ] 에스컬레이터 3대 `Log Boarding Cells`가 Walkable 셀을 낸다.

손(마우스):
- [ ] `bCanMove=false` 벤치를 잡으면 "fixed in place", 움직이는 벤치는 네 방향으로 한 칸씩.
- [ ] 레버로 기둥이 90도 돌고 부착 면의 벤치가 함께 돈다. `bCanRotate=false` 기둥은 거부 문구.
- [ ] 엘리베이터를 구조물 위로 밀면 패드 색이 바뀌고, 문 앞에서 클릭하면 **내려가** 아래층 출구 셀에 선다.
- [ ] 열차 문이 아트 메시로 열리고 닫히며, 열린 동안만 탑승된다.
- [ ] 에스컬레이터를 타면 발판 위에서 실려 위층에 내린다. (Phase 6) 행인이 뒤따라 탄다.
- [ ] 개찰구는 `HasTicket` 태그 없이는 막힌다.

## 6. 리스크

| 리스크 | 영향 | 대응 |
|---|---|---|
| B1·B2 XY 겹침 | 4·5·8번 범위 축소 | 2.3 1차 배치 후 보고. 층별 그리드는 별도 계획 |
| 아트 메시 크기가 셀 배수가 아님 | 블록이 이웃 셀을 시각적으로 침범하거나 빈 셀을 남김 | 풋프린트는 올림, 오프셋으로 중앙 맞춤. 기둥은 1x1 고정이라 1 m 초과 시 사용자 판단 |
| 원본 배치가 셀 격자에 안 맞음 | 스냅 후 최대 반 셀 이동, 벽과 겹칠 수 있음 | 교체 스크립트가 원본과의 편차를 로그로 남기고 50 cm 초과는 목록으로 보고 |
| 선로가 Y축 | 열차 코드 전제 위반 | `DoorSide`/축 프로퍼티 추가(소) |
| 열차 아트 두 량 길이가 `BodyLength` 기본(16 m)과 다름 | 클릭 프록시와 문 위치 어긋남 | 실측으로 `BodyLength`, `BoardingCells` 재지정 |
| 에스컬레이터 발판이 층 간격에 안 맞음(FlowTest에서 6.1 m vs 8 m) | 착지 셀 못 찾음 | Stage1 층 간격 실측 후 `RidePathLocal` 끝점 조정 |
| 다중 탑승 개조로 플레이어 탑승 회귀 | 8번 | 플레이어 단독 탑승 테스트를 FlowTest_1에서 먼저 |

## 7. 결정 필요 (기본값으로 진행하고 보고)

- **Q1.** 요청에 없는 `SM_B1_Bench`, `SM_B2_LBench`, `SM_B1_VendingMachine`, `SM_VendingMachine_Small1`,
  `SM_Bench_002`, `SM_LBench_001`도 바꿀지. **기본: 안 바꾼다**(스태틱 유지, 콜리전으로 Blocked).
- **Q2.** L벤치 풋프린트를 직사각으로 둘지(빈 모서리 셀도 점유), L자 점유를 위해
  `GatherOccupiedCells` 오버라이드 BP 클래스를 둘지. **기본: 직사각.**
- **Q3.** 기둥의 부착 면 패널(빗금)을 아트 위에도 보일지. **기본: 숨김**, 회전판/레버 피드백으로만 알림.
- **Q4.** 레버 비주얼: 그레이박스 레버 그대로 vs `BP_LEVER001`을 `APuzzleLever`의 자식 액터로
  붙이는 `VisualActorClass` 추가(코드 소). **기본: 그레이박스**, 요청 시 추가.
- **Q5.** 기둥 11개 중 실제로 돌릴 기둥. **기본: 전부 BP로 바꾸되 레버는 퍼즐 기둥에만**, 나머지는
  `bCanRotate=false`.
- **Q6.** 열차 `SM_Subway_Head`·`joint`가 두 량 사이·끝에 붙는 구조라면 한 액터로 묶는 데 문제
  없음. 량이 따로 움직여야 하면(연결부 흔들림) 범위 밖.

## 8. 측정 결과 (2026-09-10, unreal-mcp HTTP + trace_world)

에디터 액터 257개의 라벨·클래스·트랜스폼·바운즈를 읽고, 그리드와 같은 규칙(위에서 아래로
라인 트레이스, 가장 높은 것이 이긴다)으로 바닥 높이를 찍어 확인했다.

### 8.1 층 구조 — **겹치지 않는다. 그리드 하나로 충분하다**

2.3절의 걱정은 기우였다. B1 바닥 메시는 얇은 판이 아니라 **Z −30에서 770까지 꽉 찬 블록**이다.
즉 B1이 있는 자리에는 B2 공간이 아예 없다. 트레이스는 B1이 있으면 772를, 없으면 B2의 −26을
맞는다. 한 셀에 바닥 하나라는 그리드의 전제와 정확히 맞아떨어진다.

| 구역 | B1 (Z ≈ 771) | B2 (Z ≈ −26) | 선로 |
|---|---|---|---|
| 동쪽 | X[−2893,1912] Y[−2473,2127] 대합실·개찰구 | X[−484,1904] Y[2200,5915] 승강장 | `SM_Floor_Rail` X[−1686,−886] Y[726,9526] Ztop −199 |
| 서쪽 | X[−11486,−5797] Y[7126,9526] 대합실·개찰구 | X[−6485,−1694] Y[7131,9519] 승강장 | 같음 |

경계는 칼같이 나뉜다. 동쪽은 X=700에서 Y 2000까지 772, Y 2400부터 −26. 서쪽은 Y=8500에서
X −5800까지 773, X −5400부터 −26.

### 8.2 그리드 배치 (배치 완료)

`StationGrid`(`AGridActor`), 액터 위치 **(−11500, −2500, −250)**, `SizeInCells` **140 × 125**
(140 m × 125 m), `RegionHeight` 1100(트레이스 850 → −250), `MaxStepHeight` 100,
`MaxSlopeAngle` 45, 클리어런스 180/40.

| 통계 | 값 |
|---|---|
| walkable | 3793 |
| blocked | 7057 (클리어런스 7049, 경사 8) |
| noFloor | 6650 |
| stepBreaks | 290 |

`GridTraceIgnore` 태그를 33개 액터에 붙였다: `SM_Subway*` 30개, `BP_RailTrack`, `SM_SkySphere`,
`SM_Floor_Rail`. 에스컬레이터·열차·퍼즐 조각·바닥 타일·레버·구간은 클래스 생성자에서 스스로
붙이므로 손댈 필요가 없다.

### 8.3 엘리베이터 — 구조물 위치는 정확하고, 차체는 **B1에서 서쪽으로 밀어 넣는다**

`PuzzleElevatorDock`(−690, 1920)은 셀 **X[106,109] × Y[42,45]** 를 덮는다. 그 4×4는 B1 바닥에
뚫린 구멍이고 바닥이 B2(−29)다. 즉 **샤프트**다. 주변은 남쪽(Y41) B1 770, 동쪽(X110) B1 772,
서쪽(X105) 선로 −199, 북쪽(Y46+) B2 −29로 승강장과 이어진다.

사용자가 원한 흐름은 **B1 대합실에 서 있는 엘리베이터(1번)를 샤프트 쪽으로 밀어 넣은 뒤 타고
내려가는 것**이다(2026-09-10 채팅, 그림). 차체는 사용자가 직접 놓은 `ElevatorCar2`
(`PuzzleElevatorBlock_1`, 셀 X[110,113] Y[42,45], Z 772, yaw 90)를 그대로 쓴다. yaw 90이라
문이 동·서 면에 놓이고 이동 축이 X가 되어 서쪽으로 밀린다.

문제는 `APuzzleElevatorBlock::CanOccupyRect`였다. 차체는 자기 층과 같은 높이의 바닥으로만
나갈 수 있는데 샤프트 셀은 −29로 구워지므로 772에 있는 차체가 들어갈 수 없었다. 그래서
**목적지 사각형이 구조물 하나에 완전히 덮이면 바닥 높이를 따지지 않도록** 예외를 두었다
(`UPuzzleSubsystem::FindDockCovering`). 슬라이드 목표와 스냅은 차체가 기억하는 FloorZ를 쓰므로
차체는 772에 걸린 채 샤프트 위에 멈추고, 실제 승강은 구조물이 지시한다.

| 항목 | 값 |
|---|---|
| 구조물 | 셀 (106,42) 4×4, BeginPlay에서 Z −29로 스냅 |
| `TargetFloorCell` | (110,43) — B1 772, "반대편 층" |
| `ExitDirection` | North — 내려온 뒤 (108,46) B2로 나간다(마지막 수단 경로) |
| 퍼즐 구간 | `ElevatorShaft` 셀 (106,42) **8×4** — 샤프트 + 차체 출발 자리 |
| 이동 거리 | 772 → −29 = **801 cm** |

흐름: 차체를 서쪽으로 4칸 밀어 구조물에 올린다 → 패드 색이 바뀐다 → 동쪽 문 앞 (110,43)에서
차체를 클릭한다 → 8 m 내려간다 → 북쪽 (108,46) 승강장에 내린다.

**한 방향이다.** 내려간 차체는 샤프트 바닥에 남고, B2에서는 문 앞 셀(X105 선로·X110 B1)에 설 수
없어 다시 탈 수 없다. 위로 돌아가는 길은 에스컬레이터다. 처음 배치했던 B2 승강장의 차체
(`ElevatorCar`)는 삭제했다.

### 8.4 열차 — **선로가 Y축이다**

`SM_Subway*` 30개는 전부 같은 원점 **(−1275, 5989, −178)**, yaw −90, 스케일 (1.5, 1.5, 1.3)에
겹쳐 있다. 하나의 조립품이라 BP 하나로 묶기 쉽다. 차체 월드 AABB는 X 600 × Y 2271 × Z 727이고
선로(`SM_Floor_Rail` X[−1686,−886] Y[726,9526])도 Y로 뻗는다.

`AGridTrain`은 "X축으로 달리고 문은 +Y 면"을 전제하지만, **액터를 yaw 90으로 돌려 배치하면
그대로 맞는다**(로컬 X가 월드 Y가 된다). 코드 변경은 필요 없다. `BodyLength` 2271,
`BodyWidth` 600, `BodyHeight` 727.

### 8.5 아트 메시 치수 → 블록 설정

모든 대상 메시의 피벗이 **XY 중심 · Z 바닥**으로, `APuzzleBlock`의 규약(액터 위치 = 풋프린트
중심, Z = 바닥)과 정확히 같다. `ArtMeshOffset`은 항등으로 둔다.

| 메시 | 실측 (cm) | 풋프린트(셀) | Height |
|---|---|---|---|
| `SM_Bench_002` | 439 × 103 × 195 | 5 × 1 | 200 |
| `SM_LBench_001` | 406 × 408 × 195 | 4 × 4 | 200 |
| `SM_VendingMachine_Small` | 307 × 194 × 595 | 3 × 2 | 600 |
| `SM_Pillar_005` | 200 × 200 × 800 | **2 × 2** | 800 |

기둥이 2 m라 `APuzzleRotatingPillar`의 1×1 고정을 **정사각형 허용**으로 완화했다(3절, 9.2절).

### 8.6 개찰구

동쪽은 셀 행 Y 25–26에 게이트 본체가 X 114·119·120·122·125·127·130, 서쪽은 셀 열 X 16–17에
Y 100·102·105·107·110·112·113·118. 그 사이가 걸을 수 있는 통로다. `AGridBoxMarker` 3개를
Conditional + `RuleClass = GridCellRule_PawnHasTag`(기본 태그 `HasTicket`)로 덮었다.

### 8.7 그 밖에 발견한 것

- `Place_EV01`(164, −1874)·`Place_EV06`(164, 1926)·`Place_EV02~05`(서쪽)은 아트가 남긴
  엘리베이터 자리 표시다. 구조물이 놓인 샤프트와는 별개의 후보 지점이다.
- `EndingPoint01~03`, `Place_Escalator001_B1`, `AC_T_*`도 표식 액터다.
- 에스컬레이터 3대: `BP_escalator_up_s`(1340, 2750)·`BP_escalator_down_s`(1720, 2750)는 동쪽
  B2에, `BP_escalator_down_s2`(−5270, 7330)는 서쪽 B2에 있다. 동쪽 두 대는 Y 2750에 있는데
  그 자리에는 B1 바닥이 없어(대합실은 Y 2127에서 끝난다) 위쪽 착지 셀이 없다. FlowTest_1에서
  겪은 것과 같은 문제다(`StationFlow.md` 14.5).
- 라이팅·툰 설정은 이미 있다: `DirectionalLight`, `SkyLight`, `SkyAtmosphere`, `PPV_Toon`,
  카메라 `CAM_ToonIso`·`CAM_Isometric`.

---

## 9. 구현 결과 (2026-09-10)

계획의 Phase 0~5를 실행했다. **Phase 6(행인 에스컬레이터 탑승)은 하지 않았다** — 2.5절 설계
그대로 남아 있다.

### 9.1 코드 변경 (10개 파일)

| 파일 | 변경 |
|---|---|
| `Puzzle/PuzzleBlock.h/.cpp` | `bCanMove`·`bCanRotate` 토글, `ArtMesh`·`ArtMeshOffset`·`ArtMeshComponent` 아트 슬롯, `HollowOffset`·`HollowSize` 빈 영역, `LocalToWorldOffset`·`GetWorldHollowRect`, `GatherOccupiedCells`가 빈 영역을 건너뜀, `CanSlide`·`GetWorldMoveAxis`가 `bCanMove` 반영, 구간 탐색을 첫 틱으로(`ResolveHomeRegion`) |
| `Puzzle/PuzzleRotatingObstacle.h/.cpp` | `TryRotate`에 `bCanRotate` 검사 두 개(자기 자신·동승 블록), 중복 `LocalToWorldOffset` 제거(부모 것을 상속) |
| `Puzzle/PuzzleRotatingPillar.h/.cpp` | 풋프린트 **정사각형 허용**(1x1 고정 해제), 부착 면 판정을 면 전체로 일반화, 비주얼이 한 변 길이를 따르도록, 아트가 있으면 원기둥·빗금 패널 숨김 |
| `Puzzle/PuzzleRotationTile.cpp` | 타일 위 블록 중 `bCanRotate=false`가 있으면 회전 거부 |
| `Puzzle/PuzzleElevatorBlock.cpp` + `Puzzle/PuzzleSubsystem.h/.cpp` | 목적지가 구조물(샤프트)에 완전히 덮이면 바닥 높이 검사를 건너뜀(`FindDockCovering`). 위층에서 샤프트로 밀어 넣는 흐름을 위해 |
| `Puzzle/PuzzleFloorTile.cpp`, `Puzzle/PuzzleLever.cpp`, `Puzzle/PuzzleRegion.cpp`, `Grid/GridActor.cpp`, `Authoring/GridCellMarker.cpp` | `PostEditMove`·`PostEditChangeProperty`에 `IsTemplate()` 가드 |
| `Authoring/GridCellMarker.h` + `Grid/GridActor.cpp` | 마커에 `RuleClass` 추가. `Rule`(Instanced 서브오브젝트)이 비어 있으면 굽는 시점에 클래스 기본값으로 만든다 |
| `Player/GridPlayerController.cpp` | 고정 블록을 끌면 "This object is fixed in place." |
| `Grid/GridActor.cpp` | 생성 트레이스가 `ANavigationObjectBase`(PlayerStart 등)를 무시. 캡슐이 트레이스 시작점에 걸리면 셀이 트레이스 꼭대기 높이에 떠 버린다 |
| `Puzzle/PuzzleSubsystem.h/.cpp` | `FindDockCovering(Rect)` → `IsDockCell(Cell)`. 차체가 한 칸씩 움직이므로 샤프트 판정은 셀 단위여야 한다(9.10) |
| `Art/ArtMaterialUtil.h`(신규) | `ReplaceDefaultMaterials` — 엔진 기본 머티리얼인 슬롯만 교체(9.11) |
| `Vehicle/GridTrain.h/.cpp` | 문 오프셋으로 탑승 셀 자동 계산, 진행도 기반 이동 + 속도 커브, `ArtFallbackMaterial`(9.11) |
| `Player/GridPlayerController.h/.cpp` | `FCursorPick::VehicleRefusal` — 열차 탑승 거부 사유를 화면에 띄운다(9.11) |
| `Vehicle/GridTrain.h/.cpp` | `bShowProxyBody`, 문 연출 훅 두 개를 `BlueprintNativeEvent`로 |

**`IsTemplate()` 가드는 실제로 에디터를 죽이던 버그를 고친 것이다.** 블루프린트의 Class
Defaults를 편집하면 CDO에 `PostEditChangeProperty`가 오고, 그것이 `PostEditMove`를 부르는데
CDO에는 월드도 루트 컴포넌트도 없다. 이번 작업 중에 실제로 크래시했다(2026-09-10).

### 9.2 새 에셋 (`Content/Core`, 프로그래머 소유)

| 에셋 | 부모 | 기본값 |
|---|---|---|
| `Puzzle/BP_Block_Bench` | `APuzzleBlock` | `SM_Bench_002`, 5x1, 높이 200 |
| `Puzzle/BP_Block_LBench` | `APuzzleBlock` | `SM_LBench_001`, 4x4, **빈 영역 (0,0) 3x3**, 높이 200 |
| `Puzzle/BP_Block_VendingMachine` | `APuzzleBlock` | `SM_VendingMachine_Small`, 3x2, 높이 600 |
| `Puzzle/BP_RotatingPillar_Station` | `APuzzleRotatingPillar` | `SM_Pillar_005`, **2x2**, 높이 800, 부착 면 북 |
| `Vehicle/BP_Train_Subway` | `AGridTrain` | 메시 컴포넌트 30개, 프록시 숨김, 4525x600x727 |

L벤치의 빈 영역은 실측으로 정했다. 빈 레벨에 `SM_LBench_001`을 하나 놓고 50 cm 격자로
트레이스한 결과, 4x4 중 **좌하단 3x3이 비어 있는 L자**였다. 그 덕분에 같은 자리에 있는 2x2
기둥과 셀이 겹치지 않는다.

### 9.3 레벨 배치 (`Subway_Stage1`)

- **`StationGrid`** (−11500, −2500, −250), 140x125셀, RegionHeight 1100, Step 100, Slope 45.
- **블록 25개**: 벤치 7 + L벤치 4 + 자판기 5 + 기둥 9. 원본 스태틱 메시 액터는 삭제했다.
  겹치는 셀 **0개**(스크립트로 전수 검사), 점유 셀 129개.
- **`ElevatorCar2`**(`APuzzleElevatorBlock`, 사용자 배치) 셀 (110,42) 4x4 B1, yaw 90, 아트 `BP_elevator`.
- **`ElevatorRegion`**(`APuzzleRegion`) 'ElevatorShaft' 셀 (106,42) 8x4.
- **`PuzzleElevatorDock`**: 위치는 그대로 두고 `TargetFloorCell` (110,43), 출구 North.
- **`Train_Line1`**(`BP_Train_Subway`) (−1250, 3750, −124) yaw −90, 정차역 2개.
  느슨한 `SM_Subway*` 액터 30개는 삭제했다.
- 마커 4개: 개찰구 3개(동쪽 2·서쪽 1)와 `ShaftMouth_Walkable` 1개.
- `PlayerStart`를 개찰구 셀 밖(50, −250)으로 옮겼다.

최종 그리드 통계: **walkable 3917 · blocked 6933 · noFloor 6650 · 오버라이드 68**.

### 9.4 자동으로 확인한 것

| 항목 | 결과 |
|---|---|
| 게임 타깃 빌드 | **성공** |
| 에디터 타깃 빌드 | **성공** |
| PIE 실행 | 오류 0건 |
| 블록 25개 등록 | 풋프린트·회전 횟수 모두 의도대로(5x1 / 4x4 / 3x2 / 2x2) |
| 엘리베이터 구간 소속 | `PuzzleElevatorBlock_1: 4x4 block at cell (110,42), 1 quarter turn(s)` / `region ElevatorShaft` |
| 구조물 셀 | 4x4 모두 걸을 수 있음(경고 0) |
| 열차 | 두 정차역을 오가며 문 개폐 반복 |
| 에스컬레이터 3대 | 각각 타는 셀 3개씩 찾음 |
| 뷰포트 | 열차·엘리베이터·벤치·기둥이 아트 메시로 보이고 그레이박스는 숨겨짐 |
| 회귀: `GreyBoxTest_1` | 기둥·레버·회전 장애물·엘리베이터·행인 모두 이전과 같이 동작 |
| 회귀: `FlowTest_1` | 구간 소속(`region StationA`) 3건 정상, 구조물·열차·에스컬레이터 정상 |

### 9.5 계획과 달라진 점

1. **층이 겹치지 않았다.** B1 바닥이 두께 800 cm의 꽉 찬 블록이라 그리드 하나로 두 층이 모두
   덮인다(8.1절). 2.3절이 대비하던 축소 배치는 필요 없었다.
2. **선로는 Y축이다.** 하지만 `DoorSide` 프로퍼티는 필요 없었다 — 액터를 yaw −90으로 놓으면
   로컬 X가 월드 −Y가 되어 기존 전제가 그대로 맞는다.
3. **기둥 1x1 고정을 풀었다.** 아트 기둥이 2 m라 어쩔 수 없었다(7절 Q 대응).
4. **L벤치를 직사각형으로 두지 않았다**(Q2의 기본값과 다름). 같은 자리에 기둥이 있어 겹쳤기
   때문이다. 대신 `HollowOffset`/`HollowSize`를 추가했다.
5. **개찰구를 Conditional이 아니라 Walkable로 두었다.** 티켓을 얻을 수단이 아직 없어서
   Conditional로 두면 승강장에 갈 수 없다. 마커와 `RuleClass`는 그대로 있으므로 티켓 기능이
   생기면 `CellType`만 Conditional로 바꾸면 된다.
6. **구간 소속을 첫 틱에 정하도록 바꿨다.** 액터의 BeginPlay 순서가 정해져 있지 않아, 블록이
   구간보다 먼저 돌면 "구간 없음"으로 굳었다. Stage1에서 실제로 그랬다.

### 9.6 남은 문제

1. **블록 6개가 벽에 걸쳐 있다.** 아트가 벽에 딱 붙여 놓은 것들이라 셀 일부가 걸을 수 없는
   칸이다: `LBench_SM_B1_LBench001`(2칸), `Pillar_SM_B1_Pillar001`(2칸),
   `Pillar_SM_B1_Pillar3`(2칸), `Vending_SM_B2_VendingMachine009`(4칸),
   `Pillar_SM_Pillar03_B4`(1칸), `LBench_SM_B1_LBench002_E`(1칸), `Vending_..._005`(1칸).
   놓여 있는 데는 문제가 없고 다른 조각이 그 칸으로 들어오지 못할 뿐이다. 한 칸씩 안쪽으로
   옮기거나 그 인스턴스의 `bCanMove`를 끄면 정리된다.
2. **빈 영역이 있는 블록도 프록시 큐브는 풋프린트 전체를 덮는다.** L벤치의 빈 자리에 선
   기둥과 커서 판정이 겹친다. 기둥이 더 높아서 대개 기둥이 잡히지만, 정확히 하려면 몸체를
   슬랩 여러 장으로 나눠야 한다(회전 장애물이 그렇게 한다).
3. **동쪽 에스컬레이터 두 대의 윗끝이 허공이다.** (1340, 2750)·(1720, 2750)에 있는데 그 자리
   위로는 B1 바닥이 없다(대합실은 Y 2127에서 끝난다). `FlowTest_1`에서 겪은 것과 같은
   문제이며(`StationFlow.md` 14.5), 배치를 옮길지 층을 늘릴지는 기획 결정이다.
4. **레버가 하나도 없다.** 기둥 9개는 모두 `APuzzleRotatingPillar`가 됐지만 레버가 없으면
   돌지 않는다. 어느 기둥을 퍼즐로 쓸지 정해지면 `APuzzleLever`를 놓고 `Target`에 연결한다.
5. **퍼즐 구간은 엘리베이터 샤프트 하나뿐이다.** 나머지 블록 24개는 "구간 없음"이라 제한 없이
   밀린다. 승강장마다 구간을 두면 조각이 역 전체를 돌아다니지 않는다.
6. **열차의 두 번째 정차역은 터널이다.** 승하차 셀이 없어 연출용이다. 반대편 승강장을 정차역으로
   쓰려면 서쪽 구역 셀을 지정하면 된다.
7. **행인이 없다.** `AGridNPCSpawner`를 하나도 놓지 않았다. Phase 6과 함께 하는 편이 낫다.

### 9.8 뜬 셀 수정 (2026-09-10 저녁)

대합실 셀 (112~120, 20~22)가 공중에 떠 있었다(`floorZ=850`, 트레이스 꼭대기 높이 = 시작점
관통). 렌더 메시는 없는데 물리 오버랩 쿼리에 개찰구 `SM_TicketGate003~006_B1`이 잡혔다.
**`SM_Ticket_Gate_Lane`·`SM_Ticket_Gate_Lane_003`의 단순 콜리전이 메시 원점에서 약 (−740, −500) cm
떨어진 자리에 만들어져 있었다**(로컬 바운즈는 100×200×203인데 콜리전은 7 m 밖). 두 메시를
`Content/Developers/User/Backup/*_bak_20260910`으로 복제해 두고 콜리전을 지운 뒤 볼록 껍질(4개)로
다시 만들었다. 개찰구 본체는 여전히 자기 셀을 막고(예: (114,26) Blocked), 뜬 셀은 770/772로
내려왔다.

같은 김에 개찰구 마커를 **통로 셀만 덮는 11개**로 다시 짰다. 이전 박스 3개는 게이트 본체까지
덮어서 본체 위(Z 850)에 걸을 수 있는 셀을 만들고 있었다. `PlayerStart`도 생성 트레이스에서
무시하도록 코드를 고쳤다(9.1). 사용자가 놓은 Walkable 마커 2개(`GridBoxMarker`, `GridBoxMarker2`)는
이제 없어도 되지만 그대로 두었다.

월드 세팅 `DefaultGameMode`를 `GridTestGameMode`로 명시했다. FlowTest_1은 오버라이드 없이
프로젝트 기본값(`DefaultEngine.ini`의 `GlobalDefaultGameMode`)을 쓰고 있어 같은 클래스다.

### 9.9 카메라 — 폰을 따라가는 아이소메트릭 (2026-09-10 저녁)

`AGridPawn`은 생성자에서 `USpringArmComponent`(길이 1800, 절대 회전 −55/45)와 `UCameraComponent`를
만들어 아이소메트릭 구도를 고정한다. FlowTest_1에는 `CameraActor`가 하나도 없어서 그 폰 카메라가
그대로 쓰인다.

Subway_Stage1에서는 `CAM_ToonIso`의 `AutoActivateForPlayer`가 **Player0**이라 폰이 possess된 뒤에도
그 고정 카메라가 뷰 타깃을 가져가고 있었다. 그래서 카메라가 플레이어를 따라가지 않았다.

`AutoActivateForPlayer`를 **Disabled**로 바꿨다(`CAM_Isometric`은 이미 Disabled였다). **두 카메라
액터는 지우지 않고 그대로 두었다** — 디버그·구도 참고용이고 나중에 다시 쓸 것이기 때문이다.
되돌리려면 그 값을 Player0으로 되돌리면 된다.

### 9.10 엘리베이터를 밀 수 없던 이유 두 가지 (2026-09-10 저녁)

드래그해도 차체가 한 칸도 가지 않았다. 원인이 둘이었다.

**(1) 아트 배치 표식이 바닥으로 구워지고 있었다.** `Place_EV06`은 4×4 m 그레이박스 큐브
(`Cube_014`, Z 751~791)로 만든 엘리베이터 자리 표식인데 콜리전이 있어 그 셀들이 **791**로
구워졌다. 차체는 그 위(791)에 서 있고 서쪽 B1 바닥은 **772**라 19 cm 차이가 났다.
`APuzzleElevatorBlock::CanOccupyRect`의 허용 오차는 10 cm이므로 "The floor over there is at a
different level."로 거부된다. `Place_*`·`EndingPoint*`·`AC_*` 표식 액터 12개에
`GridTraceIgnore`를 붙였고, 이제 X 110~120 구간이 전부 772로 균일하다.

**(2) 샤프트 예외가 사각 영역 전체에만 적용됐다.** 8.3절에서 넣은
`FindDockCovering(Rect)`는 목적지 **전체**가 구조물 안일 때만 높이 검사를 건너뛰었다. 그런데
차체는 한 칸씩 움직이므로 샤프트에 들어가는 동안 반드시 걸친 상태를 지난다(예: X[109,112]는
샤프트 셀 하나와 B1 셀 셋). 그래서 첫 걸음부터 막혔다. `UPuzzleSubsystem::IsDockCell(Cell)`로
바꿔 **셀 단위**로 건너뛴다.

함께 조정한 것:

- 퍼즐 구간 `ElevatorShaft`를 **13×4**(셀 (106,42))로 넓혔다. 차체가 서 있던 X[115,118]부터
  샤프트 X[106,109]까지를 담는다. 동쪽으로는 X 118에서 막힌다.
- 차체 Z를 표식 패드 높이 791에서 실제 바닥 **772**로 맞췄다.
- 표식을 그리드에서 뺀 결과 walkable 3907 → **4016**, stepBreaks 155 → **135**.

### 9.11 열차 탑승 · 가감속 · 툰 머티리얼 (2026-09-10 저녁)

**(1) 열차에 못 타던 이유.** `Stops[0].BoardingCells`에 손으로 적어 둔 셀 4개
`(106,50) (106,51) (106,73) (106,74)`는 **그레이박스 프록시 문**(차체 길이의 ±25 %) 자리였다.
아트 문 8짝은 승강장 쪽 셀 X 106의 Y 44~48 · 53~57 · 67~71 · 76~80에 있어 하나도 겹치지 않았다.
게다가 `PickUnderCursor`는 `CanBoard`가 참일 때만 열차를 잡고 거짓이면 사유를 버린 채 뒤쪽 바닥
클릭으로 흘려 보내므로, 문 앞에서 눌러도 폰이 그냥 걸어가고 아무 설명도 나오지 않았다.

- `AGridTrain`에 **`DoorOffsetsLocal`**(진행축 기준 문 중심까지의 cm)과 `DoorWidth`를 추가하고,
  `ComputeBoardingCells`가 정차 위치·액터 회전·차체 반폭에서 승강장 셀을 계산한다. 걸을 수 없는
  셀이면 바깥으로 두 칸까지 더 본다. 저작된 `BoardingCells`가 있으면 그쪽이 이긴다(FlowTest_1의
  손 저작 4셀은 그대로 유지되는 것을 회귀로 확인했다).
- `FCursorPick`에 `VehicleRefusal`을 추가했다. 열차를 잡지 못할 때 사유를 함께 들고 나와, 바닥
  클릭이 처리될 때 화면에 띄운다. 픽 규칙 자체는 그대로다(차체가 승강장 셀을 가리므로).
- `BP_Train_Subway` 기본값: `DoorOffsetsLocal = [-1608.5, -739, 704, 1574]`(4개 양문),
  `DoorWidth = 390`. `Train_Line1`의 `Stops[0].BoardingCells`는 비워 자동 계산이 들어가게 했다.
  PIE 로그로 **탑승 셀 20개**가 아트 문 앞에 정확히 잡히는 것을 확인했다.

**(2) 가감속.** `Tick`이 `VInterpConstantTo`로 정속 이동하고 있어 진행도 개념이 없었다. 구간을
기억하도록(`MoveFrom`/`MoveTo`/`MoveLength`/`MoveDistance`) 바꾸고 진행도 0~1에 대한 속도 배율을
곱한다. `Speed`는 이제 **최고 속도**다.

- `SpeedCurve`(`UCurveFloat`, X=진행도 Y=배율)를 지정하면 그쪽이 이긴다. 비워 두면
  `AccelFraction`(0.30)·`DecelFraction`(0.15)·`EaseExponent`(2)로 만드는 기본 곡선을 쓴다.
  출발은 길게 가속하고 도착은 짧게 급감속한다.
- **`MinSpeedFactor`는 0.05가 아니라 0.25다.** 구간 통과 시간은 배율의 **조화평균**에 좌우되므로
  양 끝에서 배율이 0에 가까우면 그 짧은 구간이 전체 시간을 삼킨다. 0.05로 두었더니 50 m 구간이
  4.2초가 아니라 **17.2초**가 걸렸다(실측). 0.25에서 **7.92초**로, 정속 대비 1.9배다.
- 커브 자산은 MCP로 만들 수 없어 넣지 않았다. 디자이너가 콘텐츠 브라우저에서 Curve Float를
  만들어 `Train|Motion`의 `Speed Curve`에 넣으면 그때부터 커브가 이긴다.

**(3) 회색 격자(WorldGridMaterial).** 아트 메시 자산 자체가 모든 슬롯에 머티리얼이 없고, 원본
스태틱 메시 액터는 **컴포넌트 오버라이드**로 `MI_ToonSurface_Level001`을 덮어 쓰고 있었다. BP로
교체하면서 메시만 옮기고 오버라이드는 옮기지 않아 기본 격자가 그대로 드러났다.

- `Art/ArtMaterialUtil.h`에 `LTTSArt::ReplaceDefaultMaterials`를 두었다. **엔진 기본 머티리얼인
  슬롯만** 교체하므로 아트가 일부만 칠해 둔 메시의 작업을 지우지 않고, 여러 번 불러도 결과가 같다.
- `APuzzleBlock`·`AGridTrain`에 `ArtFallbackMaterial`을 추가했다. C++ 기본값은 비워 두고
  (README: Core → Art 하드 참조 지양) BP 기본값에서 `MI_ToonSurface_Level001`을 지정한다.
  블록은 `RefreshVisual`에서, 열차는 `OnConstruction`에서 적용한다(프록시 큐브와 문 슬랩은 제외).

**주의:** 이미 배치된 인스턴스는 **새로 추가된 프로퍼티의 CDO 값을 상속하지 않는다.** 블록 25개와
엘리베이터, 열차 인스턴스에는 값을 따로 넣어야 했다. 새 프로퍼티를 BP 기본값으로 추가할 때마다
같은 일이 생긴다.

**주의 2:** Live Coding 패치는 **메모리에만** 남는다. 에디터를 다시 켜면 디스크의 에디터 DLL로
돌아가므로 새 프로퍼티가 사라지고 레벨에 저장된 값도 함께 날아간다. 작업을 마칠 때는 에디터를 닫고
`Build.bat LetsTakeTheSubwayEditor Win64 Development`로 에디터 타깃을 다시 빌드해 두어야 한다.

### 9.7 손으로 확인해야 할 것 (마우스)

- [ ] 벤치·자판기를 드래그하면 한 칸씩 밀린다.
- [ ] 인스턴스에서 `bCanMove`를 끄면 "This object is fixed in place."가 뜬다.
- [ ] B1의 엘리베이터(`ElevatorCar2`)를 서쪽으로 4칸 밀어 구조물에 올리면 패드 색이 바뀐다.
- [ ] 동쪽 문 앞 (110,43)에서 클릭하면 타고 8 m **내려가** B2 (108,46)에 내린다.
- [ ] 샤프트 위에 걸린 차체가 중간에 떨어지지 않는다(밀리는 동안 Z 772 유지).
- [ ] 열차 문이 열린 동안 승강장 셀 (106,50/51/73/74)에서 클릭하면 탄다.
- [ ] 에스컬레이터를 클릭하면 발판에 실려 간다(동쪽 두 대는 9.6-3 문제 확인 필요).
- [ ] 카메라가 폰을 따라다니고 구도가 FlowTest_1과 같다(9.9).
- [ ] 문이 열린 동안 아트 문 앞(셀 X 106, Y 44~48/53~57/67~71/76~80)에서 클릭하면 탄다.
- [ ] 문이 닫혔을 때 열차를 클릭하면 "The doors are closed."가 뜨고 폰은 뒤쪽 바닥으로 걷는다.
- [ ] 열차가 부드럽게 출발했다가 도착 직전 급히 선다(9.11).
