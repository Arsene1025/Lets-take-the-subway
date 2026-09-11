# 역 전체 흐름 — 퍼즐 구간 · 엘리베이터 승강 · 열차 · 행인 승하차 · FlowTest_1 맵

작성일 2026-09-08. 출처: `지하철을타자 설명자료-260908.pdf`(기획, 5쪽) + 사용자 추가 요청 3건(채팅).
대상 레벨: 신규 테스트 맵 `Content/Maps/FlowTest_1`(5절). 검증 뒤 `GreyBoxTest_1`에 적용.
선행 문서: `RushHourPuzzle.md`(블록·엘리베이터·회전판), `RotatingObstacle.md`(회전 장애물·레버),
`GridNPC.md`(행인), `GreyBoxTest_1.md` 6절(층별 그리드 스케치).

---

## 0. 요청 해석 — 있는 것과 없는 것

기획 문서의 퍼즐 요소·연출 요소를 현재 코드와 대조한 결과다.

| 기획 항목 | 상태 | 근거 |
|---|---|---|
| 이동 가능한 장애물 (X/Y 직선 드래그, 회전 불가) | **있음** | `APuzzleBlock`, 드래그 한 번에 한 칸 |
| 이동 불가능한 장애물 (개찰구 포함) | **있음** | `AGridBoxMarker` Blocked, 그리드 클리어런스 |
| 엘리베이터 수평 이동 (문 축으로만 드래그) | **있음** | `APuzzleElevatorBlock` |
| 엘리베이터 회전판 (올리면 자동 90도) | **있음** | `APuzzleRotationTile` |
| 회전 장애물 + 레버 | **있음** | `APuzzleRotatingObstacle`, `APuzzleLever` |
| 장애물끼리 충돌, 막힌 쪽으로 이동 불가 | **있음** | 런타임 점유 레이어 |
| 행인 (스폰·통과·반투명·그리드 밖 진입) | **있음** | `AGridNPC`, `AGridNPCSpawner` |
| **엘리베이터 문이 양쪽** | 없음 (한쪽) | `DoorDirection` 하나 |
| **엘리베이터 구조물 — Z축 승강, 탑승 중 드래그 불가** | 없음 | `TryBoard`는 알림까지만 |
| **퍼즐 구간 밖으로 장애물이 나가지 않게** | 없음 | `CanSlide`는 바닥·점유만 본다 |
| **열차 (정차·문 개폐 5초·이동·자동 하차·주기 연출)** | 없음 | Train은 정적 메시 + Blocked 마커 |
| **열차에서 행인이 내림** | 없음 | 스포너는 타이머 전용 |
| 에스컬레이터 애니메이션 위로 행인 이동 | 바뀜 | **탈것으로 재설계**(`AGridEscalator`, 3.10). 플레이어만 타고 행인은 쓰지 않는다. 애니메이션 속도 연동은 실측값(`AnimStepSpeedAtRate1`) 대기 |
| 좁은 곳 진입 시도 시 충돌 피드백(진동/애니메이션) | 없음 | 문구 피드백만 |

사용자 추가 요청 3건은 1절에서 각각 설계 결정으로 다룬다. 범위 밖: 컷신, 스토리 진행, 스테이지
전환, **층이 XY로 겹치는 구간의 층별 그리드**(2차, `GreyBoxTest_1.md` 6절 — 이 계획은 그것 없이
성립하도록 짰다. 1.2절).

## 1. 핵심 설계 결정 (사용자 요청 3건)

### 1.1 장애물이 이동 가능한 영역 — "별도 그리드"가 아니라 **퍼즐 구간 액터**

세 가지를 비교했다.

| 안 | 내용 | 판단 |
|---|---|---|
| A. 장애물 전용 `AGridActor` 하나 더 | 블록은 두 번째 그리드 위에서만 움직인다 | **비권장.** 점유 레이어가 둘로 갈라져 폰↔블록 충돌을 양쪽에 이중 기록해야 하고, `FindGrid`가 첫 그리드만 돌려주는 전제가 전부 깨진다 |
| B. 셀 데이터에 구간 ID 비트 | `FGridCellData`에 `PuzzleZone` 저장, 마커로 굽는다 | 가능하지만 그리드가 퍼즐 개념을 알게 되고(의존 방향 역전), 직렬화 포맷이 바뀐다 |
| C. **퍼즐 구간 액터 `APuzzleRegion`** | 셀 사각 영역 하나. 서브시스템에 등록되고 `CanSlide`가 "목적지 사각형이 내 구간 안인가"를 검사 | **권장.** 그리드 무변경, 기존 관례(레지스트리 + 런타임 검사)와 같음, 구간 단위 기능(리셋·클리어 판정·디버그 표시)을 붙일 자리가 생긴다 |

C로 간다. 규칙은 단순하다: **블록(엘리베이터 포함)은 BeginPlay에서 자기 사각형을 완전히 담는
구간 하나에 소속되고, 그 구간 밖으로는 한 칸도 나가지 못한다.** 어느 구간에도 안 들어가면 경고
로그 후 제한 없음(기존 동작). 구간은 겹치지 않는다(회전판과 같은 규칙).

기획의 "장애물이 이동 가능한 그리드"를 **걸을 수 없는 셀 위로도 블록이 지나가게** 하려는 뜻으로
읽을 수도 있다(선로 위 등). 지금은 그렇게 읽지 않았다 — `CanSlide`의 "바닥이 있어야 한다" 검사를
유지한다. 필요해지면 구간에 `bAllowOverUnwalkable` 옵션을 다는 것으로 충분하다(7절 Q1).

### 1.2 엘리베이터가 얼마나 올라가는가 — **목적 층 셀을 참조**, 고정 높이는 예비값

기획: "올라가는 층과 같은 높이가 되거나(8m) 내려가는 층 바닥에 닿으면 정지".

| 안 | 내용 | 판단 |
|---|---|---|
| A. 고정 `TravelHeight` (8 m) | 구조물에 숫자 하나 | 층고가 다르면 매번 손으로 맞춰야 하고, 내려갈 때는 "바닥에 닿으면"과 어긋난다 |
| B. 명시 `TargetZ` | 구조물에 월드 Z | 바닥 메시를 옮기면 같이 틀어진다 |
| C. **목적 층의 출구 셀 `TargetFloorCell`** | 구조물이 "도착하면 내릴 셀"을 가리키고, 목표 Z = `Grid->GetCell(TargetFloorCell)->FloorZ` | **권장.** 높이가 실제 바닥에서 나오므로 올라가든 내려가든 "층 바닥에 정확히 닿는다". 출구 셀이 곧 하차 지점이라 별도 저작이 없다 |
| D. 런타임 스윕 | 차체를 위/아래로 스윕해 바닥에 닿으면 정지 | 벽·천장·다른 블록에 걸려 엉뚱한 데서 멈출 수 있어 소스 오브 트루스로는 부적합. **에디터 보조 버튼**(`Detect Target Floor`)으로만 쓴다: 구조물에서 출구 방향 한 칸 옆을 위·아래로 트레이스해 C의 셀을 채워 준다 |

C를 기본으로 하고, `TargetFloorCell`이 비어 있으면 A의 `TravelHeight`(기본 800)를 쓴다.
방향은 목표 Z가 현재보다 높으면 위, 낮으면 아래로 자동이다.

**단일 그리드로 충분한가.** 그리드는 셀마다 `FloorZ` 하나다. 엘리베이터가 오르내리는 통로(샤프트)
셀은 차체가 `GridTraceIgnore`라 **샤프트 바닥 Z로 구워지고**, 상층 출구 셀은 상층 바닥 Z로
구워진다. 둘은 단차가 커서 `NeighborMask`로 이어지지 않으므로 걸어서는 못 오르고, 차체가 유일한
통로가 된다 — 기획 그대로다. 조건은 하나, **상층 바닥이 하층 걸을 수 있는 바닥과 XY로 겹치지
않아야 한다.** 테스트 맵은 그렇게 짠다(5절). `GreyBoxTest_1`의 겹침(F0↔B1 약 20셀, B1_002↔B2_002
345셀)은 2차 층별 그리드 몫이며 이 계획의 클래스는 그때 그리드 참조만 바꾸면 된다.

**탑승 중 폰.** 차체 셀은 차체가 점유하므로 폰은 셀로 들어가지 않는다. 탑승 = 폰이 문 앞 셀에서
차체 중심으로 **직선 보행**한 뒤 차체에 붙어(`Riding`) 함께 움직인다. 도착 = 출구 방향 문 앞의
`TargetFloorCell`로 **직선 보행해 그리드로 복귀**. 이 "그리드 밖 → 가장 가까운 셀로 걸어 들어감"이
1.3절과 같은 코드다.

### 1.3 그리드가 없는 곳(움직이는 오브젝트)에서의 스폰·하차 — **이미 있는 진입 로직을 공용화**

`AGridNPC::FindEntryCell`(`NPC/GridNPC.cpp`)이 정확히 이 일을 한다: 스폰 위치의 셀에 못 들어가면
링을 넓혀 가며 **월드 거리 최근접 + 첫 경유 셀까지 경로가 있는** 셀을 고르고, 순간이동 없이
걸어 들어간다. 2026-09-07 PIE에서 NoFloor 셀 (69,62)에서 (70,62)로 진입하는 것을 확인했다.

바꿀 것은 셋이다.

1. **함수를 그리드로 옮긴다.** `AGridActor::FindEntryCell(FromWorld, MaxRadius, Pawn, OptionalGoal)`.
   행인, 플레이어 하차, 플레이어 스폰(`AGridPawn::BeginPlay`의 `FindNearestWalkableCell` 대체)이
   같은 답을 낸다.
2. **스포너에 이벤트 스폰을 단다.** 열차 문이 열릴 때 N명이 쏟아져 나와야 하므로 타이머만으로는
   안 된다. `SpawnMode = Interval | OnDemand`, `SpawnBurst(Count, Spacing)`.
3. **스포너는 열차가 아니라 승강장에 둔다.** 경유 셀은 승강장마다 다르므로 열차에 붙이면 정차역마다
   목록을 바꿔야 한다. 정차역(`FGridTrainStop`)이 스포너를 참조하고, 열차가 그 역에서 문을 열 때
   `SpawnBurst`를 부른다. 스포너 위치는 열차 문 바로 앞 선로 쪽(그리드 밖)이라 행인이 열차에서
   걸어 나오는 것처럼 보인다. 엘리베이터는 행인이 쓰지 않는다(기획: 행인은 에스컬레이터·개찰구).

움직이는 액터에 스포너를 붙이는 기능은 **만들지 않는다.** 스포너는 스폰 순간의 `GetActorLocation`을
읽으므로 붙이면 동작은 하지만, 위의 이유로 필요가 없다.

## 2. 기존 시스템과의 관계

- **그리드**: `FindEntryCell` 추가(NPC에서 이동) 외 변경 없음. 셀 데이터·직렬화 포맷 불변.
- **점유 레이어**: 그대로. 차체가 떠 있는 동안에도 샤프트 셀은 차체가 점유해 폰이 빠지지 않는다.
  열차는 정차 중 승강장 쪽 **탑승 셀**을 점유하지 않는다(열차 셀은 선로 위라 원래 걸을 수 없다).
- **서브시스템**: `Regions`, `Vehicles` 레지스트리 추가. `IsInputLocked`가 승강·승차·하차 중을 포함.
  `GetPawnReservedCells`는 폰이 탑승 중이면 비운다.
- **회전판**: 바닥 타일 공통 기반 `APuzzleFloorTile`을 뽑아내고 회전판과 엘리베이터 구조물이 상속한다.
  `NotifyBlockCameToRest`는 타일 목록을 돌며 `OnBlockCameToRest`를 부른다(가상). 회전판 동작 불변.
- **플레이어 폰**: 스텝 루프 불변. `Riding`/`Entering` 상태가 추가되고 그동안 클릭은 잠긴다.
- **행인**: `FindEntryCell` 호출부만 그리드 함수로 바뀐다. 동작 동일.
- **컨트롤러**: 픽 종류에 `Vehicle`(열차) 추가. 엘리베이터 클릭은 기존 `TryBoard` 경로 그대로.
- 의존 방향: Vehicle → Grid/Player/NPC, Puzzle → Grid/Player. **Grid는 여전히 아무것도 모른다.**

## 3. 클래스

카테고리·로그·`HideCategories`는 기존 관례(`LogLTTSGrid`, Puzzle 계열 카테고리명).

### 3.1 `Puzzle/PuzzleRegion.h/.cpp` — `APuzzleRegion : AActor` (퍼즐 구간)

- 저작: `SizeInCells`(FIntPoint, 기본 16x16), `bClampBlocks = true`, `RegionName`(FName, 로그·HUD용).
  액터 위치가 사각형 중심(`GridFootprint::MinCellFromCentre`, 박스 마커와 같은 스냅).
- 비주얼: 에디터 전용 얇은 테두리 4개(`bHiddenInGame`), `GridTraceIgnore`, `NoCollision`.
- API: `GetRect()`, `Contains(const FGridRect&)`, `Contains(FIntPoint)`.
- BeginPlay: 서브시스템 등록, 겹치는 구간이 있으면 경고.

`APuzzleBlock` 변경:
- `TWeakObjectPtr<APuzzleRegion> HomeRegion` — BeginPlay(`ClaimCells` 뒤)에 `Subsystem->FindRegionContaining(GetRect())`.
  없으면 `Warning: not inside any puzzle region; movement is unrestricted`.
- `CanSlide`: 축 검사 다음, 바닥 검사 앞에 `HomeRegion && bClampBlocks && !HomeRegion->Contains(Target)` →
  거부, 사유 `"This block cannot leave the puzzle area."`. 회전 장애물·엘리베이터는 상속으로 자동 적용.
- 회전판 회전 결과가 구간을 벗어나는 경우는 검사하지 않는다(회전판 자체가 구간 안에 있으면 결과도 안이다).

### 3.2 `Puzzle/PuzzleFloorTile.h/.cpp` — `APuzzleFloorTile : AActor` (바닥 타일 기반)

`APuzzleRotationTile`에서 뽑아낸다: `SizeInCells`, `Region`, `PadMesh`, `Grid`, `FullyContains`,
`Straddles`, 스냅·`PostEditMove`, 등록/해제, `virtual void OnBlockCameToRest(APuzzleBlock&)`,
`virtual bool IsBusy() const`(입력 잠금용). 회전판은 `OnBlockCameToRest`에서 기존 `TryRotate`를 부른다.
서브시스템의 `Tiles`는 `TArray<TWeakObjectPtr<APuzzleFloorTile>>`로 바뀐다.

### 3.3 `Puzzle/PuzzleElevatorDock.h/.cpp` — `APuzzleElevatorDock : APuzzleFloorTile` (엘리베이터 구조물)

> **2026-09-11: 저작 방식이 바뀌었다.** 목표 층은 이제 셀 번호가 아니라 **층마다 하나씩 놓고 서로를
> 가리키는 Dock**(`TargetDock`)에서 나온다. 아래 `TargetFloorCell`·`TravelHeight` 설명은 `TargetDock`이
> 비어 있을 때의 예비 경로로만 유효하다. 새 설계는 `BoardingRefactor.md` 2.3절을 본다.

- 저작: `TargetFloorCell`(FIntPoint, 1.2절 C), `TravelHeight = 800`(셀 미지정 시), `ExitDirection`
  (EGridDirection, 도착 층에서 내릴 문), `TravelSpeed = 200 cm/s`, `DoorDwellSeconds = 0.5`
  (도착 후 하차까지), `bReturnAfterUnboard = false`.
- `CallInEditor` `Detect Target Floor`: 구조물 중심에서 `ExitDirection`으로 (Size/2 + 1)칸 옆 지점을
  위(+2000)와 아래(−2000)로 트레이스해 가장 가까운 바닥의 셀을 `TargetFloorCell`에 넣는다.
- 상태: `DockedElevator`(약참조). `OnBlockCameToRest`에서 `FullyContains` && 엘리베이터면 도킹.
  도킹 중 다른 곳으로 밀려 나가면(슬라이드 시작) 해제. 비주얼: 도킹 시 패드 색 변경.
- `TryLaunch(AGridPawn*)`: 도킹된 차체가 있고, 폰이 그 문 앞이며, 아무것도 애니메이션 중이 아니면
  `Elevator->StartVerticalTravel(TargetZ, this)`. 컨트롤러의 엘리베이터 클릭 경로에서 `TryBoard`
  성공 시 서브시스템이 `FindDockUnder(Elevator)`로 찾아 부른다. 도킹되지 않은 엘리베이터의
  `TryBoard`는 지금처럼 알림만(기획: 구조물 위에서만 승강).
- 목표 Z 계산은 발사 시점에 한다(`TargetFloorCell` → `FloorZ`, 없으면 현재 바닥 ± `TravelHeight`,
  부호는 `ExitDirection` 쪽 셀 Z와 비교해 정한다).

### 3.4 `APuzzleElevatorBlock` 변경 — 양면 문, 수직 이동, 탑승자

- **양면 문**: `DoorDirection`은 유지하되 의미를 "문 축"으로 바꾼다. `GetDoorFrontCells`가 `Door`와
  그 반대편 두 줄을 돌려주고, `DoorMesh`를 두 장 붙인다. 이동 축은 지금처럼 문 축에서 유도(변화 없음).
  기획: "문이 양방향으로 있어 회전 방향은 중요하지 않다".
- `EAnimState::Lifting` 추가. `StartVerticalTravel(double TargetZ, APuzzleElevatorDock* Dock)`:
  `TryBoard` 직후 폰을 `BoardVehicle(this, 중심 오프셋)`으로 태우고(폰이 문 앞 셀 → 차체 중심으로
  직선 보행, 그동안 `Entering`), 보행이 끝나면 Z 보간 시작. 도착 → `DoorDwellSeconds` 뒤
  `Pawn->WalkOntoGrid(출구 문 앞 월드 위치)` → 폰이 복귀하면 `OnArrived` 델리게이트.
- `FloorZ`(BeginPlay 기록값)는 **차체가 지금 서 있는 높이**로 갱신한다. 위층에서 다시 밀 수 있어야
  하므로 `CanSlide`의 바닥 검사는 "목적지 셀의 FloorZ가 현재 차체 높이와 같은가"를 추가로 본다.
  위층에서 옆으로 밀면 상층 바닥(같은 Z) 위로만 나간다. 샤프트 셀은 Z가 달라 되돌아오지 못한다 —
  기획상 문제 없고, 7절 Q3에 남긴다.
- 탑승 중 드래그 불가: `IsHeld` 검사와 별개로 `Rider.IsValid()`면 `CanSlide` 거부(`"Someone is riding."`).
  `IsAnimating()`이 `Lifting`을 포함하므로 입력 잠금은 자동이다.
- 시각: 그레이박스 큐브 유지. 아트의 `E_body`/`ac_door`/`ac_pillar`(ART_1)는 `BodyMesh`/`DoorMesh`
  교체로 나중에 붙인다(4x4 m 치수 확인 필요, 7절 Q6).

### 3.5 `AGridPawn` 변경 — 탑승·진입 상태

- `enum class ERideState { OnGrid, Entering, Riding, Leaving }` + `TWeakObjectPtr<AActor> Vehicle`,
  `FVector RideOffset`.
- `BoardVehicle(AActor* Vehicle, const FVector& SeatWorld)`: 경로 버림, `Entering`으로 `SeatWorld`까지
  정속 직선 보행(스텝 루프의 `VInterpConstantTo` 재사용, 셀 검사 없음) → `Riding`. `Riding` 동안
  Tick은 `Vehicle->GetActorLocation() + RideOffset`을 따라간다(SpringArm이 폰에 있어 카메라가 같이 간다).
- `WalkOntoGrid(const FVector& FromWorld, int32 Radius = 8)`: `Grid->FindEntryCell(FromWorld, …)`로 셀을
  고르고 `Leaving`으로 직선 보행 → 도착 시 `CurrentCell = 셀`, `OnGrid`, `NotifyPawnEnteredCell`.
  실패 시 Error 로그 + 마지막 셀로 `TeleportToCell`(폰이 허공에 남지 않게).
- `IsRiding()`, `IsOnGrid()`. `GetPawnReservedCells`·컨트롤러 호버·클릭은 `OnGrid`일 때만.
- `BeginPlay`의 스폰 셀 탐색을 `FindEntryCell`로 바꾸되 **위치는 스냅**(지금과 같이). 걸어 들어가는
  것은 하차 때만.
- **충돌 피드백**(기획 "진동 혹은 애니메이션"): `Bump(EGridDirection)` — 막힌 이웃 셀 쪽으로 15 cm
  나갔다 돌아오는 0.2초 흔들림. 컨트롤러가 "클릭한 셀이 현재 셀의 이웃이고 거부 사유가
  Clearance/Marker/Object"일 때 호출. 공이라 애니메이션 없이 위치 흔들림으로 충분하다.

### 3.6 `Vehicle/GridTrain.h/.cpp` — `AGridTrain : AActor` (열차)

새 폴더 `Source/LetsTakeTheSubway/Vehicle/`. 엘리베이터와 달리 퍼즐 조각이 아니므로 Puzzle에 두지 않는다.

- 컴포넌트: `SceneRoot`, `USplineComponent Track`(선로, 레벨에서 편집), `UStaticMeshComponent Body`
  (그레이박스 박스, 나중에 `SM_Train1`), `DoorMeshes`(문 위치 표시 슬랩, 열림/닫힘 색).
  콜리전: Visibility 블록(클릭 대상), 그 외 무시. `GridTraceIgnore`.
- 저작 `Stops`(`TArray<FGridTrainStop>`): `SplineDistance`, `StopName`, `BoardingCells`(승강장 쪽 문 앞
  셀들 — 폰이 여기 서서 클릭하면 탑승), `ExitWorldOffset`(하차 시 `WalkOntoGrid`에 줄 위치, 문 앞
  선로 쪽), `TObjectPtr<AGridNPCSpawner> DisembarkSpawner`, `DisembarkCount = 3`.
- 저작: `Speed = 1200 cm/s`, `DoorOpenSeconds = 5`(기획값, 조정 가능), `LoopInterval = 20`(연출용: 문
  닫고 떠난 뒤 다음 정차까지), `bLoop = true`(마지막 정차역 뒤 첫 역으로), `bDepartAfterBoarding = true`
  (플레이어가 타면 남은 대기 없이 문을 닫는다).
- 상태기계: `Approaching → DoorsOpening(연출) → DoorsOpen(타이머) → DoorsClosing → Moving → Approaching …`.
  `DoorsOpening`과 `DoorsClosing`은 문 애니메이션 시간이고 그 동안에는 아무도 타고 내리지 못한다.
  `DoorsOpen` 진입 시 `OnDoorsOpened(StopIndex)` 델리게이트 + `DisembarkSpawner->SpawnBurst(DisembarkCount, 0.6)`.
  플레이어가 타고 있으면 스포너 버스트도 그대로(행인이 같이 내린다).
- `TryBoard(AGridPawn*, FText*)`: 문이 열려 있고(`DoorsOpen`), 폰이 정지해 있고, 현재 정차역의
  `BoardingCells`에 서 있으면 `Pawn->BoardVehicle(this, 문 안쪽 좌석)`. 아니면 사유
  (`"The doors are closed."`, `"Stand at the door first."`).
- 이동 중 `Riding` 폰은 자동으로 따라온다. 다음 정차역 `DoorsOpen` 진입 시 탑승자가 있으면
  `Pawn->WalkOntoGrid(ExitWorldOffset)` → 복귀하면 조작 가능(기획: "자동으로 하차하고 일정 위치까지
  이동한 뒤 정지"). 하차 위치까지의 추가 이동은 `WalkOntoGrid`가 고른 셀에서 끝난다 — 더 멀리
  보내고 싶으면 `FGridTrainStop::PostExitCell`(선택)로 한 번 더 `RequestMoveToCell`.
- 서브시스템 `RegisterVehicle`. `IsInputLocked`는 폰이 `OnGrid`가 아니면 true.
- 콘솔: `ltts.TrainArrive [이름]`(즉시 다음 역 도착), `ltts.TrainDoors [이름] 0|1`.

### 3.7 `AGridNPCSpawner` 변경 — 이벤트 스폰

- `ESpawnMode SpawnMode = Interval`(기존) `| OnDemand`(타이머 없음).
- `SpawnBurst(int32 Count, float Spacing)`: `Spacing` 간격으로 `Count`명, `MaxAlive`는 무시(열차 하차는
  상한과 무관해야 한다). 내부 타이머 하나로 순차 스폰.
- 기존 `ltts.SpawnNPC`는 그대로. `ltts.SpawnNPCBurst [이름] [수]` 추가.

### 3.8 `AGridActor` 변경

- `bool FindEntryCell(const FVector& FromWorld, int32 MaxRadius, const APawn* Pawn, const TOptional<FIntPoint>& Goal, FIntPoint& OutCell) const`.
  본문은 `AGridNPC::FindEntryCell` 그대로(월드 최근접 + 경로 검사). NPC는 이 함수를 부른다.

### 3.9 컨트롤러 변경

- `FCursorPick::EKind::Vehicle` + `TWeakObjectPtr<AGridTrain> Vehicle`. 열차는 어느 면을 눌러도 잡힌다
  (열차 뒤 셀은 선로라 클릭할 일이 없다). 누름/뗌: 움직이지 않았으면 `Train->TryBoard`.
- 엘리베이터 뗌: `TryBoard` 성공 → `Subsystem->FindDockUnder(Elevator)` → 있으면 `Dock->TryLaunch(Pawn)`,
  없으면 기존 피드백 `"Push it onto an elevator dock first."`.
- 폰이 `OnGrid`가 아니면 누름 무시(호버 오버레이도 끈다).

### 3.10 에스컬레이터 (일방통행 탑승) — 2026-09-08 재설계

아트의 `BP_escalator_up_s`/`down_s`(스켈레탈 `AC_001` + `AC_SCALE_ANIM_Anim`)의 **부모 클래스를
`AGridEscalator`로 바꿔서** 쓴다(`Source/LetsTakeTheSubway/Vehicle/GridEscalator.h`).
에스컬레이터는 걷는 경사로가 아니라 **탈것**이다. 엘리베이터와 같은 조작이다.

첫 구현(같은 날 오전)은 발판이 덮는 셀을 계산해 점유하고 이웃 마스크를 런타임에 끊는 방식이었다.
의도대로 동작하지 않아 **버리고 다시 짰다**. 무엇이 잘못이었는지는 3.10.1에 적는다.

- **태워 나르는 길을 셀이 아니라 경로로 적는다.** `TArray<FVector> RidePathLocal`이 액터 로컬
  좌표로 발판 표면을 따라가고, `[0]`이 타는 쪽, 마지막이 내리는 쪽이다. **올라가는 것과 내려가는
  것의 차이는 이 순서뿐이며**, 방향을 뜻하는 프로퍼티는 없다. 액터를 회전시키면 경로도 함께 돈다.
- `BeginPlay`가 이 배열로 `USplineComponent`를 만들고(전 구간 Linear), 좌석 노릇을 할
  `USceneComponent`와 클릭을 받을 `UBoxComponent`도 그때 만든다. 생성자는 컴포넌트를 하나도
  만들지 않는다 — 네이티브 루트가 있으면 리페어런트할 때 블루프린트의 `DefaultSceneRoot`와
  그 아래 메시가 전부 사라지기 때문이다(2026-09-08에 겪었다).
- 탑승: 타는 셀에 서서 클릭 → `TryBoard` → `Pawn->BoardVehicle(this, 좌석, Seat)`. 세 번째
  인자가 이번에 `AGridPawn`에 추가한 **앵커 컴포넌트**다. 폰은 여태 탈것 액터의 원점을 따라
  왔는데, 에스컬레이터는 액터가 가만히 있고 그 위의 한 점만 움직이므로 따라갈 대상을 컴포넌트로
  바꿔 줘야 한다. 인자를 비우면 지금까지대로라서 엘리베이터·열차는 한 줄도 바뀌지 않는다.
- 하차: 경로 끝에 닿으면 `WalkOntoGrid(끝점 + 진행방향 * LandingDistance)`. 엘리베이터와 같다.
- **그리드에 굽지 않는다.** 셀을 점유하지도, 이웃 연결을 끊지도 않는다. 층을 가르는 것은
  에스컬레이터가 아니라 **바닥이 없다는 사실**이다: 경사 구간의 그레이박스 계단에
  `GridTraceIgnore`를 붙이면 그 셀들이 `NoFloor`가 되어 걸어서 층을 오갈 방법이 사라지고,
  남는 수단이 탑승뿐이 된다. 규칙을 그리드 데이터에 새기지 않고도 일방통행이 성립한다.
- **행인은 에스컬레이터를 쓰지 않는다.** `AGridNPC`에는 `ERideState`가 없어 걷는 것 말고는 못
  하고, 경사 구간에 바닥이 없으므로 그 길로는 길찾기가 되지 않는다. 행인 동선은 계단·엘리베이터
  쪽으로만 흐른다.
- 클릭은 **엘리베이터 규칙**이다. 어느 면을 눌러도 잡히고, 지금 탈 수 없으면 왜 안 되는지
  알려 준다("Stand at the near end to ride. This escalator runs one way."). 열차처럼 "탈 수
  있을 때만 잡기"로 두면 거부 이유가 영영 화면에 닿지 못한다 — 첫 구현의 실제 증상이었다.
- 애니메이션은 노브 하나로 맞춘다. `AnimStepSpeedAtRate1`(PlayRate 1일 때 발판이 초당 나아가는
  cm, 실측)이 0이 아니면 `BeginPlay`가 모든 스켈레탈 메시의 PlayRate를
  `RideSpeed / AnimStepSpeedAtRate1`로 맞춘다. 부호는 그대로 둔다 — 내려가는 에스컬레이터는
  같은 애니메이션을 거꾸로 돌려 만든 것이다. 0이면 아직 재 보지 않았다는 뜻이고 손대지 않는다.
- 배치 확인은 디테일 패널의 **Log Boarding Cells** 버튼과 `bDrawDebugPath`로 한다.

#### 3.10.1 첫 구현을 버린 이유

| 첫 구현 | 문제 |
|---|---|
| 풋프린트 셀을 `SetOccupant`로 점유 | 발판이 셀 격자에 딱 떨어지지 않아 덮는 셀을 인스턴스마다 손으로 맞춰야 했다 |
| `AGridActor::BlockCellExit`으로 이웃 비트 제거 | 구운 데이터를 런타임에 고치고 되돌리지 못한다. 그리드가 탈것을 알게 된다 |
| 좌석 높이를 `CellToWorld`의 `FloorZ`에서 | 경사 구간에 바닥이 구워져 있어야만 동작한다. 층을 가르려면 정확히 그 바닥을 없애야 하는데 서로 모순이다 |
| 두 끝 좌석 사이 직선 보간 | 중간 셀 높이를 무시해 공이 발판 위로 뜨거나 파묻힌다 |
| 탈 수 있을 때만 클릭을 잡음 | 반대쪽에서 누르면 아무 말 없이 폰이 뒤쪽 바닥으로 걸어간다 |
| 탑승마다 `AGridEscalatorCarrier` 스폰 | 앵커 컴포넌트로 대체해 임시 액터가 사라졌다 |

**Q6 실측(2026-09-08):** 아트 에스컬레이터의 걸을 수 있는 면은 상승 약 6.1 m, 진행 약 6.5 m,
폭 3.8 m다. 계획이 가정한 8x8 m가 아니다. FlowTest_1의 층 간격은 8 m이므로 발판만으로는 두 층을
잇지 못한다. 경로 끝점을 위층 바닥에 맞추면 동작은 하지만 발판 끝과 바닥 사이에 단차가 남는다.
정리하려면 BP를 X 1.23배 Z 1.31배로 늘려 8 m에 맞추거나 층 간격을 6 m로 줄여야 한다.

## 4. 흐름 규칙

### 4.1 엘리베이터 (퍼즐 → 승강 → 하차)

```
블록 치우기 ─ 엘리베이터를 회전판에 올림 → 90도(문 축 전환)
 ─ 구조물 위로 밀어 넣음 → OnBlockCameToRest → 도킹(패드 점등)
 ─ 폰이 문 앞 셀에서 엘리베이터 클릭 → TryBoard ✓ → TryLaunch
 ─ 폰 Entering(문 앞 → 차체 중심 직선 보행) → Riding
 ─ 차체 Lifting: Z 보간(TravelSpeed) → TargetZ 도착
 ─ DoorDwellSeconds → 폰 Leaving(출구 문 앞 → FindEntryCell 셀) → OnGrid, 입력 해제
```

거부: 도킹 안 됨 / 폰 이동 중 / 문 앞 아님 / 무언가 애니메이션 중 / 탑승자 있는 차체 드래그.
도킹 중 폰이 차체 셀로 걸어갈 수 없는 것은 점유 레이어가 보장한다.

### 4.2 열차 (연출 + 승하차)

```
Approaching(스플라인 이동) → 정차 → DoorsOpening(DoorOpeningSeconds, 승하차 막힘)
 → DoorsOpen: OnDoorsOpened → 행인 버스트, 탑승자 하차(WalkOntoGrid)
 → DoorOpenSeconds 대기(플레이어 탑승 시 bDepartAfterBoarding면 즉시)
 → DoorsClosing(DoorCloseSeconds, 승하차 막힘)
 → Moving(다음 정차역까지) → 반복. bLoop면 마지막 역 뒤 첫 역.
```

퍼즐을 푸는 동안에도 계속 돈다(기획 "연출"). 플레이어가 없는 승강장에서도 문이 열리고 행인이 내린다.
행인은 `GridPassThrough`라 열차 셀·선로 셀과 무관하게 스포너 위치에서 승강장으로 걸어 들어온다.

### 4.3 퍼즐 구간

블록의 모든 슬라이드는 `HomeRegion` 안이어야 한다. 구간 경계는 에디터에서만 보인다. 회전판·구조물은
구간 안에 둔다(밖에 두면 엘리베이터가 닿지 못한다 — BeginPlay에서 경고).

### 4.4 에스컬레이터 (일방통행 탑승)

```
폰이 타는 셀(BoardingCells)에 서서 에스컬레이터 클릭
 ─ PickUnderCursor: EKind::Escalator (언제나 잡힌다)
 ─ OnPressed → TryBoard → CanBoard ✓ → 좌석을 경로 시작점에 놓음
 ─ Pawn->BoardVehicle(this, 좌석, Seat) → 폰 Entering(직선 보행) → Riding
 ─ 에스컬레이터 Riding: 좌석을 스플라인 거리를 따라 RideSpeed로 옮김, 폰은 TickRide로 추종
 ─ 경로 끝 → WalkOntoGrid(끝점 + 진행방향 * LandingDistance) → Leaving → OnGrid
 ─ Unloading에서 IsOnGrid 확인 → 태우기 종료, 틱 끔
반대쪽 끝·옆에서 클릭 → CanBoard ✗ → "Stand at the near end to ride. This escalator runs one way."
```

입력 잠금은 따로 없다. 폰이 `OnGrid`가 아니면 `RequestMoveToCell`도 `CanBoard`도 거부한다.

## 5. FlowTest_1 맵 — 전체 흐름 검증용

목적: **퍼즐 → 엘리베이터 승강 → 개찰구 → 에스컬레이터 → 열차 → 하차**를 한 바퀴 도는 최소 역.
`GreyBoxTest_1`의 겹침·크기 문제 없이 기능만 본다. 층은 XY로 절대 겹치지 않게 배치한다(1.2절).
아트가 넘긴 모듈(ART_1, 2026-09-08 병합)을 쓴다: `SM_Floor*`, `SM_Elevator1`, `SM_Escalator*`,
`SM_TicketGate_B1_009~016`, `SM_Train1`, `SM_Trail_B3`, `BP_escalator_up_s/down_s`, `MI_GreyBox_*`.

| 항목 | 값 |
|---|---|
| 레벨 | `Content/Maps/FlowTest_1`, Empty Open World + OFPA, 스트리밍 끔, GameMode `GridTestGameMode` |
| 그리드 | 원점 `(0, 0, −700)`, `SizeInCells (90, 30)`, `RegionHeight 1200`(−700 ~ +500), `MaxStepHeight 110`, `MaxSlopeAngle 50`, `bIsSpatiallyLoaded = false` |
| 층 높이 | 승강장 Z **−600**, 선로 Z **−1000**, 상층 랜딩 Z **+200**(8 m 위) |

셀 좌표(X, Y)와 월드 = `원점 + 셀 × 100`. 치수는 배치하며 실측으로 확정한다.

| 구역 | 셀 범위 | Z | 비고 |
|---|---|---|---|
| 선로 | x 0~89, y 0~5 | −1000 | `SM_Trail_B3` 반복. 열차 스플라인은 y 3 선상 |
| 승강장 A | x 0~25, y 6~29 | −600 | 시작점. PlayerStart 셀 (3, 20) |
| 퍼즐 구간 A | x 4~25, y 10~25 (22x16) | −600 | `APuzzleRegion`. 블록 2~3개, 기둥 마커 1, 회전판 (12,14) 4x4. **구조물까지 품어야 한다**(9.3절) |
| 엘리베이터 시작 | (8,14) 4x4, 문 축 Y | −600 | 회전판을 거쳐야 X축으로 바뀌어 구조물까지 밀 수 있다 |
| 엘리베이터 구조물 | x 22~25, y 12~15 (4x4) | −600 | `APuzzleElevatorDock`, `ExitDirection East`, `TargetFloorCell (26, 13)`. 샤프트 벽: y 11과 y 16 열, x 22~25 (Blocked 마커 또는 벽 메시) |
| 상층 랜딩 | x 26~41, y 6~29 | +200 | `SM_Floor`. **x 25 이하와 겹치지 않는다** |
| 개찰구 | x 33~34 열, y 7·9·11·…(1 m 틈) | +200 | `SM_TicketGate_B1_009~016`. 틈은 걸을 수 있음(2차에 Conditional `HasTicket`) |
| 에스컬레이터 ↓ | x 42~49, y 8~11 | +200 → −600 | `BP_escalator_down_s`(플레이어용). 45°, 8 m |
| 에스컬레이터 ↑ | x 42~49, y 18~21 | −600 → +200 | `BP_escalator_up_s`(행인용) |
| 승강장 B | x 50~79, y 6~29 | −600 | 열차 탑승 |
| 열차 정차역 B | 열차 x 57~72, 문 (60,6)·(69,6) | — | `BoardingCells` = 문 앞 승강장 셀 (60,6)(61,6)(69,6)(70,6) |
| 열차 정차역 A | 열차 x 5~20, 문 (8,6)·(17,6) | — | 하차역. `PostExitCell (12, 9)`, 그 셀에 StageClear 마커(한 바퀴 완료) |
| 행인 스포너 1 | (44, 5) 그리드 밖(선로 가장자리) | −600 | Interval. 경유 (46,19)→(42,19)→(36,19)→(30,19), 개찰구 지나 소멸 |
| 행인 스포너 2 | (69, 4) 그리드 밖 | −700 | OnDemand, 정차역 B의 `DisembarkSpawner`. 경유 (69,7)→(60,20)→(50,20)→(42,20) 에스컬레이터 ↑ → (30,20) |

**한 바퀴**: 승강장 A에서 블록을 치워 엘리베이터를 회전판에 올리고(문 축 Y→X), 동쪽으로 밀어 구조물에
도킹 → 서쪽 문 앞 (21, 12~15)에서 클릭 → 8 m 상승 → 동쪽 문으로 랜딩 (26,13) 하차 → 개찰구 틈 통과 →
에스컬레이터 ↓로 승강장 B → 열차 문 앞 (60,6)에서 문 열릴 때 클릭 → 열차가 서쪽 정차역 A로 →
자동 하차 (12,9) → StageClear. 그동안 열차는 주기적으로 오가고 행인은 두 스포너에서 나온다.

## 6. 검증 체크리스트 (PIE, `ltts.GridDebug 2`)

자동(로그)
- [ ] 게임·에디터 타깃 빌드 성공.
- [ ] 구간 BeginPlay: 블록 N개가 `RegionA`에 소속, 소속 없는 블록 경고 0건.
- [ ] 구조물 `Detect Target Floor` → `TargetFloorCell (26,13)`, Z +200 로그.
- [ ] 엘리베이터 승강 로그: `launching to Z 200`, `arrived`, 폰 `left the grid`/`entered the grid at (26,13)`.
- [ ] 열차: 정차역 B `DoorsOpen` → 스포너 2 버스트 3명, 5초 뒤 `DoorsClosing`, `Moving`, 정차역 A 도착.
- [ ] 행인이 `NPCs alive` 상한과 버스트를 각각 지킨다(버스트는 상한 무시).
- [ ] PIE 두 번 연속 실행에 ensure 없음(타이머·약참조).
- [ ] `GreyBoxTest_1`이 회귀 없이 열리고 기존 러쉬아워·회전 장애물·행인이 동작한다(구간이 없으면 제한 없음).

손으로
- [ ] 블록을 구간 경계 밖으로 밀면 거부 문구, 안에서는 자유.
- [ ] 엘리베이터가 회전판에서 돌아 문 축이 바뀌고 구조물에 도킹하면 패드가 점등된다.
- [ ] 도킹 전 클릭은 "구조물 위로 먼저" 문구. 도킹 후 문 앞 클릭이면 폰이 걸어 들어가 함께 올라간다.
- [ ] 상승 중 클릭·드래그 무시, 카메라가 폰을 따라간다.
- [ ] 도착 후 폰이 동쪽 문으로 랜딩에 걸어 내리고 조작이 돌아온다.
- [ ] 랜딩에서 엘리베이터를 동쪽으로 밀면 밀리고, 서쪽(샤프트)으로는 안 밀린다.
- [ ] 개찰구 틈만 통과, 개찰구 정면 클릭 시 공이 부딪혀 흔들린다(`Bump`).
- [ ] 에스컬레이터 ↓로 내려간다. 행인이 ↑로 올라오며 애니메이션 방향과 이동 방향이 같다.
- [ ] 문 닫힌 열차 클릭 → "문이 닫혀 있다". 문 앞이 아닌 곳에서 클릭 → "문 앞에 서라".
- [ ] 문 열린 열차에 타면 문이 닫히고 출발, 정차역 A에서 자동 하차 후 (12,9)로 걸어가 StageClear.
- [ ] 열차에서 내린 행인이 플레이어와 겹칠 때 반투명.
- [ ] 퍼즐을 푸는 동안 열차가 계속 오가고 행인이 내린다.

## 7. 결정 필요 / 남은 가정

| # | 항목 | 내용 |
|---|---|---|
| Q1 | 구간의 의미 | 블록이 걸을 수 없는 셀(선로 등) 위로도 지나가야 하는가. 지금은 아니오. 필요하면 `bAllowOverUnwalkable` |
| Q2 | 높이 결정 | 1.2절 C(목적 셀) 권장. 기획이 "항상 8 m"를 원하면 A로 바꿔도 클래스는 같다 |
| Q3 | 위층에서 엘리베이터 재사용 | **구현에서 왕복으로 해결**(9.2절). 다만 위층에서 차체를 옆으로 밀어내 버리면 구조물을 벗어나 되돌아올 수 없다. 위층 퍼즐이 생기면 그때 다룬다 |
| Q4 | 열차 탑승 조작 | "문 앞 셀 + 열차 클릭"으로 했다. 문 앞 셀에 서기만 하면 자동 탑승으로 바꾸는 것도 한 줄이다 |
| Q5 | 하차 후 추가 이동 | `PostExitCell`로 한 번 더 걷게 했다. 기획의 "일정 위치"가 컷신 지점이면 그때 델리게이트로 넘긴다 |
| Q6 | 아트 에셋 치수·콜리전 | `E_body`(엘리베이터), `SM_Train1`, 에스컬레이터 BP의 크기와 콜리전 프리셋을 에디터에서 확인해야 그레이박스 큐브를 교체할 수 있다. 계획은 큐브로 먼저 간다 |
| Q7 | 기본 맵 | `EditorStartupMap`을 `FlowTest_1`로 바꿀지. 지금은 `GreyBoxTest_1` 유지 |
| Q8 | 층별 그리드(2차) | `GreyBoxTest_1`에 적용할 때 필요. 이 계획의 클래스는 `Grid` 참조 하나만 바꾸면 되도록 짰다 |

## 8. 작업 순서

에디터는 클래스를 추가하는 단계마다 **닫고** 정식 빌드한다(Live Coding 크래시 전례, `GreyBoxTest_1.md` 7.4).
빌드: `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" <타깃> Win64 Development -project="C:\Users\User\Desktop\Lets-take-the-subway\LetsTakeTheSubway.uproject" -waitmutex`.
커밋·푸시는 사용자 지시가 있을 때만, `PROG_1`에.

1. **퍼즐 구간** — `APuzzleRegion`, 서브시스템 레지스트리, `APuzzleBlock::HomeRegion`/`CanSlide`. → 게임 타깃 빌드. `GreyBoxTest_1` 회귀(구간 없음 = 제한 없음).
2. **진입 공용화 + 이벤트 스폰** — `AGridActor::FindEntryCell`(NPC에서 이동), `AGridPawn` 상태기계·`BoardVehicle`·`WalkOntoGrid`·`Bump`, 스포너 `OnDemand`/`SpawnBurst`, 서브시스템 입력 잠금. → 빌드. 행인 회귀.
3. **바닥 타일 기반 + 엘리베이터 구조물** — `APuzzleFloorTile` 추출(회전판 회귀), `APuzzleElevatorDock`, `APuzzleElevatorBlock` 양면 문·`Lifting`·탑승자, 컨트롤러 도킹 분기. → 빌드.
4. **열차** — `AGridTrain`, 정차역, 컨트롤러 `Vehicle` 픽, 콘솔 명령. → 빌드.
5. **에디터 타깃 빌드** → 에디터 실행 → **FlowTest_1 배치**(5절, MCP 또는 수작업), 그리드 생성, `Detect Target Floor`, 스포너·정차역 설정, Save.
6. **6절 체크리스트** — 자동 항목은 PIE 로그로, 손 항목은 사용자가.
7. 이 문서에 "구현 결과" 절 추가(계획과 달라진 점, 실측 좌표).

예상 규모: 신규 클래스 4(`PuzzleRegion`, `PuzzleFloorTile`, `PuzzleElevatorDock`, `GridTrain`), 수정 6
(`PuzzleBlock`, `PuzzleElevatorBlock`, `PuzzleRotationTile`, `PuzzleSubsystem`, `GridPawn`, `GridPlayerController`,
`GridNPCSpawner`, `GridNPC`, `GridActor`), 맵 1. 1~2단계는 서로 독립이라 순서를 바꿔도 된다.

---

## 9. 구현 결과 (2026-09-08)

8절의 **1~6단계를 모두 수행했다.** 코드 4단계, FlowTest_1 배치, PIE 자동 검증까지 끝났고,
남은 것은 마우스가 필요한 손 검증뿐이다(9.6절).

### 9.1 만들거나 고친 파일

| 파일 | 내용 |
|---|---|
| `Puzzle/PuzzleRegion.h/.cpp` | **신규.** 퍼즐 구간. 셀 사각형 + 에디터 전용 테두리, 겹침 경고 |
| `Puzzle/PuzzleFloorTile.h/.cpp` | **신규.** 회전판에서 뽑아낸 바닥 타일 기반(Abstract). 영역 계산·스냅·포함 판정·`OnBlockCameToRest`·`IsBusy` |
| `Puzzle/PuzzleElevatorDock.h/.cpp` | **신규.** 승강 구조물. 도킹 판정, 목표 Z 계산, `Detect Target Floor`, 패드 색 전환 |
| `Vehicle/GridTrain.h/.cpp` | **신규.** 열차. 정차역·문 개폐·자동 하차·행인 버스트·순환, 콘솔 명령 둘 |
| `Puzzle/PuzzleRotationTile.h/.cpp` | 기반 클래스 위로 옮김. 회전 로직은 그대로 |
| `Puzzle/PuzzleBlock.h/.cpp` | `HomeRegion`, `CanStartMoving`·`CanOccupyRect` 가상 훅, `EAnimState::Lifting`, `SetFloorZ`/`SnapToRect` protected화 |
| `Puzzle/PuzzleElevatorBlock.h/.cpp` | 양면 문, `StartVerticalTravel` 4단계 상태기계, 탑승 중 드래그 거부, 층 높이 검사 |
| `Puzzle/PuzzleSubsystem.h/.cpp` | `Regions`·`Vehicles` 등록부, `FindRegionContaining`, `FindDockUnder`, 타일 목록을 기반 클래스로, 탑승 중 입력 잠금 |
| `Player/GridPawn.h/.cpp` | `ERideState` 상태기계, `BoardVehicle`·`WalkOntoGrid`·`Bump`, 스폰 셀 탐색을 `FindEntryCell`로 |
| `Player/GridPlayerController.h/.cpp` | `FCursorPick::EKind::Vehicle`, 엘리베이터 클릭의 구조물 분기, 인접 셀 거부 시 `Bump` |
| `Player/GridHUD.cpp` | 탑승 상태 줄 |
| `Grid/GridActor.h/.cpp` | `FindEntryCell` 추가(행인에서 이동) |
| `NPC/GridNPC.cpp` | `FindEntryCell`이 그리드 함수를 부르도록 위임 |
| `NPC/GridNPCSpawner.h/.cpp` | `ENPCSpawnMode`, `SpawnBurst`, `ltts.SpawnNPCBurst` |

### 9.2 계획과 달라진 점

| 항목 | 계획 | 실제 | 이유 |
|---|---|---|---|
| 열차 선로 | `USplineComponent` | **정차역 셀들의 직선 구간** | 스플라인은 액터에 붙으면 액터와 함께 움직여서 경로 구실을 못 한다. 절대 트랜스폼으로 두면 저작이 혼란스럽고, 지하철 승강장 구간은 어차피 직선이다. 셀 번호는 디버그 오버레이에서 그대로 읽힌다 |
| 열차 커서 픽 | 언제나 잡힘 | **폰이 문 앞 셀에 서 있고 문이 열렸을 때만** | 4 m짜리 차체가 언제나 잡히면 그 뒤 승강장 셀을 영영 클릭할 수 없다. 열차를 `GetBlockActors`에도 넣어 바닥 트레이스가 통과하게 했다 |
| 하차 위치 | `ExitWorldOffset`(월드 벡터) | **`ExitCell`(셀)** | 이 프로젝트의 다른 모든 저작과 같은 단위. 구조물의 `TargetFloorCell`과도 일관된다 |
| 블록 이동 제약 | `CanSlide`에 직접 삽입 | **`CanStartMoving` + `CanOccupyRect` 가상 훅** | 구간 검사·탑승자 검사·층 높이 검사가 서로 다른 이유인데 한 함수에 몰아넣으면 파생 클래스가 끼어들 자리가 없다 |
| 도킹 상태 | 알림으로만 갱신 | **매 틱 재판정 + 알림은 피드백용** | 차체가 도크를 벗어나는 경로가 여럿(드래그, 회전판, 승강)이라 나갈 때마다 알림을 거는 것보다 현재 상태를 보는 편이 틀릴 여지가 없다 |
| 문 시간 | 닫히는 시간만(`DoorCloseSeconds`) | **열리는 시간도 별도 단계**(`DoorOpeningSeconds` + `DoorsOpening`) | 2026-09-10 추가. 문 애니메이션을 붙이려면 여는 연출에도 시간이 있어야 하고, 그 시간 동안 승하차가 열려 있으면 폰이 움직이는 문을 통과해 들어간다. 애니메이션 자리는 `AnimateDoorsOpening/Closing(Alpha)` 훅으로 비워 두었다 |
| 엘리베이터 문 | 한쪽 | **양쪽(기획 반영)** | 설명자료의 "문이 양방향으로 있어 회전 방향은 중요하지 않다" |
| 승강 방향 | 한 방향(올라가면 끝) | **왕복** | 구조물이 "차체가 지금 어느 층에 있는가"로 목표를 고른다. 한 방향뿐이면 위층에 올라간 플레이어가 되돌아올 방법이 없어 테스트 한 바퀴가 끊긴다. Q3의 절반이 이걸로 해소됐다 |

### 9.3 구현하며 드러난 배치 제약

**퍼즐 구간은 승강 구조물까지 품어야 한다.** 블록은 소속 구간 밖으로 한 칸도 나가지 못하는데,
엘리베이터도 블록이다. 구조물이 구간 밖에 있으면 차체를 거기까지 밀 수 없다. 5절 표의 구간 A를
`x 4~25`로 넓혀 구조물(x 22~25)을 포함시켰다. 배치할 때 이 관계를 먼저 확인할 것.

컴파일에서 걸린 것 셋. `TObjectPtr<T>*`와 `T**`를 섞을 수 없어 슬롯 배열 타입을 맞췄고,
`GridHUD.cpp`에서 지역 변수 이름이 바깥 `GridPawn`을 가려 C4456이 에러로 올라왔으며,
새로 만든 파일에 UTF-8 BOM이 없거나 중복돼 MSVC가 첫 줄을 깨뜨렸다(프로젝트의 모든 소스가 BOM을
가진다 — 새 파일도 맞출 것).

## 10. FlowTest_1 실제 배치 (2026-09-08)

5절 계획에서 달라진 점과 실측값이다. 배치는 에디터 내장 MCP 서버(`http://127.0.0.1:8000/mcp`)에
HTTP JSON-RPC로 붙어 수행했다.

### 10.1 계획과 달라진 점

| 항목 | 계획 | 실제 | 이유 |
|---|---|---|---|
| 레벨 템플릿 | Empty Open World (WP + OFPA) | **`MovementTestMap` 복제(비 WP)** | MCP에 새 레벨 도구가 없다. 기능 테스트 맵이라 외부 액터 파일이 흩어지지 않는 단일 `.umap`이 오히려 다루기 쉽다 |
| 바닥·계단 | 아트 그레이박스 모듈(`SM_Floor*`, `BP_escalator_*`) | **엔진 큐브 슬랩** | 아트 모듈의 실제 치수를 아직 재지 않았다. 레이아웃을 먼저 확정하고 치수를 확인한 뒤 교체한다(Q6) |
| 에스컬레이터 | 45도 경사로 | **1 m 단차 계단 8단, 두 줄** | 회전된 경사로는 끝점 맞추기가 까다롭고, 1 m 챌판은 이 프로젝트의 설계 언어 그대로다(`MaxStepHeight` 110이 잇는다) |
| 선로 바닥 | Blocked 마커로 막기 | **그리드 아래(Z −1000)에 두기** | 그리드가 ActorZ(−700)까지만 트레이스하므로 저절로 NoFloor가 된다. 마커가 필요 없다 |
| 퍼즐 배치 | 회전판 (12,14) | **엘리베이터 (8,12) → 회전판 (8,20) → 구조물 (22,20)** | 엘리베이터가 문 축(Y)으로만 움직이므로 회전판은 **북쪽**에 있어야 한다. 동쪽에 두면 닿을 수 없다 |
| 개찰구 | x 33~34, y마다 1 m 틈 | **벽 4개 + 틈 3개(y 12·18·24)** | 틈이 12개면 개찰구가 아니라 울타리다. 벽 윗면은 Box Marker로 막았다(트레이스가 지붕을 바닥으로 굽는다) |

### 10.2 최종 좌표

그리드 원점 `(0, 0, −700)`, 90 x 30 셀, `RegionHeight 1200`, `MaxStepHeight 110`, `MaxSlopeAngle 50`.
승강장 Z −600, 상층 랜딩 Z +200, 선로 Z −1000.

| 액터 | 셀 | 설정 |
|---|---|---|
| `PuzzleRegion_A` | (4,10) 22x16 | 이름 StationA, 클램프 켬. 엘리베이터·회전판·구조물·블록을 모두 품는다 |
| `Elevator_1` | (8,12) 4x4 | 문 North → 이동 축 Y |
| `RotationTile_1` | (8,20) 4x4 | 시계 방향 |
| `ElevatorDock_1` | (22,20) 4x4 | 출구 East, 목적 셀 (26,21), 220 cm/s |
| `PuzzleBlock_A` | (9,17) 2x1 | 엘리베이터의 북쪽 길을 막는다 |
| `PuzzleBlock_B` | (13,20) 1x2 | 엘리베이터의 동쪽 길을 막는다 |
| `Train_1` | 정차 (65,3) · (13,3) | 1400 cm/s, 문 5초, 순환 |
| `NPCSpawner_Platform` | (76,4) 그리드 밖 | Interval 5초, 최대 3명 |
| `NPCSpawner_TrainB` | (69,4) 그리드 밖 | OnDemand, 정차역 B의 하차 스포너 |
| `StageClear_1` | (12,9) | 열차 하차 후 도착 지점 |
| `PlayerStart` | (3,20) | 승강장 A 서쪽 |

**풀이**: 블록 A를 3칸 비켜 → 엘리베이터 북쪽 8칸 → 회전판에서 90도(문 축 Y→X) → 블록 B를
2칸 비켜 → 동쪽 14칸으로 구조물 도킹 → 문 앞 (21,20~23)에서 클릭 → 8 m 상승 → (26,21) 하차 →
개찰구 틈(y 12·18·24) 통과 → 계단으로 승강장 B → 문 열린 열차 클릭 → 정차역 A 자동 하차 →
(12,9) StageClear.

### 10.3 자동으로 확인한 것

| 항목 | 결과 |
|---|---|
| 게임 타깃 빌드 | **성공** |
| 에디터 타깃 빌드 | **성공** |
| 그리드 생성 | 90x30 = 2700 셀, walkable **1792** — 승강장 A 624 + 랜딩 384 + 승강장 B 720 + 계단 64와 정확히 일치 |
| 마커 적용 | 개찰구 지붕 42셀 + StageClear 1셀 = **overrides 43**, blocked 42, stageClear 1 |
| 개찰구 틈 (33,18) | `type=Walkable neighbours=-E-W` — 남북이 벽으로 막히고 동서만 열렸다. 개찰구가 맞다 |
| 개찰구 벽 (33,15) | `type=Blocked reason=Marker floorZ=450` — 지붕이 도달 불가 셀로 남지 않는다 |
| 층 분리 | `path (3,20) -> (26,21): unreachable` — **승강장 A에서 랜딩으로 걸어 올라갈 수 없다.** 엘리베이터가 유일한 통로다 |
| 계단 연결 | `path (28,20) -> (78,20): 54 step(s)` — 랜딩에서 계단을 거쳐 승강장 B까지 이어진다 |
| 하차 동선 | `path (17,6) -> (12,9): 8 step(s)` |
| 구간 소속 | 블록 4개 전부 `region StationA`, 소속 없음 경고 0건 |
| 구조물 | `Elevator dock, exit East, target cell (26,21)` |
| 열차 | `doors open at stop 0 'PlatformB'` → 버스트 3명 → `stop 1 'PlatformA'` → 순환. 5초 간격 유지 |
| 행인 진입 | 그리드 밖 (76,4)·(69,4)에서 각각 (76,6)·(69,6)으로 **걸어 들어온다** |
| 행인 소멸 | 마지막 경유 셀에서 `reached its destination` |
| FlowTest_1 경고·오류 | **0건** |
| `GreyBoxTest_1` 회귀 | 기존 블록·장애물·레버·회전판·행인 모두 정상. 구간이 없으므로 "movement is unrestricted" 경고만 뜨고 동작은 이전과 같다 |

### 10.4 배치하며 걸린 것

- MCP `set_properties`의 `values`는 **객체가 아니라 JSON 문자열**이다. 객체로 넘기면 성공을
  반환하면서 아무것도 바뀌지 않는다. 처음 그리드 설정이 통째로 무시된 원인이었다.
- `add_to_scene_from_class`의 `name`은 **레이블**이지 오브젝트 이름이 아니다. 이후 조회는
  `get_label`로 매핑해야 한다.
- `DebugInspectCell` / `DebugPathStart` / `DebugPathGoal`을 쓰면 `LogDebugReport`가 자동으로
  돌아간다. 함수 호출 도구가 없어도 셀과 경로를 검증할 수 있다.

### 10.5 배치 중 고친 코드

열차가 **탑승 보행이 끝나기 전에 문을 닫을 수 있었다.** 폰은 문 앞 셀에서 차체 안까지 직선으로
걸어 들어가는데, `bDepartAfterBoarding`이 남은 대기를 1.5초로 줄이는 바람에 보행이 끝나기 전에
출발할 수 있었다. 그러면 폰이 정지한 좌석 위치로 걸어가는 동안 열차가 빠져나가 뒤늦게 허공에서
붙는다. `DoorsOpen`에서 타는 중인 폰이 있으면 문을 닫지 않도록 했다.

## 11. 엘리베이터 아트 교체 (2026-09-08)

엘리베이터의 그레이박스 큐브를 아트의 `BP_elevator`로 바꿨다.

### 11.1 왜 리페어런팅이 아니라 자식 액터인가

`BP_elevator`를 `APuzzleElevatorBlock`의 자식 클래스로 리페어런팅하면 아트 블루프린트가 곧
게임플레이 액터가 되어 배치가 간단해진다. 그러지 않은 이유가 둘이다.

- **소유권.** `Content/Art`는 아트가 계속 고치는 영역이다(README 규칙). 리페어런팅은 아트 에셋의
  부모 클래스를 프로그래머 코드에 묶으므로, 아트가 그 블루프린트를 다시 만들거나 옮기면 양쪽이
  같은 파일에서 부딪힌다.
- **입력 충돌.** `BP_elevator`의 이벤트 그래프에는 `EventActorOnClicked`에 걸린 문 회전 연출이
  있다. 컨트롤러가 `bEnableClickEvents`를 켜 두므로, 부모로 삼으면 드래그를 시작할 때마다 문이
  같이 돌아간다.

대신 `APuzzleBlock`에 `VisualActorClass`(+ `UChildActorComponent VisualActor`)를 두고 아트를
자식으로 품는다. 아트 에셋은 한 글자도 바뀌지 않는다.

### 11.2 아트는 보여 주기만 한다

자식으로 생긴 아트 액터는 `SanitiseVisualActor()`가 손질한다: 모든 프리미티브의 콜리전을 끄고
`GridTraceIgnore` 태그를 붙인다.

- **커서 판정은 그레이박스 프록시가 계속 맡는다.** 아트가 트레이스를 가로채면 조각을 잡는 규칙이
  메시 모양에 좌우되고, 아트가 바뀔 때마다 조작감이 흔들린다. 큐브는 `SetVisibility(false)`로
  숨기되 콜리전은 남기므로, 눈에 보이는 것은 아트이고 잡히는 것은 언제나 풋프린트와 정확히 같은
  4x4 상자다.
- **그리드 생성이 아트를 바닥으로 굽지 않는다.** 그러지 않으면 조각이 우연히 놓인 자리가 지형으로
  굳는다.
- 엘리베이터는 아트가 자기 문을 갖고 있으므로 그레이박스 문 표시 두 장도 함께 숨긴다.

### 11.3 치수

`E_body`와 `Plane_002`가 X·Y 모두 −200~+200, 즉 **정확히 400 x 400 cm**이고 원점이 바닥 중앙이다.
`APuzzleBlock`의 "액터 위치 = 풋프린트 중심, Z = 바닥"과 그대로 맞아 보정이 전혀 없다. 높이는
`ac_pillar` 기준 약 275 cm로, 프록시 큐브의 `Height` 300보다 조금 낮다. 잡는 면이 아트 지붕보다
25 cm 위에 있지만 위에서 내려다보는 카메라에서는 차이가 드러나지 않는다.

### 11.4 확인한 것

| 항목 | 결과 |
|---|---|
| 에디터·게임 타깃 빌드 | **성공** |
| 자식 액터 생성 | `BP_elevator_C_0`, 태그 `GridTraceIgnore` |
| 아트 콜리전 | 아트만 튀어나온 지점(Y 1610) 트레이스가 아트를 지나쳐 **바닥 Z −600**에 맞는다 |
| 프록시 콜리전 | 엘리베이터 중심 트레이스가 **Z −300**(큐브 윗면)에 맞는다. 잡는 대상은 그대로다 |
| 그레이박스 숨김 | `BodyMesh.bVisible=false`, `DoorMesh.bVisible=false` |
| 그리드 통계 | walkable 1749 · blocked 42 — 아트 배치 전과 **완전히 동일** |
| PIE | 경고·오류 0건. 퍼즐·열차·행인 모두 이전과 같이 동작 |

다른 조각도 같은 방식으로 아트를 붙일 수 있다. `VisualActorClass`는 `APuzzleBlock`에 있으므로
일반 블록·회전 장애물·엘리베이터가 모두 쓴다.

## 12. 툰 셰이더 적용 (2026-09-08)

FlowTest_1을 `GreyBoxTest_1`과 같은 룩으로 맞췄다. 값은 전부 `GreyBoxTest_1`에서 읽어 그대로
옮긴 것이지 새로 정한 것이 아니다.

### 12.1 툰 셰이딩이 걸리는 두 갈래

| 갈래 | 내용 |
|---|---|
| **표면** | `MI_GreyBox_*`가 이미 툰 마스터 `M_ToonSurface`의 인스턴스다. 이 머티리얼을 붙이는 것만으로 표면이 툰 셰이딩을 탄다. 인스턴스마다 `LitColor` / `ShadowColor` / `Roughness 0.8`이 다르게 잡혀 있어 층 구분이 색으로 읽힌다 |
| **포스트 프로세스** | 언바운드 `PostProcessVolume`에 `MI_ToonOutline_Default`와 `MI_ToonShade_Default`를 블렌더블로 얹는다. 여기서 외곽선과 셀 음영이 나온다 |

둘 중 하나만 있으면 룩이 반쪽이 된다. 머티리얼만 있으면 외곽선이 없고, 볼륨만 있으면 표면이
일반 PBR로 칠해진다.

### 12.2 포스트 프로세스 볼륨

`bUnbound = true`(카메라가 볼륨 밖에 있어도 항상 적용 — 층을 오갈 때 셰이딩이 튀지 않는다),
우선순위 0, 블렌드 가중치 1. 오버라이드 18개는 전부 **PBR스러운 것을 끄는 방향**이다.

| 묶음 | 값 |
|---|---|
| 톤 커브 | `ToneCurveAmount 0`, `FilmSlope 1`, `FilmToe/Shoulder/BlackClip/WhiteClip 0`, `ExpandGamut 0` — 필름 톤 매핑을 평평하게 만들어 셀 색이 그대로 나온다 |
| 노출 | `AEM_Manual`, `AutoExposureBias 0`, 물리 카메라 노출 끔 — 층을 오갈 때 밝기가 튀지 않는다 |
| 끄는 것 | Bloom, SceneFringe, Vignette, FilmGrain, AmbientOcclusion, MotionBlur 전부 0 |
| 라이팅 | `ReflectionMethod None`, `DynamicGlobalIlluminationMethod None` — Lumen을 꺼서 셀 음영이 간접광에 흐려지지 않는다 |

### 12.3 라이팅

`GreyBoxTest_1` 실측값 그대로다.

| 액터 | 값 |
|---|---|
| DirectionalLight | Movable, 세기 6, 색온도 6500 K, 피치 −60 / 요 −30, Atmosphere Sun Light 켬, 동적 그림자 거리 40000 |
| SkyLight | Movable, Real Time Capture, 세기 1.0 |
| ExponentialHeightFog | 밀도 0.005, 낙차 0.2, 볼류메트릭 끔 |
| SkyAtmosphere | 기본값 |

### 12.4 층별 머티리얼 배정

FlowTest_1의 바닥은 엔진 큐브라 에셋에 머티리얼이 없다. 액터마다 `OverrideMaterials`로 붙였다.
`GreyBoxTest_1`의 층 색 규칙(F0 위 · B1 중간 · B2 아래)을 높이에 맞춰 옮긴 것이다.

| 대상 | 머티리얼 |
|---|---|
| 승강장 A · B, 선로 바닥 (Z −600) | `MI_GreyBox_B2` |
| 상층 랜딩 (Z +200) | `MI_GreyBox_B1` |
| 계단 두 줄 | `MI_GreyBox_Stage000` |
| 개찰구 벽 | `MI_GreyBox_F0` |

퍼즐 조각은 이미 코드에서 `MI_GreyBox_Movable`(주황)을 쓰므로 손댈 것이 없었다.

### 12.5 확인한 것

| 항목 | 결과 |
|---|---|
| 볼륨 설정 | 오버라이드 18개, 블렌더블 2개 — `GreyBoxTest_1`과 항목·값이 동일 |
| 머티리얼 | 바닥 31개 전부 적용 성공, 실패 0건 |
| 룩 비교 | 같은 카메라 각도로 두 레벨을 캡처해 대조. 라벤더 그림자, 검은 외곽선, 주황 조각, 평평한 셀 음영이 동일하게 나온다 |

프로젝트 설정(Lumen·VSM·정적 라이팅 끔)은 `Config/DefaultEngine.ini`에 있어 레벨과 무관하게
공유되므로 따로 손댈 것이 없었다.

## 13. 남은 일 — 손으로 확인할 것

마우스 입력은 자동화할 수 없다. 아래는 사람이 PIE(`ltts.GridDebug 2`)에서 확인해야 한다.

- [ ] 블록 A·B를 구간 경계 밖으로 밀면 "This piece cannot leave the puzzle area."
- [ ] 블록 A를 치우기 전에는 엘리베이터가 북쪽으로 밀리지 않는다.
- [ ] 엘리베이터가 회전판에서 90도 돌고, 그 뒤로 동서로만 밀린다.
- [ ] 구조물에 도킹하면 패드 색이 바뀌고 "The elevator is in place." 문구가 뜬다.
- [ ] 도킹 전에 엘리베이터를 클릭하면 "Push the elevator onto its dock first."
- [ ] 문 앞 셀에서 클릭하면 폰이 걸어 들어가고, 8 m 올라간 뒤 (26,21)로 걸어 내린다.
- [ ] 상승 중에는 클릭·드래그가 무시되고 HUD에 탑승 상태가 뜬다.
- [ ] 랜딩에서 다시 타면 **내려온다**(구조물이 왕복한다).
- [ ] 개찰구 벽을 정면으로 클릭하면 공이 부딪혀 흔들린다(`Bump`).
- [ ] 문 닫힌 열차를 클릭하면 "The doors are closed.", 문 앞이 아니면 "Stand at a door to board."
- [ ] 문 열린 열차를 클릭하면 타고, 정차역 A에서 자동으로 내려 (12,9)까지 걸어가 StageClear가 뜬다.
- [ ] 열차 뒤쪽 승강장 셀이 계속 클릭된다(차체가 커서를 먹지 않는다).
- [ ] 행인이 플레이어와 겹칠 때 반투명해진다.

## 14. 에스컬레이터 재구현 (2026-09-08)

3.10을 코드로 옮겼다. 같은 날 오전의 첫 구현은 통째로 버렸다(3.10.1).

### 14.1 바뀐 파일

| 파일 | 내용 |
|---|---|
| `Vehicle/GridEscalator.h/.cpp` | 새로 씀. 옛 `Grid/GridEscalator.*`(임시 받침 액터 포함)는 삭제 |
| `Player/GridPawn.h/.cpp` | `BoardVehicle`에 세 번째 인자 `USceneComponent* FollowComponent`(기본값 없음=지금까지대로), 멤버 `RideAnchor`, 헬퍼 `RideBaseLocation()` |
| `Player/GridPlayerController.cpp` | include 경로, 그리고 에스컬레이터 픽을 **언제나** 잡도록 조건 제거 |
| `Grid/GridActor.*`, `Grid/GridTypes.h` | 첫 구현이 넣었던 `BlockCellExit` / `CanLeaveCell` / `OppositeDirection`을 **되돌렸다**. 그리드는 다시 탈것을 모른다 |

`AGridPawn`의 변경은 이 한 줄로 요약된다: 폰이 따라가는 기준점이 "탈것 액터의 원점"에서
"앵커 컴포넌트가 있으면 그것, 없으면 액터 원점"으로 바뀌었다. 엘리베이터와 열차는 인자를
넘기지 않으므로 동작이 그대로다.

### 14.2 계획과 달라진 점

| 계획 | 실제 | 이유 |
|---|---|---|
| 경로를 인스턴스마다 설정 | **블루프린트 기본값**에 넣었다 | 두 BP의 로컬 기하가 완전히 같고 차이는 경로 **순서**뿐이다. 클래스 기본값에 두면 새로 배치하는 인스턴스가 그냥 동작한다 |
| 좌석을 루트에 붙임 | 좌석은 **어디에도 붙이지 않는다** | 매 프레임 월드 좌표로 옮기므로 부모가 필요 없고, 아트 루트의 모빌리티가 무엇이든 경고가 나지 않는다 |
| 계단을 제거하거나 태그 | `Stair_Down_0~7`에 `GridTraceIgnore` **태그만** | 그레이박스 계단은 눈으로 볼 것과 클릭 대상으로 남는다. 빠지는 것은 그리드 굽기뿐이다 |

### 14.3 실측한 로컬 기하 (두 BP 공통)

레벨에서 아래로 트레이스를 쏘아 쟀다. 액터 로컬 좌표, 진행은 로컬 −Y다.

| 지점 | 실측 표면 z | 경로에 넣은 z (+70) |
|---|---|---|
| 아래쪽 발판 바깥 끝 (0, 290) | 94.3 | 164.3 |
| 아래쪽 평면과 경사의 이음매 (0, 15) | 100 | 170 |
| 경사와 위쪽 평면의 이음매 (0, −500) | 700 | 770 |
| 위쪽 발판 바깥 끝 (0, −610) | 705.2 | 775.2 |

트레이스로 잰 표면 높이 그대로 넣었더니 공이 발판에 묻혔다. 트레이스가 맞는 것은 발판 아래
난간 밑판이지 눈에 보이는 발판 윗면이 아니기 때문이다. 그래서 경로 z를 **70 cm 올려** 넣었다
(2026-09-08, 사용자 확인). 상승 610.9 cm, 경로 전체 길이 **1176 cm**. `RideSpeed` 75 cm/s이므로 한 번 타면 약 15.7초다.
`BP_escalator_up_s`는 이 순서 그대로, `BP_escalator_down_s`는 **뒤집어서** 넣는다.

### 14.4 자동으로 확인한 것

| 항목 | 결과 |
|---|---|
| 에디터·게임 타깃 빌드 | **성공**, 경고 0 |
| 블루프린트 컴포넌트 | `DefaultSceneRoot`·`AC_001`·`AC_T_002`·`Cube_011`·`Cube_012` 모두 그대로 |
| 그리드 재생성 | walkable 1717 · blocked 42 · noFloor 940 |
| 내려가는 에스컬레이터 구간 | (44,10) (46,10) (49,10) 모두 `NoFloor` — 걸어서 못 건넌다 |
| 타는 셀 | (41,10) `Walkable` floorZ 0, 이웃 `N-SW` — 동쪽(에스컬레이터 쪽)으로 나가는 길이 없다 |
| 내리는 셀 | (52,10) `Walkable` floorZ −600 (승강장) |
| 걷는 계단 `Stair_Up_0~7` | (46,20) `Walkable` floorZ −300 — 행인 동선은 그대로 |
| PIE BeginPlay | `BP_escalator_down_s_C_1: ride is 1176 cm long, boarding from 3 cell(s), first is (41,9).` 위쪽도 같은 형식으로 (58,25) |
| PIE 에스컬레이터 오류 | **0건** |

### 14.5 남은 문제 두 가지

**(1) `BP_escalator_up_s_C_0`의 위치가 맞지 않는다.** 지금 (5480, 2480, −674)에 있는데, 위쪽
끝이 허공에 떠 있다. 위층 바닥은 X 4200에서 끝나고 에스컬레이터 위쪽 발판은 X 4842에서
시작하므로 6.4 m가 비어 있다. 그래서 타면 올라간 뒤 내릴 셀을 찾지 못하고
`WalkOntoGrid`가 타기 전 셀로 되돌린다(오류 로그 한 줄). 내려가는 쪽과 똑같이 맞추려면
**(4850, 2000, −694)** 로 옮기면 된다 — 마커 `EscalatorLane_Up`이 이미 그 자리(Y 2000)를
가리키고 있다. 다만 그 자리에는 걷는 계단 `Stair_Up_0~7`이 있으므로, 옮긴 뒤 그 계단까지
`GridTraceIgnore`로 빼면 **행인이 층을 오갈 길이 없어진다.** 층 이동 수단을 어떻게 나눌지는
기획 결정이라 손대지 않았다.

**(2) `EscalatorLane_Down` 마커가 남아 경고 24줄이 뜬다.** 이 마커는 에스컬레이터 구간을
`Walkable`로 강제하던 것인데 이제 그 셀에 바닥이 없다. 덮어쓰기는 무시되므로 동작에는
영향이 없고 로그만 시끄럽다(`override on cell (42,9) ignored -- that cell has no floor.`).
지우면 깨끗해진다.

### 14.6 아직 재지 않은 값

`AnimStepSpeedAtRate1`은 **0**이다. `AC_SCALE_ANIM_Anim`의 PlayRate 1에서 발판이 초당 몇 cm
나아가는지를 재서 넣으면, 발판과 그 위에 실린 공이 미끄러지지 않는다. 0인 동안은 애니메이션
속도에 손대지 않는다.

### 14.7 손으로 확인할 것 (마우스)

- [ ] 위층 (41,10) 부근에 서서 내려가는 에스컬레이터를 클릭 → 공이 걸어 들어가 발판을 따라 내려가 승강장 (52,10)에 선다.
- [ ] 발판 위를 걸어서 건너갈 수 없다(구간이 `NoFloor`라 클릭해도 경로가 없다).
- [ ] 승강장 쪽 끝에서 내려가는 에스컬레이터를 클릭 → "Stand at the near end to ride. This escalator runs one way."
- [ ] 타는 중에 클릭하면 무시된다. 내린 뒤에는 평소처럼 움직인다.
- [ ] 발판 애니메이션 방향과 공의 진행 방향이 같다(위·아래 각각).
- [ ] 엘리베이터·열차 탑승이 예전과 똑같다(`FollowComponent`를 넘기지 않는 경로).
