# 그리드 NPC — 스포너에서 주기적으로 나와 경로를 따라 걷다 사라지는 행인

작성일 2026-09-07. 출처: 사용자 요청(채팅). 대상 레벨: `Content/Maps/GreyBoxTest_1` B2 동쪽 승강장.
선행 문서: `Docs/Plans/RotatingObstacle.md`(점유·콘솔 명령·배치 패턴), `RushHourPuzzle.md`.
구현 시작 시 이 문서를 `Docs/Plans/GridNPC.md`로 복사해 프로젝트 관례대로 유지한다.

---

## Context (왜 만드는가)

지하철역 레벨에 **행인 NPC**를 넣는다. 지금까지는 플레이어 폰과 퍼즐 조각만 그리드를 쓰고 있어
역이 비어 보인다. NPC는 (1) 플레이어처럼 그리드를 한 칸씩 걷고, (2) 스포너에서 주기적으로 나와
지정 경로를 따라 목적지에 도착하면 사라지며, (3) 스포너가 그리드 밖(범위 밖·바닥 없음·막힘)에
있어도 가장 가까운 걸을 수 있는 셀로 스스로 들어온다.

## 0. 요청 해석과 확정 사항

| 항목 | 결정 (2026-09-07 확정) |
|---|---|
| 경로 지정 | 스포너에 **경유 셀 목록**(`TArray<FIntPoint> Waypoints`)을 적는다. 셀 사이는 기존 A*(`AGridActor::FindPath`)로 자동 연결. 마지막 셀이 목적지 |
| 충돌 | NPC는 플레이어·퍼즐 블록과 **서로 통과**한다. 셀을 점유하지 않는다. 단 **플레이어와 겹치는 동안 NPC가 반투명**해진다 |
| 리스폰 | **고정 간격** `SpawnInterval` + **동시 수 제한** `MaxAlive`. 도착해 사라지면 슬롯이 빈다 |
| 비주얼 | 그레이박스 1종(엔진 기본 도형: 원기둥 몸통 + 구 머리 + 방향 표시 코). 종류 확장은 `NPCClass` 슬롯으로 나중에 |
| 그리드 밖 스폰 | NPC를 스포너 위치에 그대로 만들고, 가장 가까운 걸을 수 있는 셀까지 **직선으로 걸어 들어간** 뒤 경로를 시작한다. 순간이동이 아니다 |

## 1. 기존 시스템과의 관계

새로 만드는 것은 NPC 폰과 스포너 둘. 그리드 쪽 변경은 **태그 하나**뿐이다.

- 이동 루프는 `AGridPawn::Tick`(`Player/GridPawn.cpp:268-322`)의 것을 그대로 옮긴다:
  `VInterpConstantTo` 정속 보간, 도착 오차 0.5 cm, 도착 후 `CurrentCell` 갱신·`Path.RemoveAt(0)`.
  플레이어 폰은 카메라·컨트롤러 피드백이 붙어 있어 기반 클래스로 쓰지 않고, 루프를 **의도적으로
  복제**한다(플레이어 코드는 건드리지 않는다).
- 길찾기·최근접 셀은 `AGridActor::FindPath` / `FindNearestWalkableCell`(`Grid/GridActor.cpp:215-252`)
  재사용. 둘 다 `CanPawnEnter`를 거치므로 Conditional 규칙(`UGridCellRule_PawnHasTag`, 예: `HasTicket`)이
  NPC에도 그대로 적용된다. 스포너의 `NPCTags`로 필요한 태그를 NPC에 붙인다.
- **점유 통과**: `CanPawnEnter`(`GridActor.cpp:150`)와 `DescribeCell`(`:193`)의 점유 검사를
  "폰이 `GridPassThrough` 태그를 갖지 않을 때만" 하도록 바꾼다. 시그니처 변경 없음, 기존 호출자 전부
  무변경, 플레이어는 태그가 없으므로 동작 동일. 통과 여부는 질의마다가 아니라 **이동체의 속성**이므로
  태그가 맞다(`GenerationIgnoreTag`, `PawnHasTag`와 같은 관례).
- NPC는 `SetOccupant`를 **부르지 않고** `NotifyPawnEnteredCell`도 **부르지 않는다**(StageClear는
  플레이어 이벤트). 방어용으로 `AGridTestGameMode::HandleStageClear`에 `Cast<AGridPawn>` 가드를 넣는다.
- 반투명 머티리얼은 새 에셋을 만들지 않는다. 엔진의
  `/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent`(Translucent, `Color` 벡터 파라미터의 **알파가
  불투명도**; `CineCameraComponent`가 런타임에 같은 식으로 쓴다)를 MID로 쓴다. 프로젝트 툰 마스터
  `M_ToonSurface`는 Opaque라 인스턴스로는 반투명이 안 된다.
- 콘솔 명령 패턴은 `Puzzle/PuzzleRotatingObstacle.cpp:971-974`(`FAutoConsoleCommandWithWorldAndArgs`).
- 디버그 경로는 `FGridRuntimeDebugDrawer::DrawPath`를 **쓰지 않는다**(배치 ID 하나라 플레이어 경로를
  지운다, `Grid/GridDebug.cpp:39`). `ltts.GridDebug 2`일 때 매 프레임 `DrawDebugLine`으로 그린다.

## 2. 클래스

새 폴더 `Source/LetsTakeTheSubway/NPC/`. 로그는 `LogLTTSGrid`, `UCLASS(HideCategories = (Physics,
Collision, Networking, Input, LOD, Cooking, HLOD, DataLayers, Replication))`, 카테고리 `"Grid NPC"` /
`"NPC Spawner"`.

### `NPC/GridNPC.h/.cpp` — `AGridNPC : APawn`

폰인 이유: `CanPawnEnter`·`FindPath`가 `const APawn*`를 받고 Conditional 규칙이 폰 태그를 읽는다.
그리드 생성 트레이스도 폰을 무시한다(`GridActor.cpp:318`). 컨트롤러는 없다
(`AutoPossessAI = Disabled`, `AIControllerClass = nullptr`).

- **컴포넌트**: `SceneRoot`; `BodyMesh` 엔진 Cylinder(스케일 0.6, 0.6, 1.2, 바닥에 놓이도록 Z 오프셋),
  `HeadMesh` 엔진 Sphere(0.4), `NoseMesh` 엔진 Cube(10 cm, 머리 +X) — 원기둥+구만으로는 진행 방향이
  안 보인다. 머티리얼 `MI_GreyBox_Movable`(몸) / `MI_GreyBox_F0`(머리), 로드는 `PuzzleLever.cpp:62-75` 식.
  **모든 프리미티브 `NoCollision` + `ECR_Ignore`**(플레이어 폰과 같음, `GridPawn.cpp:25-33`). 퍼즐
  액터처럼 Visibility를 막으면 NPC 위를 클릭했을 때 바닥 클릭이 죽는다.
- **프로퍼티**: `MoveSpeed = 300`, `HeightAboveFloor = 60`, `bPassThroughOccupants = true`,
  `GhostMaterial`(기본 엔진 `M_SimpleUnlitTranslucent`), `GhostColor = (0.75, 0.85, 1.0, 0.35)`,
  `BlockedRetrySeconds = 0.5`. 생성자에서 `SpawnCollisionHandlingMethod = AlwaysSpawn`.
- **상태**: `Grid`(Transient), `TWeakObjectPtr<AGridNPCSpawner> OwnerSpawner`(로그용으로만 씀),
  `TWeakObjectPtr<AGridPawn> PlayerPawn`, `Waypoints`, `NextWaypointIndex`,
  **`TOptional<FIntPoint> CurrentCell`**(그리드에 들어오기 전엔 unset — 가짜 셀로 겹침 판정·재계획을
  하지 않기 위해), `TArray<FIntPoint> Path`, `RetryTimer`, `bGhosted`, `GhostMID`, `NormalMaterials`.
- **API**: `Initialize(AGridNPCSpawner*, const TArray<FIntPoint>& Waypoints, int32 EntrySearchRadius)`
  (지연 스폰과 `FinishSpawning` 사이에 호출), `GetCurrentCell()`, `GetNextCell()`, `IsOnGrid()`.

**BeginPlay**
1. `FindGrid` 실패 → Error 로그, `Destroy`.
2. `bPassThroughOccupants`면 `Tags.AddUnique(LTTSGrid::PassThroughOccupantsTag())`, 아니면 제거.
3. **진입 셀** `FindEntryCell(SpawnLocation, Radius)`: `WorldToCell(위치)`가 `CanPawnEnter`면 그 셀.
   아니면 `FindNearestWalkableCell`과 같은 링 확장을 돌되, **후보가 처음 나온 반경에서 월드 거리가
   가장 가깝고 `Waypoints[0]`까지 경로가 있는 셀**을 고른다. (기존 함수는 스캔 순서상 첫 히트를
   돌려줘 (69,62)에서 선로 쪽 (68,61)을 집을 수 있고, 경로가 없는 섬에 들어갈 수도 있다.)
   실패 → Warning, `Destroy`.
4. 전체 경로 = `[EntryCell] + leg(Entry→W0) + leg(W0→W1) + …`. 구간 하나라도 `FindPath` 실패 →
   `OwnerSpawner->ReportRouteFailure(...)` 후 `Destroy`. 중복 경유 셀·진입 셀 == W0은 빈 구간이라
   그냥 건너뛴다.
5. **위치는 스냅하지 않는다.** `Path[0] = EntryCell`이므로 일반 스텝 루프가 스포너 위치에서 진입 셀
   중심(`CellToWorld + HeightAboveFloor`)까지 3D 직선으로 걸어 들어간다. 이것이 요청 3의 "자동 이동".

**Tick** (`GridPawn.cpp:268-322` 복제 + 차이점)
- `Path` 비면 → 목적지 도착 → `Destroy`.
- `NextCell = Path[0]`; 매 프레임 `CanPawnEnter(NextCell, this)` 재검사. 거부되면(통과 모드에선
  Conditional 규칙 변화뿐, 비통과 모드에선 점유 포함) `CurrentCell`이 있으면 거기로 스냅하고
  `BlockedRetrySeconds`마다 `CurrentCell → Waypoints[NextWaypointIndex..]`로 재계획(첫 회 Warning 한 번).
- `VInterpConstantTo(Old, Target, Dt, MoveSpeed)`, 수평 델타로 yaw 설정, 0.5 cm 도착 →
  `CurrentCell = NextCell; Path.RemoveAt(0)`; `CurrentCell == Waypoints[NextWaypointIndex]`면 인덱스 전진.
- `UpdateGhost()`: 플레이어 = `GetFirstPlayerController()->GetPawn()`을 `AGridPawn`으로 캐스트(약참조
  캐시). `{CurrentCell, NextCell} ∩ {플레이어 CurrentCell, GetNextCell()}`가 비어 있지 않으면 겹침.
  **상태가 바뀔 때만** 세 메시 머티리얼을 `GhostMID` ↔ `NormalMaterials`로 교체. `GhostMaterial`이
  없으면 불투명 유지 + Warning 한 번(숨기면 NPC가 사라져 더 나쁘다).
- `LTTSGridDebug::ShouldDrawWorld()`면 `DrawDebugLine`(수명 0)으로 남은 경로.

**EndPlay**: 그리드 관련 정리 없음(점유하지 않으므로). 스포너를 되부르지 않는다.

### `NPC/GridNPCSpawner.h/.cpp` — `AGridNPCSpawner : AActor`

레버(`APuzzleLever`)와 같은 "에디터 배치 액터" 꼴이되, **셀 스냅을 하지 않는다**(그리드 밖 배치가
의도된 사용법). `bIsEditorOnlyActor`가 아니다(BeginPlay가 필요).

- **프로퍼티**: `TSubclassOf<AGridNPC> NPCClass`(기본 `AGridNPC`), `TArray<FIntPoint> Waypoints`,
  `SpawnInterval = 4`, `FirstSpawnDelay = 1`, `MaxAlive = 3`, `bEnabled = true`,
  `EntrySearchRadius = 8`, `NPCMoveSpeed = 300`, `TArray<FName> NPCTags`.
- **컴포넌트**: `SceneRoot` + 에디터용 표식 메시(엔진 Cone, `MI_GreyBox_Movable`, `NoCollision`,
  `bHiddenInGame = true`). 생성자에서 `Tags.Add(LTTSGrid::GenerationIgnoreTag())`,
  `#if WITH_EDITORONLY_DATA bIsSpatiallyLoaded = false`.
- **BeginPlay**: `FindGrid`; 경유 셀 검증(`IsValidCell` && `IsCellWalkableStatic`, 잘못된 셀마다
  Warning, 하나라도 잘못되거나 목록이 비면 `bEnabled = false`); 요약 로그; 켜져 있으면
  `SetTimer(SpawnTimer, this, &OnSpawnTimer, SpawnInterval, true, FirstSpawnDelay)`.
  타이머는 BeginPlay에서만 건다(`OnConstruction`은 에디터 월드에서도 돈다).
- **`OnSpawnTimer`**: `Alive`(`TArray<TWeakObjectPtr<AGridNPC>>`)에서 무효 항목 제거 →
  `Alive.Num() >= MaxAlive`면 건너뜀 → `SpawnOne()`.
- **`SpawnOne(bool bIgnoreCap = false)`**: `SpawnActorDeferred<AGridNPC>(NPCClass, 액터 트랜스폼(회전 0),
  this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn)` — `APawn` 기본값은
  `AdjustIfPossibleButDontSpawnIfColliding`이라 명시해야 한다. `MoveSpeed`, `Tags += NPCTags`,
  `Initialize(...)`, `FinishSpawning`, `Alive`에 추가, 로그.
- **`ReportRouteFailure(const AGridNPC&, FIntPoint From, FIntPoint To)`**: Error 한 번, 타이머 해제,
  `bEnabled = false`. 매 간격마다 스폰-소멸을 반복하지 않기 위해.
- **EndPlay**: 타이머 해제. 살아 있는 NPC 파괴는 `EndPlayReason == Destroyed`일 때만(스포너만 지워진
  경우). PIE 종료(`EndPlayInEditor`)에서는 액터가 `Destroy`되지 않고 `OnDestroyed`도 안 오므로
  **`OnDestroyed`에 의존하지 않고** 약참조 정리로만 센다.
- **콘솔** `ltts.SpawnNPC [이름 부분 문자열]`: 일치하는 스포너에 `SpawnOne(true)`(상한 무시). 없으면 Warning.

### 그리드 변경 (최소)

- `Grid/GridTypes.h` `LTTSGrid` 네임스페이스에 `inline const FName& PassThroughOccupantsTag()` →
  `"GridPassThrough"`. 주석에 "통과는 질의가 아니라 이동체의 속성이므로 태그" 이유를 적는다.
- `Grid/GridActor.cpp`: 익명 네임스페이스 `PawnPassesThroughOccupants(const APawn*)`;
  `:150` → `if (!PawnPassesThroughOccupants(Pawn) && IsCellOccupied(Cell))`, `:193`(`DescribeCell`)도 동일.
  `GridActor.h:214-222, :239` 주석 갱신.

### 기타

- `Player/GridHUD.cpp:106` 뒤에 `NPCs alive %d`(`TActorIterator<AGridNPC>`, 디버그 HUD일 때만).
  의존 방향 Player→NPC, NPC→Grid/Player. **NPC는 Puzzle을 모른다.**
- `Player/GridTestGameMode.cpp:34` `HandleStageClear`에 `if (!Cast<AGridPawn>(Pawn)) return;`.
- `Config/DefaultGame.ini`: 기존 BasicShapes 항목 옆에
  `+DirectoriesToAlwaysCook=(Path="/Engine/EngineDebugMaterials")` (네이티브 생성자 참조는 쿠커가 못 찾는다).

## 3. 핵심 규칙 요약

### 3.1 진입 (그리드 밖 스폰)
스포너 위치 → `WorldToCell` → 들어갈 수 있으면 그 셀, 아니면 링 확장으로 **첫 후보 반경 중 월드 최근접
+ W0까지 경로 있는 셀**. 반경 `EntrySearchRadius` 안에 없으면 소멸 + Warning. 진입은 걸어서.

### 3.2 경로
`Entry → W0 → W1 → … → Wn`을 스폰 시점에 한 번에 계산해 하나의 `Path`로 잇는다. 통과 모드라
점유는 무시되므로 실패는 정적 저작 오류 → 스포너 정지. 비통과 모드(`bPassThroughOccupants = false`)
에서는 매 프레임 다음 셀 재검사 후 대기·재계획.

### 3.3 겹침 반투명
셀 집합 교집합 기준(현재 셀 + 진입 중인 셀). 두 이동체가 마주 걸어올 때 셀이 겹치기 전에 시각적으로
겹치는 구간까지 덮는다. 툰 아웃라인 포스트프로세스는 반투명 머티리얼에 안 먹지만 그레이박스에서는 무방.

## 4. 프로토타입 레이아웃 (B2 동쪽 승강장) — 2026-09-07 에디터에서 실측

그리드 원점 `(-7000, -2400, -700)`, 셀 1 m, 크기 100x138. 에디터 MCP로 트레이스해 승강장 실제
범위를 다시 쟀다. **계획 단계의 "x 70~96"은 틀렸다. 실제 걸을 수 있는 범위는 x 70~94다.**

| 셀 | 실측 |
|---|---|
| x 66~68 | Z -100 (윗층 바닥). 승강장과 5 m 차이라 이어지지 않는다 |
| **x 69** | **NoFloor** (승강장 가장자리 틈, 트레이스는 Z -1000에 맞는다) |
| x 70~94 | Walkable, 바닥 Z -600. y 42~63 |
| x 95 이상 | 트레이스 히트 없음. **바닥이 없다** |
| y 64 이상 | 승강장 아님 |

| 액터 | 위치/설정 |
|---|---|
| `NPCSpawner_1` | 월드 `(-50, 3850, -600)` = 셀 **(69, 62)**, NoFloor. 진입 스텝을 시험한다 |
| `Waypoints` | `(72,62) -> (88,62) -> (93,58) -> (93,44)` — 동쪽 열을 따라 내려가며 퍼즐(x <= 92)을 피한다 |
| 간격/상한 | `SpawnInterval 4`, `FirstSpawnDelay 1`, `MaxAlive 3` |

에디터에서 확인한 것 (`LogDebugReport`):

- `cell (69,62): type=NoFloor reason=NoFloorHit` — 스포너는 그리드 밖에 선다.
- `cell (70,62): type=Walkable ... neighbours=NES-` — 서쪽 이웃이 없다. 즉 x 69가 정말 틈이다.
  진입 셀은 (70,62)가 나와야 한다.
- 경유 셀 (72,62) (88,62) (93,58) (93,44) 전부 `type=Walkable`, 바닥 Z -600.
- `path (70,62) -> (93,44): 41 step(s)` — 경로 전체가 이어져 있다.

## 5. 검증 체크리스트 (PIE, `ltts.GridDebug 2`)

자동(로그)으로 확인
- [ ] 게임 타깃·에디터 타깃 빌드 성공.
- [ ] 스포너 BeginPlay: "4 waypoints valid, timer started", 경고 없음.
- [ ] 첫 NPC: `entering the grid at cell (70,62)` 로그와 경로 길이(약 43 스텝).
- [ ] `NPCs alive`가 3을 넘지 않고 약 4초 간격으로 스폰. 도착 NPC 소멸 후 슬롯이 다시 찬다.
- [ ] "Stage clear cell … reached by GridNPC" 로그가 **없다**.
- [ ] `ltts.SpawnNPC`로 상한을 넘겨 스폰된다.
- [ ] PIE 종료·재시작 두 번 연속에 ensure/에러 없음(타이머·약참조 초기화).
- [ ] 스포너를 둔 채 Generate Grid → 셀 통계 불변(`GridTraceIgnore`).

손으로 확인
- [ ] 행인이 스포너(틈 위)에서 (70,62)로 걸어 들어온 뒤 경로를 따라간다. 코가 진행 방향을 향한다.
- [ ] 플레이어를 NPC 경로 위에 세워 두면 NPC가 통과하며 겹치는 동안 반투명, 지나가면 불투명.
- [ ] NPC 위를 클릭해도 뒤 바닥으로 플레이어가 이동한다(NoCollision).
- [ ] 경유 셀을 퍼즐 블록 위로 잡아도 NPC가 통과하고, 블록은 여전히 밀린다.
- [ ] 디버그 경로 선이 NPC마다 그려지고 플레이어 경로 선은 지워지지 않는다.
- [ ] `bPassThroughOccupants = false`로 바꾼 NPC는 블록 앞에서 멈췄다가 블록이 비키면 다시 간다.

## 6. 남은 가정

| # | 가정 | 왜 문제인가 |
|---|---|---|
| 1 | 회전 장애물·회전 타일은 NPC를 밀어내지 않는다 | 통과 모드에선 벽 안에 잠깐 있다 걸어 나온다. 비통과 모드로 쓰려면 `GetPawnReservedCells`에 NPC를 포함시켜야 한다 |
| 2 | 진입은 직선 보행 | 스포너와 진입 셀 사이 벽을 뚫고 지나갈 수 있다. 스포너는 가장자리 근처에 두는 것이 전제 |
| 3 | 경로는 스폰 시 한 번 계산 | 통과 모드에선 문제 없음. Conditional 규칙이 도중에 바뀌면 3.2의 재계획이 처리 |
| 4 | 경유 셀은 정수 좌표 직접 입력 | 웨이포인트 액터·스플라인 저작이 편할 수 있다. 셀 배열이 기획에 불편하면 후속 |
| 5 | 반투명은 엔진 디버그 머티리얼 | 아트 단계에서 툰 마스터에 Opacity 핀을 단 전용 머티리얼로 교체(`GhostMaterial` 프로퍼티만 바꾸면 됨) |

## 7. 작업 순서

에디터는 먼저 **닫는다**(Live Coding으로 클래스를 추가하면 PIE가 죽은 전례, `GreyBoxTest_1.md:297`).
빌드 명령: `"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" <타깃> Win64 Development -project="C:\Users\User\Desktop\Lets-take-the-subway\LetsTakeTheSubway.uproject" -waitmutex`.
커밋·푸시는 사용자 지시가 있을 때만, `PROG_1`에.

1. 이 문서를 `Docs/Plans/GridNPC.md`로 저장.
2. **그리드 태그** — `GridTypes.h` 태그 함수, `GridActor.cpp:150, :193` 조건 추가, 헤더 주석.
   → 게임 타깃 빌드. 기존 호출자 무변경 확인.
3. **`NPC/GridNPC`** — 클래스·비주얼·진입 셀·경로 결합·스텝 루프·고스트·디버그 선. → 게임 타깃 빌드.
4. **`NPC/GridNPCSpawner`** — 프로퍼티·타이머·지연 스폰·상한·경로 실패 정지·콘솔 명령. → 게임 타깃 빌드.
5. **HUD 줄, GameMode 가드, `DefaultGame.ini` 쿡 항목.**
6. **에디터 타깃 빌드**(`LetsTakeTheSubwayEditor`) + 게임 타깃 한 번 더.
7. **배치**(4절) — 에디터 켜고 MCP 또는 수작업, Save Current Level.
8. **체크리스트**(5절) — 자동 항목은 PIE 로그로, 손 항목은 사용자가.
9. `Docs/Plans/GridNPC.md`에 "구현 결과" 절을 붙여 계획과 달라진 점 기록.

---

## 8. 구현 결과 (2026-09-07)

7절의 1~6단계를 구현했다. **7~8단계(레벨 배치와 PIE 체크리스트)는 남아 있다.** 에디터가 실행
중이라 Development 에디터 타깃을 빌드할 수 없고, 실행 중인 에디터에는 새 클래스가 로드돼 있지
않아 액터를 놓을 수 없다. 9절 참고.

### 8.1 만들거나 고친 파일

| 파일 | 내용 |
|---|---|
| `NPC/GridNPC.h/.cpp` | **신규.** 행인 폰. 진입 셀 탐색, 경로 결합, 한 칸 스텝 루프, 고스트 전환, 디버그 선 |
| `NPC/GridNPCSpawner.h/.cpp` | **신규.** 타이머 스폰, 동시 수 상한, 경유 셀 검증, 경로 실패 시 정지, `ltts.SpawnNPC` |
| `Grid/GridTypes.h` | `LTTSGrid::PassThroughOccupantsTag()` 추가 |
| `Grid/GridActor.cpp` | `CanPawnEnter`·`DescribeCell`의 점유 검사를 통과 태그로 건너뛰게 함 |
| `Grid/GridActor.h` | 점유 레이어 주석에 통과 태그 설명 추가 |
| `Player/GridHUD.cpp` | `NPCs alive` 줄 |
| `Player/GridTestGameMode.cpp` | `HandleStageClear`에 플레이어 폰 가드 |
| `Config/DefaultGame.ini` | `/Engine/EngineDebugMaterials` 쿡 항목 |

### 8.2 계획과 달라진 점

| 항목 | 계획 | 실제 | 이유 |
|---|---|---|---|
| `HeightAboveFloor` | 60 | **0** | 액터 원점을 셀 바닥에 두고 메시를 위로 쌓는 편이 계단에서 발이 뜨지 않는다. 엔진 기본 도형은 원점이 중심이라 오프셋은 메시 쪽에서 준다 |
| 웨이포인트 | `(95,58) -> (95,44)` | **`(93,58) -> (93,44)`** | 실측 결과 x 95는 바닥이 없다. 4절 |
| 스포너 Z | -540 | **-600** | 승강장 바닥 높이. 행인이 수평으로 걸어 들어온다 |
| 몸통 치수 | 프로퍼티 없음 | `BodyHeight` 120 · `BodyDiameter` 60 | 그레이박스 크기를 배치하며 맞출 수 있게 열어 뒀다 |
| 막힘 대기 | 재계획만 | 그리드 밖이면 **진입 셀부터 다시** 찾는다 | 노렸던 진입 셀이 블록에 막히는 경우가 있다 |

### 8.3 구현하며 정한 세부

- **경로는 스폰 시 한 번에 다 잇는다.** `[진입 셀] + leg(진입->W0) + leg(W0->W1) + ...`. 진입 셀을
  경로의 첫 원소로 넣고 **위치는 스냅하지 않는 것**이 요청 3의 핵심이다. 평소의 스텝 루프가
  그대로 "스폰 지점에서 그리드까지 걸어 들어가기"가 된다.
- **진입 셀은 `FindNearestWalkableCell`을 쓰지 않는다.** 그 함수는 링 스캔에서 처음 걸린 셀을
  돌려주므로 (69,62)에서 선로 쪽을 집을 수 있고, 경로가 없는 섬에 들어갈 수도 있다. 대신 후보가
  처음 나온 반경 안에서 **월드 거리가 가장 가깝고 첫 경유 셀까지 길이 있는** 셀을 고른다.
- **`CurrentCell`은 `TOptional`이다.** 그리드에 올라서기 전에는 값이 없다. 가짜 셀로 겹침 판정이나
  재계획을 하지 않기 위해서다.
- **행인은 `NotifyPawnEnteredCell`을 부르지 않는다.** StageClear는 플레이어의 사건이다.
  게임 모드에도 `Cast<AGridPawn>` 가드를 넣어 두 겹으로 막았다.
- **콜리전을 완전히 끈다.** 퍼즐 조각처럼 Visibility만 막으면 행인이 커서와 바닥 사이에 끼어
  플레이어가 그 뒤 셀을 클릭할 수 없다.
- **고스트는 상태가 바뀔 때만 머티리얼을 갈아 끼운다.** 매 프레임 `SetMaterial`을 부르지 않는다.
- **디버그 선은 `DrawDebugLine`으로 직접 그린다.** `FGridRuntimeDebugDrawer::DrawPath`는 영구 라인
  배처의 배치 ID가 하나뿐이라, 행인이 같이 쓰면 플레이어 경로 선을 지운다.
- **PIE 종료 시 스포너는 행인을 거두지 않는다.** 그때 액터는 `Destroy`되지 않고 `EndPlay`만
  받으므로 `OnDestroyed`도 오지 않는다. `EndPlayReason == Destroyed`, 즉 스포너만 지워진 경우에만
  거둔다. 살아 있는 수는 약참조를 매번 걷어 내며 센다.

### 8.4 자동으로 확인한 것

| 항목 | 결과 |
|---|---|
| 게임 타깃 빌드 (`LetsTakeTheSubway Win64 Development`) | **성공** |
| 에디터 타깃 빌드 (`LetsTakeTheSubwayEditor Win64 DebugGame`) | **성공.** `WITH_EDITOR` 코드까지 컴파일됨 |
| 에디터 타깃 빌드 (`... Win64 Development`) | **불가.** 에디터가 실행 중이라 Live Coding이 막는다 |
| 프로토타입 좌표 | 에디터 MCP 트레이스와 `LogDebugReport`로 4절대로 실측. 경로 41스텝 확인 |

컴파일에서 걸린 것 둘. `TSubclassOf`와 `UClass*`를 삼항 연산자로 섞을 수 없어 `if`로 풀었고,
`APawn::Controller` 멤버와 이름이 겹치는 지역 변수 `Controller`가 C4458 경고를 에러로 올렸다.

## 9. 배치와 PIE 검증 (2026-09-07)

사용자가 에디터를 닫은 뒤 `LetsTakeTheSubwayEditor Win64 Development`를 빌드하고 에디터를 다시
켜서 4절대로 배치했다. 배치와 검증은 에디터 내장 MCP 서버에 HTTP JSON-RPC로 붙어 수행했다.

### 9.1 배치한 것

| 항목 | 값 |
|---|---|
| 액터 | `NPCSpawner_1` (`AGridNPCSpawner`) |
| 위치 | `(-50, 3850, -600)` = 셀 (69,62), NoFloor |
| Waypoints | (72,62) (88,62) (93,58) (93,44) |
| 간격 · 상한 | 4.0초 · 3명 |
| 외부 액터 파일 | `Content/__ExternalActors__/Maps/GreyBoxTest_1/B/O1/BCMG1C2058LRVHNO99VQTF.uasset` |

`save_actor`는 예상대로 "Asset does not exist"로 실패했다. 툴바의 Save Current Level(Slate ref
`b1`)을 눌러야 새 외부 액터 패키지가 만들어진다.

### 9.2 PIE 로그로 확인한 것

```
GridNPCSpawner_...: at cell (69,62), 4 waypoints valid, every 4.0 s, up to 3 alive; timer started.
GridNPC_0: entering the grid at cell (70,62); 4 waypoint(s), 42 step(s).
GridNPC_0: reached its destination; despawning.
```

| 항목 | 결과 |
|---|---|
| 진입 셀 | **(70,62)** — 계획대로. 그리드 밖 (69,62)에서 걸어 들어왔다 |
| 경로 길이 | 42 스텝 (진입 1 + 경유 41) |
| 스폰 간격 | 03.985 → 07.986 → 11.986. **정확히 4초** |
| 동시 수 | 3에서 멈췄다가, 첫 행인이 도착해 사라진 뒤 다음이 태어났다 |
| 소멸 | 마지막 경유 셀에서 `reached its destination; despawning` |
| HUD | `NPCs alive 3` |
| StageClear | 행인이 밟아도 이벤트가 뜨지 않았다 |
| 경고·오류 | 없음 |

뷰포트 캡처로 행인 세 명이 승강장 위를 걸어가는 것, 몸통이 바닥에 정확히 놓이는 것,
플레이어 공과 겹치지 않는 것을 확인했다.

### 9.3 배치 중 걸린 것

`add_to_scene_from_class`를 한 번 불렀는데 **빈 스포너가 하나 더 생겼다.** 라벨
`GridNPCSpawner`, 위치 (140,650,100), 경유 셀 없음. 첫 PIE에서 "no waypoints; the spawner is
disabled" 경고로 드러났다. `remove_from_scene`으로 지우고 다시 저장했으며, 두 번째 PIE에서는
스포너 로그가 하나만 나온다. 다음에 MCP로 액터를 만들 때는 **생성 직후 같은 클래스의 액터 수를
세어 볼 것.**

## 10. 남은 작업

코드와 배치와 자동 검증은 끝났다. 남은 것은 **손으로 봐야 하는 항목**뿐이다. 5절의
"손으로 확인할 것" 중 아래 넷이다.

1. 플레이어를 행인 경로에 세워 두고 겹치는 동안 반투명해지는지.
2. 행인 위를 클릭해도 뒤쪽 바닥으로 플레이어가 이동하는지.
3. `ltts.GridDebug 2`에서 행인 경로 선과 플레이어 경로 선이 서로를 지우지 않는지.
4. `bPassThroughOccupants`를 끈 행인이 블록 앞에서 기다렸다 다시 가는지.

사용법과 동작 흐름은 `Docs/GridNPC_Usage.html`, `Docs/GridNPC_Flow.html`에 정리했다.
