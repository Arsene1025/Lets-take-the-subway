# 지하철 아트 애니메이션 적용과 문 중심 승하차

작성일 2026-09-15. 대상: `AGridTrain`(`Source/LetsTakeTheSubway/Vehicle/GridTrain.h/.cpp`), `VehicleSeat.h`,
`GridPlayerController`, `Content/Core/Vehicle/BP_Train_Subway.uasset`, `BP_Train_Subway_S2.uasset`.
선행 문서: `StationFlow.md`(열차 원설계, 문 애니메이션 훅), `BoardingRefactor.md`(예약 탑승, 좌석 투영),
`Subway_Stage1.md`(열차 BP 신설), `StageRailExtension.md`(정차역 셀 재저작).

---

## 0. 왜

| 요청 | 현재 상태 |
|---|---|
| 1. 아트가 만든 지하철 애니메이션을 스테이지 열차에 적용 | 아트(ART_2 `556667f`)가 스켈레탈 메시 `SM_Subway_001`과 `Anim_Subway_DoorOpen/DoorClose/WheelRotate`를 올렸지만, 배치된 열차는 스태틱 메시 30개 조립체였고 `AnimateDoorsOpening/Closing` 훅은 빈 몸체였다. 애니메이션은 어떤 맵에도 없는 `BP_SubwayAnim`에만 물려 있었다 |
| 2. 승객이 문이 아닌 곳으로 드나듦. 양쪽 문짝 한가운데로, 높이는 그대로 | 탑승 좌석이 "폰이 선 칸을 진행축에 투영한 점"이라 문 폭 390 cm에 걸친 탑승 셀의 가장자리 칸(±200 cm)에서 타면 문 반폭 195 cm 밖으로 들어갔다. 하차는 저작된 `ExitCell`로 곧장 걸어 나왔는데, 이 셀이 승강장 가운데(열차 중앙)라 차체 벽을 비스듬히 뚫고 나왔다 |
| 3. 문이 열리고 닫히는 중에는 탑승 불가 확인 | 3절 |
| 4. 개방 애니메이션만 있으면 역재생으로 폐쇄 대체 | 없었음 |

## 1. 결정

1. **애니메이션은 재생하지 않고 짚는다.** 문 단계 진행도(`GetPhaseAlpha`)를 그대로 애니메이션 위치로 쓴다(`SetPosition`). 승하차를 막는 것은 단계 타이머이므로, 따로 재생하면 프레임이 튀거나 콘솔로 단계를 건너뛸 때 보이는 문과 탈 수 있는지가 어긋난다.
2. **문 시간은 애니메이션 길이에 맞춘다**(`bMatchDoorTimingToAnimation`, 기본 켬). 사용자 결정(2026-09-15). 문 열림·닫힘이 1.0 s에서 2.0 s로 늘었고, 탑승이 막히는 시간도 그만큼 늘었다.
3. **BP의 스태틱 메시 부품 30개는 제거했다.** 사용자 결정. 스켈레탈 메시에 차체·문·바퀴·연결 주름막·실내 좌석과 손잡이가 모두 들어 있음을 뷰포트 캡처로 확인했다(스태틱만/스켈레탈만 비교, 차량 안 시점 포함). 스태틱 메시 애셋 자체는 남아 있다.
4. **좌석은 가장 가까운 문의 한가운데.** 진행축 성분은 `DoorOffsetsLocal`의 문 중심, 폭 방향은 중심선이다. 2026-09-15 추가 요청으로 **높이는 폰이 서 있던 높이 그대로** 고정한다(예전 식은 객차 바닥 + `HeightAboveFloor`라 걸어 들어가며 공이 가라앉았다). 탑승 전에는 **문 한가운데와 일직선이 되는 자리로 먼저 옮겨 선 뒤** 똑바로 들어간다(4.2). 본 이름의 `L/R`이 한 문의 두 짝, `front/back`이 차체의 두 면이므로 "양쪽 문짝 한가운데"는 `DoorOffsetsLocal`과 같은 자리다(2절 실측).
5. **하차는 언제나 문 한가운데에서 문 앞 칸으로.** 기존 맵의 `ExitCell`은 고치지 않고 의미만 넓혔다: 문 앞 칸이면 착지 셀, 아니면 문 앞으로 나온 뒤 걸어갈 목표(`PostExitCell`이 비었을 때).

## 2. 실측 (에디터 MCP, 2026-09-15)

| 항목 | 값 | 의미 |
|---|---|---|
| `SM_Subway_001` 로컬 바운드 | X 0→3016.7, Y ±211, Z 0→600 | 스태틱 `SM_Subway1_Body`+`SM_Subway2_Body`와 같은 애셋 공간 |
| S1 BP 부품 트랜스폼(30개 공통) | Loc (−2239, −25, −54), Scale (1.5, 1.5, 1.3) | `ArtMesh`에 그대로 적용 |
| S2 BP 부품 트랜스폼(30개 공통) | Loc (−2089.73, −28, −58.15), Scale (1.4, 1.68, 1.4) | S1과 다르다. BP마다 따로 적용 |
| S1 문 중심 | −1609.0 / −739.0 / +703.6 / +1573.6 | 저작값 `[-1608.5, -739, 704, 1574]`와 0.5 cm 이내 |
| 애니메이션 길이 | DoorOpen 2.0 s, DoorClose 2.0 s, WheelRotate 1.0 s | RateScale 1, 루트모션 없음 |
| `SM_Subway_001` 머티리얼 | 슬롯 1개, 엔진 기본(WorldGrid) | `ApplyArtFallbackMaterial`이 스켈레탈도 칠하도록 확장 |
| S2 `BodyLength` | CDO·인스턴스 모두 **2120** | 실제 길이 3016.7 × 1.4 ≈ 4223의 절반(초기 한 량 실측값). 좌석 반폭이 960 cm로 잘려 양 끝 문(±1469, ±1501)으로는 문 중심에 앉지 못했다 → **4223으로 수정**. 클릭 프록시도 두 량 전체를 덮게 된다(S1 인스턴스 4525와 같은 기준) |
| S1 CDO `BodyLength` | 2271 (배치 인스턴스는 4525로 덮어씀) | 새로 배치할 열차를 위해 CDO도 4525로 수정 |

## 3. 문 개폐 중 탑승 차단 — 확인 결과(코드 변경 없음)

- `CanBoard`는 `Phase == DoorsOpen`이 아니면 거부한다. `TryBoard`는 반드시 `CanBoard`를 거친다.
- 컨트롤러는 거부되면 예약을 유지하고 문 앞에서 기다리다, 완전히 열린 프레임에 탄다.
- 하차는 `EnterDoorsOpen`(완전 개방 진입)에서만 일어난다.
- 탑승 중(Entering) 폐쇄 유예: `Rider`가 아직 좌석에 붙지 않았으면 문을 닫지 않는다.
- PIE 실측:
  - 폰을 문 앞 칸에 세우고 요청 → `doors opening`(프레임 380)~`doors open`(386) 동안 타지 않고, 387에 탔다.
  - `doors closing` 중에 문 앞 칸에서 요청 → 타지 않고 그대로 출발했다.
- 애니메이션을 같은 진행도로 짚으므로 보이는 문과 차단 규칙이 프레임 단위로 일치한다.

## 4. 설계

### 4.1 `VehicleSeat.h`

`SeatFacingRider`는 엘리베이터가 그대로 쓴다. 열차용으로 셋을 더했다.

| 함수 | 역할 |
|---|---|
| `AlongAxis(World, Centre, Axis)` | 진행축 위 위치(cm) |
| `NearestDoorIndex(World, Centre, Axis, DoorOffsets)` | 가장 가까운 문 |
| `SeatAtDoor(Centre, Axis, DoorOffset, HalfExtent, SeatZ)` | 문 한가운데 좌석. Z는 인자 그대로 |

### 4.2 `AGridTrain` 승하차

- 예약 탑승(`GridPlayerController::RequestTrainBoarding`): 먼저 `AGridTrain::GetDoorFrontCells`(문마다 문 한가운데에 가장 가까운 문 앞 칸 하나)로 보낸다. 그런 칸에 갈 길이 없을 때만 문 앞 칸 전체로 보낸다. 폰은 문 한가운데와 거의 일직선인 칸에서 문이 열리길 기다린다.
- `TryBoard`: 폰의 진행축 위치가 가장 가까운 문 중심에서 `DoorWidth/2 + CellSize` 안이면 `SeatAtDoor`, 아니면 예전 `SeatFacingRider`(손으로 적은 탑승 셀이 문 위치와 어긋난 그레이박스 맵 대비). 좌석 높이는 폰의 현재 Z.
- 정렬 경유점: 폰의 자리를 진행축 방향으로만 옮겨 문 중심과 일직선이 되는 점(Z 동일). `AGridPawn::BoardVehicle`의 `ApproachWorld`로 넘기면 폰은 경유점까지 걸은 뒤 좌석으로 들어간다(`PendingSeat`). 경유점이 걸을 수 없거나 점유된 칸에 떨어지면 옮기지 않고 그 자리에서 들어간다(승강장 구조물 속으로 파고들지 않게). 1 cm 안이면 생략.
- `EnterDoorsOpen` 하차:
  1. 착지 셀 = `ExitCell`이 이 역의 탑승 셀이면 그것, 아니면 탑승 셀 중 폰에게 가장 가까운 칸. 탑승 셀이 없는 역은 예전처럼 `ExitCell` 또는 차체 중심.
  2. 착지 셀을 마주 보는 문이 폰이 앉은 문과 다르면(짧은 승강장, 다른 문 앞에 저작된 `ExitCell`) 차체 안에서 그 문 한가운데로 옮긴 뒤 내린다. 높이는 폰의 현재 높이 그대로.
  3. 착지 셀이 문에서 진행축으로 `DoorWidth/2 + CellSize`보다 멀면 Warning.
- 하차 후 목표 `UnloadedGoalCell` = `PostExitCell` → 없으면 착지 셀과 다른 `ExitCell`. Stage1은 (114,73), Stage2는 기존 `ExitCell`로 이어서 걸어간다. 맵 재저작 불필요.

### 4.3 `AGridTrain` 아트

| 멤버 | 설명 |
|---|---|
| `ArtMesh` (`USkeletalMeshComponent`, 기본 서브오브젝트) | NoCollision, 단일 노드 모드, `AlwaysTickPoseAndRefreshBones`, 갱신 빈도 최적화 끔. 메시·트랜스폼은 BP 기본값 |
| `DoorOpenAnimation` / `DoorCloseAnimation` / `WheelAnimation` | 닫힘이 비면 열림을 거꾸로 짚는다 |
| `bMatchDoorTimingToAnimation` | BeginPlay에서 `DoorOpeningSeconds`/`DoorCloseSeconds`를 `GetPlayLength()/RateScale`로 |
| `WheelAnimSpeedAtRate1` | 0이 아니면 달리는 동안 PlayRate = 현재 속도 / 이 값. 에스컬레이터 `AnimStepSpeedAtRate1`과 같은 규약. S1 500, S2 540(추정값, 바퀴 둘레 기준) |
| `LogDoorBoneOffsets` (CallInEditor 버튼) | 문 본을 L/R·front/back을 뗀 이름으로 묶어 평균 X를 `DoorOffsetsLocal`과 비교해 로그로 찍는다 |

동작:
- `ScrubDoorAnimation`: 애니메이션이 바뀔 때만 `SetAnimation`(재생 위치를 0으로 되돌리므로 매 틱 부르면 안 된다), 재생 중이면 `Stop`, `SetPosition(Alpha × 길이, 노티파이 없음)`. 멈춰 있어도 컴포넌트 틱이 포즈를 평가한다(엔진 `AnimSingleNodeInstanceProxy`).
- `EnterDoorsOpen`에서 `AnimateDoorsOpening(1)`, `EnterMoving` 맨 앞에서 `AnimateDoorsClosing(1)` → 바퀴 루프 재생. 문과 바퀴는 같은 단일 노드 슬롯을 나눠 쓰지만 정차/주행으로 갈려 겹치지 않는다. 바퀴 클립은 문 본을 키잉하지 않아 레퍼런스 포즈(닫힘)가 된다.
- `ArtMesh->AddTickPrerequisiteActor(this)`로 짚은 위치가 같은 프레임에 반영된다.
- 메시가 없으면(그레이박스) 모든 아트 함수가 즉시 반환한다. BP가 `AnimateDoors*` 이벤트를 구현하면 **부모 호출을 넣어야** 기본 구현이 돈다.

> 구현 중 버그: `ScrubDoorAnimation(GetDoorClosingAnimation(bReverse), Alpha, bReverse)`처럼 한 호출식에서 `bReverse`를 채우고 읽으면 MSVC가 인자를 오른쪽부터 평가해 역재생이 정방향으로 돌았다(닫힘 → 열림 → 끝에서 닫힘으로 튐). 애니메이션을 먼저 변수로 받도록 고쳤다.

### 4.4 시험 명령 `ltts.TrainRide [이름 일부] [시작 셀 X] [시작 셀 Y]`

`GridPlayerController::RequestTrainBoarding`을 새로 두고 클릭 경로와 공유한다(`ltts.ElevatorRide`와 같은 이유). 시작 셀을 주면 폰을 그 셀로 옮긴 뒤 요청한다. 문 가장자리 칸 탑승 같은 경우를 재현할 때 쓴다. 탑승 로그에 어느 문으로, 진행축 몇 cm 좌석에 앉았는지 남긴다.

## 5. BP 편집 (완료)

두 BP 모두 같은 절차, 트랜스폼만 2절 표 값.
1. 상속된 `ArtMesh`: Skeletal Mesh `SM_Subway_001`, 위치·스케일 지정.
2. Class Defaults: 애니메이션 3종, `WheelAnimSpeedAtRate1`, `bMatchDoorTimingToAnimation` 켬, `BodyLength`(S1 4525, S2 4223).
3. `SM_Subway*` 스태틱 부품 30개 제거. 남은 컴포넌트: `SceneRoot`, `ArtMesh`, `BodyMesh`, `DoorMesh0/1`.
4. 맵은 수정하지 않았다. 배치 인스턴스는 BP 기본값을 그대로 받는다(Stage1·Stage2 재로드로 확인).

## 6. 검증 (PIE / Simulate)

| 확인 | 결과 |
|---|---|
| 설정 로그 | Stage1·Stage2 세 대 모두 `doors opening 2.0 s / open 5.0 s / closing 2.0 s` |
| 문 열림·닫힘(연속 캡처) | 열림 단계에서 서서히 열리고, 닫힘 단계 2초 동안 서서히 닫히고, 출발 시점에 완전히 닫힘. 달리는 동안 닫힘 |
| 역재생(`DoorCloseAnimation` 비움) | 수정 후 열린 상태에서 서서히 닫힘 확인 |
| Stage1 가장자리 칸 (106,80) 탑승 | 0번 문, 좌석 −1608 cm(문 중심). 차체 벽(폭 방향 300 cm)을 지나는 지점이 문 중심에서 약 144 cm(문 반폭 195) |
| Stage1 하차(문 재개방) | 좌석 → 문 앞 가운데 칸 (106,78)으로 수직 하차 → `PostExitCell` (114,73)로 이동 |
| Stage2 `Train_Line1` 끝 문 가장자리 칸 (369,48) 탑승 | 0번 문, 좌석 −1501 cm. 벽 통과 지점이 문 중심에서 약 74 cm(문 반폭 182). 하차는 (369,49)로 수직, 이후 `ExitCell` (368,65)로 이동 |
| 높이(추가 요청 전) | 좌석 Z = 열차 Z + 50(`HeightAboveFloor`) |
| 정렬 후 탑승·높이 고정(추가 요청) | Stage1: (106,80)·(106,76)에서 요청 → 문 중심 칸 (106,78)로 걸어가 대기 → 문이 열리면 진행축으로 −8 cm 정렬 → 폭 방향 400 → 0으로 수직 진입(진행축 −1608.5 유지). 폰 Z는 승강장 21.4에서 탑승·주행·하차 내내 그대로(열차 Z −124) |
| 그레이박스 `FlowTest_1`(메시 없는 `AGridTrain`) | 경고 없음, 문 시간 1.0 s 유지. 저작 `ExitCell` (69,6)이 1번 문 앞이라 차체 안에서 1번 문 중심으로 옮긴 뒤 수직 하차 |
| 맵 파일 | 변경 없음 |

## 7. 남은 것

- **직접 확인**: 실제 클릭으로 Stage1 동쪽 → 서쪽, Stage2 A′ → B, D → C 왕복. 이번 검증은 `ltts.TrainRide`와 `ltts.TrainDoors`로 같은 역에서 탔다 내렸다.
- 정렬·높이 고정 추가 뒤 Stage2는 PIE로 다시 재지 못했다(에디터가 네트워크 확인 요청 시간 초과로 프레임당 수 초씩 멈춤). 특히 Stage2 `S2_PlatformA` 0번 문은 문 앞 칸이 (369,48)·(369,49) 두 칸뿐이라 가장 가까운 칸도 문 중심에서 약 1 m 떨어져 있다. 문 중심 쪽 칸 (369,50)이 걸을 수 없는 칸이면 정렬이 생략되어 비스듬히 들어가고(로그 `the spot in line with the door is blocked`), 걸을 수 있는 칸(높이만 다른 바닥)이면 승강장 높이 그대로 옆으로 1 m 옮긴 뒤 들어간다. 어느 쪽인지 확인하지 못했다.
- 높이를 승강장 기준으로 고정했으므로 승강장과 객차 바닥 높이가 다르면 공이 객차 바닥에서 떠 보이거나 파묻혀 보일 수 있다.
- 바퀴 회전 속도(`WheelAnimSpeedAtRate1`)는 추정값이다. 달리는 열차를 보고 미끄러져 보이면 조정한다.
- `Log Door Bone Offsets` 버튼은 MCP로 누를 수 없어 실행하지 않았다. 문 위치는 2절 스태틱 바운드 실측으로 확인했다.
- 열차 안이 들여다보이는 카메라에서는, 문 앞 승강장이 없는 문에 앉았다가 다른 문으로 옮겨 내리는 순간(4.2-2)이 보일 수 있다.
- 참고: Stage2 `S2_PlatformA` 탑승 셀이 16칸으로 찍힌다(`StageRailExtension.md` 5.3 기록은 18칸). 이번 변경은 탑승 셀 계산에 손대지 않았으므로 그 사이의 맵·그리드 변경에서 온 차이다.
