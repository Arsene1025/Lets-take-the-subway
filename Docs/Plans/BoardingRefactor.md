# 탑승 리팩토링 — 엘리베이터 문 축 · 자동 접근 탑승 · Dock 쌍 · 문으로 똑바로 타기

작성일 2026-09-11. 출처: 사용자 요청 5건(채팅 + 그림). 브랜치 `PROG_1`.
선행 문서: `StationFlow.md`(엘리베이터 구조물·열차·에스컬레이터 원설계), `Subway_Stage1.md`(스테이지 1 배치),
`RushHourPuzzle.md`(블록·클릭/드래그 규칙).

---

## 0. 왜 고쳤나

| # | 요청 | 고치기 전 | 고친 뒤 |
|---|---|---|---|
| 1 | 엘리베이터 입구를 (−Y,+Y)·(−X,+X) 두 축으로 묶기 | `EGridDirection DoorDirection` 네 방향. 문은 양면이라 North와 South가 같은 배치를 뜻하는 서로 다른 값이었다 | `EPuzzleMoveAxis DoorAxis` 두 축. 구버전 값은 로드할 때 자동으로 옮긴다 |
| 2 | 엘리베이터 클릭 시 가장 가까운 입구로 이동 후 자동 탑승 | 문 앞 셀에 **미리 서 있어야** 탔다. 아니면 "Walk around to a door first." | 클릭 = "타겠다". 폰이 가장 가까운 문 앞까지 걸어가 자동으로 탄다 |
| 3 | Dock으로 상·하 목표 지정 | `TargetFloorCell` 셀 번호 + `TravelHeight` 예비값. 레벨에서 맞추기 까다로웠다 | 층마다 Dock을 하나씩 놓고 `TargetDock`으로 서로를 가리킨다 |
| 4 | 지하철 탑승이 대각선이라 부자연스러움 | 좌석이 차체 중심이라 문 앞에서 X·Y가 동시에 움직였다 | **열차만** 좌석이 폰을 마주 본다. 문에 수직으로만 들어간다. 엘리베이터는 지시대로 차체 한가운데로 모인다(2.4절) |
| 5 | 지하철도 멀리서 클릭하면 최단거리 이동 후 자동 탑승 | 문 앞 셀이 아니면 클릭이 뒤쪽 바닥으로 흘러가 폰이 말없이 걸어갔다 | 2번과 같은 예약 탑승을 쓴다. 문 앞에서 열차를 기다린다 |

## 1. 사용자 결정 (2026-09-11)

- **열차는 언제나 클릭할 수 있다.** 이동 중이어도, 문이 닫혀 있어도 누르면 승강장 문 앞으로 걸어가 기다린다.
  대가로 **차체 뒤 승강장 셀은 클릭할 수 없다**(차체가 커서를 가로챈다). 알고 고른 대가다.
- **걸어오는 동안 열차는 문을 잡아 두지 않는다.** 늦으면 다음 열차를 기다린다.
- **구버전 Dock 저작(`TargetFloorCell`·`TravelHeight`)은 지우지 않고 예비로 남긴다.** 테스트 맵
  `FlowTest_1`·`GreyBoxTest_1`이 그 방식으로 저작돼 있고, 구조물 하나로 충분한 배치에서는 여전히 가장 짧다.

## 2. 설계

### 2.1 문은 방향이 아니라 축이다 (`APuzzleElevatorBlock`)

`DoorAxis`(`EPuzzleMoveAxis`, `InvalidEnumValues="None, Both"`)가 새 저작 값이다. 이동 축은 예전처럼
문에서 유도하므로(`GetWorldMoveAxis` → `GetWorldDoorAxis`) 둘이 어긋날 수 없다.

**구버전 이전이 까다로웠던 점.** UHT는 `_DEPRECATED` 접미사를 붙여도 프로퍼티의 엔진 이름을 바꾸지
않는다(`UhtPropertyParser.cs`는 `EPropertyFlags.Deprecated`만 세운다). 즉 이름을 바꾸면 저장된 값이
조용히 버려진다. 그래서 **이름은 `DoorDirection` 그대로 두고** 에디터에서만 감췄다(`UPROPERTY()`).

이전은 `PostLoad`에서 한 번만 일어나며, `bDoorAxisMigrated` 표식이 그것을 보장한다. 표식이 없으면
로드할 때마다 옛 방향이 되살아나 **새로 지정한 축을 다음 로드가 덮어쓴다.** 저작에서 `DoorAxis`를
건드리는 순간 `PostEditChangeProperty`가 표식을 세운다.

`GetWorldDoorDirection()`은 남아 있다. 축의 양의 방향을 대표로 돌려주며, 문 앞 셀 계산처럼 방향
하나를 집어야 하는 곳에서 쓴다.

### 2.2 예약 탑승 (`AGridPlayerController::FPendingBoarding`)

```
클릭 → (탈 수 있는 상태인가?) → 문 앞 셀 목록 → 가장 가까운 셀로 걸어감 → 도착 → 탑승
```

- **예약은 컨트롤러가 갖는다.** 탈것은 자기를 타려고 누가 걸어오는지 알 필요가 없고, 폰은 자기가 왜
  그 셀로 가는지 알 필요가 없다. 조작의 상태이므로 조작하는 쪽에 산다.
- **거절은 누른 자리에서 한다.** `APuzzleElevatorDock::CanLaunch`를 `TryLaunch`에서 떼어냈다. 도킹
  안 됨·승강 중·같은 층·내릴 곳 없음은 폰을 보내기 전에 걸러진다. 문 앞까지 걸어가게 해 놓고 거기서
  거절하는 것은 조작을 두 번 낭비시키는 일이다.
- **열차만은 기다린다.** `CanBoard`가 거짓이어도 예약을 살려 둔다(문이 닫혀 있음, 아직 터널에 있음).
  사유는 **한 번만** 알린다 — 매 프레임 같은 줄을 다시 쓰면 그사이의 다른 안내를 전부 덮는다.
- **누름은 언제나 새 뜻이다.** `OnPressed`가 맨 앞에서 예약을 버린다. 그러지 않으면 플레이어가 다른
  곳을 눌러 폰을 돌려세운 뒤에도 예약이 살아남아 엉뚱한 순간에 태운다.
- **취소 조건**: 대상이 사라짐 · 폰이 다른 경로로 그리드를 떠남 · 폰이 문 앞이 아닌 곳에 멈춰 섬
  (`CurrentCell != DoorCell && !IsMoving()`).

`AGridActor::FindNearestReachableCell(From, Candidates, Pawn, OutCell, OutSteps)`가 "가장 가까운
입구"를 정한다. **직선 거리가 아니라 경로 길이**다 — 벽 하나를 사이에 둔 코앞의 문을 고르면 폰이 역을
한 바퀴 돈다. 후보를 맨해튼 거리로 정렬하고, 4연결 격자에서 경로 길이가 맨해튼 거리보다 짧을 수 없다는
사실로 가지치기하므로 문이 여덟 짝인 열차에서도 A*를 한두 번만 돌린다.

열차 쪽 후보는 `AGridTrain::GetApproachCells` — **정차역을 가리지 않고** 모든 역의 탑승 셀이다.
클릭한 순간 열차가 터널 한가운데 있을 수 있고, 그때 "지금 서 있는 역"은 아직 아무 데도 아니다.

### 2.3 Dock 쌍 (`APuzzleElevatorDock`, `APuzzleFloorTile`)

층마다 Dock을 하나씩 **샤프트의 같은 XY에** 놓고 `TargetDock`으로 서로를 가리킨다. 한쪽만 채워도
된다 — 비어 있으면 자기를 가리키는 Dock을 역으로 찾는다(게임 중에는 한 번만 훑고 캐시, 에디터에서는
매번 다시 본다).

같은 XY에 Dock이 여럿이므로 **층이 판정에 들어간다.**

| 문제 | 해결 |
|---|---|
| 샤프트 셀은 아래층 높이로 구워진다. 위층 Dock도 거기로 끌려 내려간다 | `APuzzleFloorTile::ResolveFloorZ` 가상 함수. Dock은 **영역 둘레 한 칸**(차체 문이 열릴 자리)의 바닥 중 **배치된 Z에 가장 가까운 것**을 자기 층으로 삼는다. 디자이너는 대충 그 층 높이에 놓기만 하면 된다 |
| 아래층 Dock이 위층에 걸린 차체를 자기 것이라 주장한다 | `FullyContains`를 가상으로 열고 Dock이 층 높이 일치(오차 10 cm)를 더한다. `FindElevatorOnTop`·`FindDockUnder`·`NotifyBlockCameToRest`가 전부 이 한 곳을 지난다 |
| 겹친 타일 경고가 샤프트마다 뜬다 | `CanCoexistWith` 가상 함수. 층이 다른 Dock끼리는 겹쳐도 된다 |

`APuzzleBlock::FloorZ`는 BeginPlay에서 셀 높이로 정해지고 승강이 끝날 때 `SetFloorZ(TargetZ)`로
갱신된다. 그래서 B1에서 밀려 들어온 차체는 샤프트 위에서도 B1 층으로 남고 **위층 Dock이** 그것을
받는다. 내려간 뒤에는 아래층 Dock이 받아 다시 올려 보낸다. 왕복이 저절로 성립한다.

**예비 경로는 건드리지 않았다.** `TargetDock`이 없으면 `ResolveFloorZ`·`FullyContains`·`ComputeTargetZ`
모두 예전 그대로 동작한다.

### 2.4 문으로 똑바로 타고 내리기 (`Vehicle/VehicleSeat.h`)

```cpp
FVector SeatFacingRider(RiderWorld, BodyCentreWorld, EdgeAxisWorld, HalfExtent, SeatZ);
```
문 면과 **나란한** 성분은 폰의 것을 그대로 쓰고, **수직인** 성분만 0(차체 중심선)으로 만든다. 그러면
폰은 선 자리에서 차체를 향해 똑바로 걸어 들어간다.

| 탈것 | EdgeAxis | HalfExtent |
|---|---|---|
| 열차 | `GetActorForwardVector()` (로컬 X = 진행축) | `BodyLength/2 − 100` |

**엘리베이터는 이 규칙을 쓰지 않는다**(2026-09-11 사용자 지시). 좌석이 **차체 한가운데**다. 탈것의
생김새가 다르기 때문이다: 열차는 45 m짜리 객차라 탄 문 앞에 그대로 서 있는 편이 자연스럽지만,
엘리베이터는 4 m짜리 방이고 문이 양쪽에 하나씩이라 어느 쪽으로 들어왔든 가운데 서는 것이 사람이
하는 짓에 가깝다. 한때 열차와 같은 규칙을 쓴 코드는 `GetSeatWorldFor`의
`ELEVATOR SEAT FACING RIDER DISABLED 2026-09-11` 블록(`#if 0`)에 남겨 두었다.

하차는 둘 다 "좌석에 가장 가까운 문 앞 칸"이다. 열차는 `ExitCell` 저작이 없을 때 그 역의 탑승 셀 중
**폰에게 가장 가까운 칸**으로 내린다(예전에는 `BoardingCells[0]`이라 객차 어디에 앉아 있었든 언제나
같은 문으로 나왔다). 엘리베이터는 좌석이 가운데이므로 `FindArrivalExit`가 문 한가운데 칸을 골라,
폰이 거기로 똑바로 걸어 나간다.

## 3. 변경 파일

| 파일 | 변경 |
|---|---|
| `Vehicle/VehicleSeat.h` (신규) | `LTTSVehicle::SeatFacingRider` |
| `Puzzle/PuzzleElevatorBlock.h/.cpp` | `DoorAxis` + `PostLoad` 이전 + `bDoorAxisMigrated`, `GetWorldDoorAxis`, `GetBoardableDoorCells`, `GetSeatWorldFor`, `IsPawnAtDoor`가 탈 수 있는 셀만 봄 |
| `Puzzle/PuzzleFloorTile.h/.cpp` | `GetFloorZ`, `FullyContains` 가상화, `ResolveFloorZ`, `CanCoexistWith` |
| `Puzzle/PuzzleElevatorDock.h/.cpp` | `TargetDock`, `GetTargetDock`(역방향 탐색+캐시), `CanLaunch` 분리, `FindArrivalExit`, 세 가상 함수 오버라이드, 구버전 값은 `Legacy` 카테고리로 |
| `Grid/GridActor.h/.cpp` | `FindNearestReachableCell` |
| `Vehicle/GridTrain.h/.cpp` | `GetApproachCells`, 좌석 투영, 하차 셀을 폰에게 가장 가까운 칸으로 |
| `Player/GridPlayerController.h/.cpp` | `FPendingBoarding` + 4개 함수, 엘리베이터·열차 클릭 분기, 열차 상시 픽, 호버에 문 앞 셀, 상태 텍스트 |

되살릴 수 있게 남긴 것: `PickUnderCursor`의 `TRAIN PICK GATED BY CanBoard DISABLED 2026-09-11`
(`#if 0`)과 그것이 채우던 `FCursorPick::VehicleRefusal` 소비 코드.

## 4. 레벨 저작 방법 (새 방식)

1. 층마다 `APuzzleElevatorDock`를 하나씩, **샤프트의 같은 XY**에 놓는다. Z는 그 층 바닥 근처면 된다.
2. 둘 중 **한쪽**의 `TargetDock`에 다른 쪽을 넣는다.
3. 각 Dock의 `ExitDirection`은 "그 층에 도착했을 때 차체 문 앞에 설 자리가 없으면 어디로 내릴지"다.
   보통은 문 앞 셀이 있으므로 쓰이지 않는다.
4. 구버전 값(`TargetFloorCell`·`TravelHeight`·`bTravelUp`·Detect Target Floor)은 `Legacy` 고급
   카테고리에 있다. `TargetDock`을 채웠다면 쳐다보지 않아도 된다.

## 5. 검증

빌드 (2026-09-11, 둘 다 **성공**):
- [x] 게임 타깃 `LetsTakeTheSubway Win64 Development`.
- [x] 에디터 타깃 `LetsTakeTheSubwayEditor Win64 Development`. Live Coding이 켜진 에디터가 떠
      있으면 UBT가 거부하므로 에디터를 닫고 빌드했다. **새 프로퍼티는 Live Coding으로 안 된다**
      (`Subway_Stage1.md` 9.11 주의 2).

PIE 로그로 확인한 것 (`Subway_Stage1`):
- [x] 두 엘리베이터의 문 축이 이전 뒤 정상. `ElevatorCar2` AxisY, `ElevatorCar3`는 아래 8절에 따라
      AxisX로 바꿨다.
- [x] 네 Dock이 서로를 찾았다. 한쪽에만 `TargetDock`을 넣었는데 반대쪽이 역방향 탐색으로 짝을 찾는다.

  ```
  PuzzleElevatorDock_1: Z  -29 -> PuzzleElevatorDock_0 at Z  772 (801 cm up).
  PuzzleElevatorDock_0: Z  772 -> PuzzleElevatorDock_1 at Z  -29 (801 cm down).
  PuzzleElevatorDock_3: Z  -26 -> PuzzleElevatorDock_2 at Z  771 (797 cm up).
  PuzzleElevatorDock_2: Z  771 -> PuzzleElevatorDock_3 at Z  -26 (797 cm down).
  ```
- [x] 같은 XY에 Dock이 둘씩 겹쳐 있어도 **"overlaps floor tile" 경고가 없다**(`CanCoexistWith`).
- [x] 위층 Dock이 `ResolveFloorZ`의 둘레 스캔으로 B1 높이(772 / 771)를 스스로 찾았다. 샤프트 셀은
      B2 높이로 구워져 있는데도 아래로 끌려 내려가지 않는다.
- [x] 열차 `Platform_B2_East`의 탑승 셀 20칸이 잡힌다 → `GetApproachCells`의 후보. 터널 정차역은
      탑승 셀이 없어 저절로 빠진다.
- [x] 남은 경고는 전부 이번 작업과 무관한 기존 배치 문제다(벽에 걸친 블록, 부착 면 없는 기둥).

PIE 로그로 확인한 것 (`FlowTest_1`, `GreyBoxTest_1` — 구버전 경로 회귀):
- [x] `PuzzleElevatorDock_0: Elevator dock at Z -600 (legacy), exit East, target cell (26,21).`
      짝이 없는 Dock은 예전 그대로 `TargetFloorCell`로 돈다.
- [x] `GridTrain_0`의 손 저작 탑승 셀 4개가 그대로 이긴다(`4 boarding cell(s) authored`).
- [x] 에스컬레이터 3대, 퍼즐 구간 소속(`region StationA`) 정상.

**마우스로 확인해야 하는 것** (로그로는 확인할 수 없다):
- [ ] 대합실 멀리서 엘리베이터 클릭 → 폰이 가장 가까운 문 앞으로 걸어가 자동 탑승 → 내려간다.
      문 앞 셀에서 좌석까지 **한 축으로만** 이동한다.
- [ ] 내려간 뒤 다시 클릭하면 아래층 Dock이 받아 **위로** 올려 보낸다(왕복).
- [ ] 차체를 샤프트로 밀면 그 층 Dock의 패드만 켜진다.
- [ ] Dock 밖 엘리베이터 클릭 → "Push the elevator onto its dock first." 폰은 걷지 않는다.
- [ ] 걸어가는 중 다른 바닥 클릭 → 예약 취소, 폰이 새 목적지로 간다.
- [ ] 열차가 터널에 있을 때 클릭 → 승강장 문 앞으로 걸어가 대기 → 문이 열리면 자동 탑승.
      좌석까지 문에 수직으로만 이동.
- [ ] 문이 열렸다 닫혀도 계속 기다린다(사유는 한 번만 표시).
- [ ] 벤치 드래그·레버·에스컬레이터가 예약 탑승과 간섭하지 않는다.

## 6. 레벨 작업 결과 (`Subway_Stage1`, 2026-09-11)

샤프트 두 곳에 위층 Dock을 하나씩 새로 놓고 짝을 이었다. 아래층 Dock 둘은 위치도 구버전 값도
건드리지 않았다(`TargetDock`이 있으면 쳐다보지 않는다).

| 액터 | 위치 | 층 Z | 설정 |
|---|---|---|---|
| `ElevatorDock_B1_East` (신규) | (−700, 1900) | **772** | `TargetDock` = `PuzzleElevatorDock`, exit South |
| `PuzzleElevatorDock` (기존) | (−700, 1900) | −29 | 그대로. 역방향으로 짝을 찾는다 |
| `ElevatorDock_B1_West` (신규) | (−5500, 9300) | **771** | `TargetDock` = `PuzzleElevatorDock2`, exit West |
| `PuzzleElevatorDock2` (기존) | (−5500, 9300) | −26 | 그대로 |

## 7. 측정 (2026-09-11, unreal-mcp `trace_world`)

계획을 쓴 뒤 사용자가 레벨을 더 만졌다. 작업 시작 시점의 상태와 바닥 실측은 다음과 같다.

| 액터 | 위치 | 비고 |
|---|---|---|
| `PuzzleElevatorDock` | (−700, 1900, −28.6) | 동쪽 샤프트, B2. 셀 (106,42) 4×4 |
| `PuzzleElevatorDock2` | (−5500, 9300, −25.9) | 서쪽 샤프트, B2. 셀 (58,116) 4×4 |
| `ElevatorCar2` | (200, −1800, 770), yaw 180 | 동쪽 **B1 대합실**, 셀 (115,5). 샤프트에서 멀고 퍼즐 구간 밖 |
| `ElevatorCar3` | (−5500, 9300, −25.9), yaw 180 | 서쪽 B2 Dock 위에 이미 올라가 있다 |

바닥 실측(위에서 아래로 트레이스):

| 샤프트 | 동쪽(+X) | 서쪽(−X) | 남쪽(−Y) | 북쪽(+Y) | 샤프트 안 |
|---|---|---|---|---|---|
| 동쪽 (106,42) | **772 (B1)** | 934 (벽) | 770 (B1) | **−28.7 (B2)** | −28.7 |
| 서쪽 (58,116) | **−26 (B2)** | **771 (B1)** | −26 (B2) | 바닥 없음 | −26 (차체 위 274) |

## 8. 스스로 판단해 바꾼 것 — 서쪽 엘리베이터의 문 축

`ElevatorCar3`의 문은 남북 축(AxisY)으로 저작돼 있었다. 그런데 실측대로 서쪽 샤프트의 승강장은
**동쪽이 B2, 서쪽이 B1**이고 북쪽에는 바닥이 아예 없다. 문이 남북을 향하면 B1 쪽으로 열리는 문이
하나도 없어 **올라간 뒤 다시 탈 수 없는 편도 승강기**가 된다.

그래서 `DoorAxis`를 **AxisX**로 바꿨다. 이제 B2에서는 동쪽 문, B1에서는 서쪽 문이 승강장을 향한다.
부수 효과로 차체를 미는 축도 동서로 바뀐다(문이 이동 축을 정의한다).

**되돌리려면** `ElevatorCar3`의 `Door Axis`를 `Y axis (north/south)`로 되돌리면 된다. 그 경우 서쪽
승강기는 내려오는 편도가 되고, B1에서 내릴 때는 Dock의 `ExitDirection`(West) 예비 경로를 탄다.

동쪽 샤프트는 손대지 않았다. 문이 남북인데 승강장도 남쪽(B1 770)·북쪽(B2 −28.7)이라 그대로 왕복한다.

## 9. 남은 일

1. 마우스 확인(5절 목록).
2. `ElevatorCar2`가 셀 (115,5)에 있어 샤프트(106,42)에서 멀고 `ElevatorShaft` 구간 밖이다
   (`region none`). 동쪽 퍼즐을 구성하려면 차체를 구간 안으로 되돌리거나 구간을 넓혀야 한다.
3. 열차 정차역 1(`Tunnel_North`)은 승강장이 없어 탑승 셀이 0이다. 연출용이라면 그대로 두면 된다.

## 7. 측정 (2026-09-11, unreal-mcp)

계획을 쓴 뒤 사용자가 레벨을 더 만졌다. 지금 상태는 다음과 같다.

| 액터 | 위치 | 비고 |
|---|---|---|
| `PuzzleElevatorDock_1` | (−700, 1900, −28.6) | 동쪽 샤프트, B2. `TargetFloorCell` (110,43), exit North |
| `PuzzleElevatorDock_3` | (−5500, 9300, −25.9) | 서쪽 샤프트, B2. `TargetFloorCell` (62,118), exit East |
| `PuzzleElevatorBlock_1` | (200, −1800, 770), yaw 180 | 동쪽 **B1 대합실**. `DoorDirection` North → `DoorAxis` AxisY |
| `PuzzleElevatorBlock_0` | (−5500, 9300, −25.9), yaw 180 | 서쪽 Dock 위에 올라가 있다. `DoorDirection` South → `DoorAxis` AxisY |

둘 다 문이 남북 축이라 **문 축 이전은 동작을 바꾸지 않는다**(East/West였다면 축이 뒤집혔을 것이다).

서쪽 샤프트는 확인이 더 필요하다: 서쪽 B1 대합실은 X[−11486,−5797]인데 Dock은 X −5500이라 그 위에
B1 바닥이 없어 보인다. 위층 Dock을 어디에 둘지는 실제 바닥 높이를 재고 정한다.

---

## 10. 스테이지 클리어 컷신 (2026-09-11 추가)

스테이지를 클리어하면 엘리베이터를 타고 올라가다 연출 컷신으로 넘어간다. 엔딩 승강기에는 갈 층도
내릴 바닥도 없으므로, 새 엘리베이터 클래스를 만들지 않고 **구조물에 스위치 하나**를 두는 쪽을 택했다.
같은 차체, 같은 조작, 같은 퍼즐이고 달라지는 것은 "이 구조물에 올려놓고 타면 어떻게 되는가"뿐이다.

### 10.1 저작 (`Elevator Dock|Stage Clear` 카테고리)

| 프로퍼티 | 기본 | 뜻 |
|---|---|---|
| `bIsClear` | false | 켜지면 **엔딩 승강기**. 짝 Dock도 구버전 목표 셀도 필요 없다. BlueprintReadWrite라 BP가 켤 수 있다 |
| `bClearOnStageClear` | false | 그리드의 StageClear 셀을 밟으면 `bIsClear`를 자동으로 켠다. BP 없이 "클리어 셀 → 엔딩 승강기"를 잇는 가장 짧은 길 |
| `ClearTravelHeight` | 1500 | 연출이 시작되기까지 움직일 거리(cm) |
| `bClearTravelUp` | true | 올라갈지 내려갈지 |
| `ClearTravelSpeed` | 0 | 0이면 `TravelSpeed`. 연출의 속도는 이동의 속도와 다르므로 따로 둔다 |

### 10.2 흐름

```
차체를 Dock 위에 완전히 겹치게 민다  (bIsClear여도 이 조건은 그대로다)
 → 차체 클릭 → 가장 가까운 문 앞으로 걸어가 자동 탑승 (기존 예약 탑승 그대로)
 → StartHoldingTravel: ClearTravelHeight만큼 이동
 → 목표 높이에서 **멈춰 선다**. 내려 주지 않는다
 → OnStageClearCutscene(NativeEvent) + OnClearCutscene(델리게이트)  ← 연출 자리
```

`CanLaunch`는 도킹과 "차체가 승강 중이 아님"까지만 보고 `bIsClear`면 곧바로 통과한다. 목표 층도 내릴
자리도 없는 것이 정상이므로 그 둘을 따지는 검사를 지난다.

### 10.3 붙잡아 두기 — `ETravelPhase::Holding`

`APuzzleElevatorBlock::StartHoldingTravel(TargetZ, Speed, Rider)`가 새 진입점이다. 기존
`StartVerticalTravel`과 준비 과정(`BeginTravel`)을 공유하고 도착 처리만 다르다.

- **`SetFloorZ`를 하지 않는다.** 도착 높이는 층이 아니라 허공이다. 층으로 기록하면 구조물이 "내 층의
  차체가 아니다"라며 도킹을 풀고, 연출이 시작되기도 전에 잠금이 한 겹 풀린다.
- `Holding`은 종착 상태다. `IsTravelling()`이 계속 true라 **입력 잠금 세 겹**(폰이 그리드 밖,
  차체가 `Lifting`, 구조물이 `IsBusy`)이 연출 내내 살아 있다. 폰은 차 안에 `Riding`으로 남는다.
- `OnArrived`는 발생하지 않는다. 도착이 아니라 정지이므로 `OnHoldReached`가 그 자리를 대신한다.

### 10.4 컷신 연결 — 둘 중 편한 쪽

1. **Dock을 BP로 파생**해 `OnStageClearCutscene(Elevator, Rider)`를 오버라이드한다.
2. **레벨 BP**에서 그 Dock 액터의 `OnClearCutscene(Dock, Elevator, Pawn)`에 바인딩한다. 구조물을
   BP로 바꾸지 않아도 된다.

둘 다 같은 순간에, **한 번만** 발생한다. 네이티브 기본 구현은 로그만 남긴다 — 열차의 문 연출 훅과
같은 규칙이다: 규칙은 C++가 쥐고 있고 연출이 비어 있어도 게임이 성립한다.

페이드는 `Start Camera Fade`, 씬 전환은 `Open Level` 등 BP 표준 노드로 한다. 둘 다 `Engine` 모듈이라
`Build.cs` 변경이 없다. 프로젝트에는 아직 LevelSequence 에셋이 하나도 없고 `LevelSequence`·`MovieScene`
모듈도 링크돼 있지 않다. 시퀀서를 C++에서 재생해야 할 때 `Build.cs`에 추가한다.

> **연출 BP가 차체나 폰을 파괴하거나 언포제스하면 안 된다.** 폰은 탈것이 사라지면 스스로 그리드로
> 내려서고(`AGridPawn::TickRide`), 차체는 승객이 사라지면 승강을 끝낸다. 레벨을 통째로 여는 씬 전환은
> 월드가 함께 사라지므로 상관없다.

### 10.5 확인한 것 (PIE 로그, 2026-09-11)

`PuzzleElevatorDock3`에 임시로 `bIsClear`·`bClearOnStageClear`를 켜고 `ClearTravelSpeed` 120으로 시험한 뒤
되돌렸다(저장 안 함).

```
PuzzleElevatorDock_5: Elevator dock at Z 771, stage-clear exit: rides 1500 cm up then the cutscene.
PuzzleElevatorDock_5: stage-clear exit at Z 771; rides 1500 cm up at 120 cm/s, then the cutscene.
```
- [x] 짝 없음 경고가 나지 않는다(엔딩 승강기는 짝이 없는 것이 정상).
- [x] 나머지 Dock 네 개의 짝 로그는 그대로 — 회귀 없음.
- [x] **탑승부터 컷신까지 끝까지 확인했다** (콘솔 명령 `ltts.ElevatorRide`, 평지 B1의 임시 구조물).
      폰이 문 앞 (118,9)까지 스스로 걸어가 타고, Z 770 → 1370으로 600 cm 올라간 뒤 멈춘 채 컷신 훅이
      한 번 불렸다. 시험 배치는 되돌렸다. 로그는 `Docs/StageClearElevator.md` 7절에 있다.

**시험용 콘솔 명령을 하나 두었다.** `ltts.ElevatorRide [이름 일부]`는 차체를 클릭하는 것과 **같은
함수**(`AGridPlayerController::RequestElevatorBoarding`)를 부른다. 탑승은 마우스로만 시작되므로
자동 시험이 닿지 못하던 구석이었는데, 특히 스테이지 클리어는 탑승부터 컷신까지 한 사슬이라 손으로만
확인할 수 있었다. 시험용 경로를 따로 만들면 시험이 통과해도 실제 조작이 통과한다는 보장이 없으므로
클릭 경로를 공개 함수로 뽑아 둘이 나눠 쓴다.

**저작 방법은 `Docs/StageClearElevator.md`에 따로 정리했다** -- 기획·레벨 쪽에서 읽을 문서라
설계 문서와 분리했다.

## 11. 그리드 수정 — `SM_B1_SignPanel003` (2026-09-11)

서쪽 B1 대합실의 셀 몇 개가 이웃과 높이가 달랐다. 원인은 `SM_B1_SignPanel003`이다.

안내판은 Z 770에서 **1325**까지 서 있는데, 그리드 생성 트레이스는 `원점 Z(−250) + RegionHeight(1100)`
= **Z 850**에서 시작한다. 즉 트레이스 시작점이 안내판 **안**이다. `GenerateFromTraces`는 그런 경우
(`Hit.bStartPenetrating`) 바닥 높이를 트레이스 꼭대기인 850으로 적고 Blocked/Clearance로 판정한다.
그래서 그 칸만 773이 아니라 850이 됐다. 개찰구 레인에서 겪은 것과 같은 문제다(`Subway_Stage1.md` 9.8).

고친 방법은 표식 액터들과 같다(`Subway_Stage1.md` 9.10): 안내판에 **`GridTraceIgnore` 태그**를 붙이고
그리드를 다시 생성했다. 안내판은 이제 셀을 막지 않는다.

| | 전 | 후 |
|---|---|---|
| walkable | 4047 | **4051** |
| blocked | 6799 | **6795** |
| noFloor | 6654 | 6654 |
| stepBreaks | 135 | 135 |

확인: 셀 (10~12, 113~119)이 전부 `type=Walkable floorZ=773.1 neighbours=NESW`로 균일해졌다.

**재생성을 부른 방법.** `Generate Grid`는 `CallInEditor` 버튼이라 MCP로 직접 부를 수 없고, Slate 자동화
클릭은 이 패널에서 동작하지 않았다. 대신 `AGridActor::PostEditChangeProperty`가 생성 관련 프로퍼티를
편집하면 `bAutoRegenerateOnEdit`에 따라 `GenerateGrid()`를 부른다는 점을 이용해 `RegionHeight`를
1101로 바꿨다가 1100으로 되돌렸다. 결과는 버튼을 누른 것과 같고, 최종 값도 원래대로다.
