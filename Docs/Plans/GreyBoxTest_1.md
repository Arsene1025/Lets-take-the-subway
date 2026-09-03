# GreyBoxTest_1 계획 — GreyBoxMap001.blend 임포트 + 그리드 이동 적용

작성일 2026-09-03. 대상 파일 `C:\Users\User\Downloads\GreyBoxMap001.blend` (Blender 4.5에서 저장, 1.6 MB).
목표: 원본 그레이박스를 형태·배치 그대로 보존한 채 빈 레벨 **GreyBoxTest_1**에 올리고, 기존 그리드 이동 시스템(`AGridActor` + `AGridPawn`)이 그 위에서 동작하게 만든다. 기본 라이팅을 넣고, 플레이어 Pawn은 실린더 대신 **공**으로 바꾼다.

---

## 0. 한 장 요약

| 항목 | 결정 |
|---|---|
| 익스포트 | Blender 헤드리스 스크립트로 **FBX** 1개 (모디파이어 적용, 메시만, 오브젝트 이름 유지) |
| 임포트 | UE 5.8 Interchange **Import Into Level** → 메시는 `Content/Art/GreyBox/GreyBoxMap001/`, 액터는 레벨에 원래 위치로 |
| 레벨 | `Content/Maps/GreyBoxTest_1` — **Empty Open World** 템플릿 (World Partition + OFPA, README 규칙), 스트리밍 끔 |
| 그리드 | 1차: `AGridActor` 1개, 원점 `(-7000, -2400, -700)`, 100 × 138 셀, `RegionHeight 2100`, `MaxStepHeight 100`, `MaxSlopeAngle 45` |
| Pawn | `AGridPawn`을 반지름 50 cm 구로 변경 (`/Engine/BasicShapes/Sphere`), `HeightAboveFloor = 50`, 굴림 회전 옵션 |
| 라이팅 | Directional(Movable) + SkyLight(Real Time Capture) + SkyAtmosphere + ExponentialHeightFog + PostProcessVolume(노출 고정). 프로젝트가 이미 Lumen/VSM/동적 라이팅 전용 |
| 후속 | 층이 겹치는 구간(F0↔B1, B1_002↔B2_002)은 **층별 그리드 + 링크 셀** 확장으로 해결 (2차) |

---

## 1. Blender 파일 분석 결과

### 1.1 단위와 좌표

- 씬 단위: **Metric, 1 unit = 1 m**. FBX로 내보내면 UE에서 1 m = 100 cm로 그대로 들어온다.
- 축: Blender(오른손) → UE(왼손) 변환으로 **Y가 반전**된다. Blender `(x, y, z)` → UE `(100x, -100y, 100z)` cm.
- 이름 규칙의 숫자는 **2 m 단위**다. 예: `Floor - 10*12` = 20 × 24 m, `TicketGate ... 0.5*1*1` = 1 × 2 × 2 m, Player 구 지름 2 m.
- 전체 바운딩 박스: X −70 ~ 30 m, Y −114 ~ 24 m, Z −19 ~ 15 m (100 × 138 × 34 m).
  → UE 기준 X −7000 ~ 3000, **Y −2400 ~ 11400**, Z −1900 ~ 1500.

### 1.2 컬렉션 구성 (오브젝트 36개)

| 컬렉션 | 오브젝트 | 임포트 처리 |
|---|---|---|
| Stage001/F0 | Floor_F0_001~003 (z 12~13 m), Floor_F0_005 (z 0~8 m, 계단 위 단) | 메시로 임포트 |
| Stage001/B1 | Floor_B1_001~003 (상면 z 1), Stiar_B1_001/002, TicketGate_B1_001~008, Store, Elevator, Wall_Left_001/002, Wall_Right_001~003 | 메시로 임포트 |
| Stage001/B2 | Floor_B2_001/002 (승강장 z −6, 선로 z −10), Escalator (경사로 −6 → 1), Trail_B2 (레일) | 메시로 임포트 |
| Stage001/Movable | **Player** (구, F0 위 (3, 0, 14)), **Train** (선로 안, Bevel 모디파이어) | Player는 임포트 제외 → PlayerStart 위치로만 사용. Train은 임포트하되 그리드 트레이스에서 제외 |
| Stage000 | Floor 10*12 (z 0~2), Escalator_Step, Escalator_Ceiling (Solidify), **Player.001** | 테스트 스테이지. 별도 폴더로 임포트, Player.001 제외 |
| Misc. | Camera, Light | 제외 (카메라 위치 (7.36, −6.93, 4.96)은 참고용) |
| Empty_Objects, Bool_Objects, Export | 비어 있음 | 없음 |

머티리얼은 없다(`Dots Stroke`는 그리스펜슬 기본값). UE에서 그레이박스 머티리얼을 따로 붙인다.

### 1.3 층 구조와 겹침 (그리드에 직접 영향)

바닥 상면 높이: **B2 −6 m / B1 +1 m / Stage000 +2 m / F0_005 +8 m / F0 +13 m**.

현재 `AGridActor`는 셀마다 **FloorZ 하나**만 가진다 (위에서 아래로 한 번 트레이스, 첫 히트가 바닥). 따라서 XY가 겹치는 층은 위층만 보인다.

| 겹침 | 범위 (Blender) | 잃는 셀 | 판단 |
|---|---|---|---|
| F0 위 → B1 | x 5~25, y −5~−1 | 약 20셀 (나머지는 Store/Elevator/벽이 차지) | 1차에서 무시 가능 |
| F0 위 → Stage000 | x 0~20, y 0~5 | 약 100셀 | 테스트 스테이지라 1차에서 무시 |
| B1_002 위 → B2_002 승강장 | x −35~−20, y −64~−41 | **약 345셀** (승강장 북쪽 절반) | 2차 층별 그리드에서 해결 |

> **정정 (구현 중 확인):** Stiar_B1_001은 F0가 아니라 **B2(−6) ↔ B1(+1)** 을 잇는다. F0(+13)로 올라가는 계단·경사로는 없고 Elevator(상단 z 9)도 F0에 닿지 않는다. 자세한 연결 구조는 7.5절 참고.

### 1.4 경사·계단·틈새 치수 (그리드 설정 근거)

| 요소 | 실측 | 기본 설정과 충돌 |
|---|---|---|
| Stiar_B1_001 | 챌판 **1 m**, 디딤판 1 m, 7단 (−6 → 1), 이후 10 m 층계참 | `MaxStepHeight 50` 초과 → **100으로 상향** |
| Stiar_B1_002 | 챌판 1 m, 디딤판 1 m, 6단 (1 → 7), 상단 F0_005 상면 8 | 동일 |
| Escalator | 경사로, 10 m 상승 / 11 m 진행 ≈ **42°** | `MaxSlopeAngle 35` 초과 → **45로 상향** (셀당 상승 ≈ 0.9 m < 100) |
| TicketGate 001~004 | 폭 1 m, 간격 **1 m**, y 정수 정렬 | 클리어런스 박스(±40 cm) 통과 가능 |
| TicketGate 005~008 | 간격 1 m이나 **y가 −0.18 m 어긋남** | 틈 셀이 클리어런스에 걸려 **전부 Blocked**. 아트 수정 요청 또는 `ClearanceHalfWidth 30` |
| Stiar_B1_002 | y −45.212 ~ −40.212 (0.212 m 어긋남) | 계단은 X 방향이라 가장자리 셀만 영향 |
| Store / Elevator | 6 × 6 × 6 m, 6 × 6 × 8 m **속이 찬 박스** | 지붕이 바닥으로 잡히지만 6 m 단차라 고립. Box Marker로 Blocked 처리 |
| Train | 선로 안, 지붕 z −1 | 이동체. 그리드 생성 시 제외 |

핵심: 이 맵의 설계 언어는 **1 m 셀, 1 m 챌판, 1 m 틈**이다. 그리드 원점을 **정수 m에 정렬**해야 계단 디딤판 중심과 개찰구 틈 중심에 트레이스가 떨어진다.

### 1.5 코드 제약 (현재 상태)

- `AGridActor::FindGrid`는 월드의 **첫 번째** 그리드만 반환 → 1차는 그리드 1개.
- `CellSize = 100` 고정, `SizeInCells` 최대 512 → 100 × 138은 문제 없음.
- 커서 클릭은 `ECC_Visibility` 라인 트레이스 히트 지점 → `WorldToCell`. 여러 층 높이가 한 그리드에 있어도 동작.
- Pawn은 충돌 없음(`NoCollision`). 공 지름이 틈보다 커도 물리적으로 막히지 않는다.
- `DirectoriesToAlwaysCook`에 `/Engine/BasicShapes`가 이미 있어 Sphere도 쿠킹된다.

---

## 2. 결정 사항 (권장안과 대안)

| # | 결정 | 권장 | 대안 / 비고 |
|---|---|---|---|
| D1 | 교환 포맷 | **FBX** (Interchange, 콜리전·씬 임포트 성숙) | glTF도 가능. USD는 과함 |
| D2 | 임포트 방식 | **Import Into Level** (액터 배치·이름 자동) | 메시만 임포트 후 수동 배치 — 32개라 가능하지만 오류 여지 |
| D3 | 레벨 템플릿 | **Empty Open World** (WP + OFPA, README 2절) + World Settings에서 **Enable Streaming 끔** | `MovementTestMap`은 WP가 아님. 팀 규칙을 따를지 확인 필요 |
| D4 | 그리드 구성 | 1차 **단일 그리드** (원점 z −700, 높이 2100으로 B2~F0 전부 커버) | 2차: 층별 `AGridActor` 3개 + 링크 셀. 6절 참고 |
| D5 | 공 크기 | **반지름 50 cm** (1셀 = 1 m 안에 들어감) | 원본 구는 지름 2 m. 아트 의도가 2 m면 `BodyMesh` 스케일만 2로 |
| D6 | Player 구 | 임포트 제외, **PlayerStart**로 대체 | 위치 F0 (300, 0, 1350) |
| D7 | Stage000 | 임포트하되 액터 폴더 `GreyBox/Stage000`으로 분리 | 필요 시 Data Layer로 숨김 |
| D8 | 기본 맵 | `EditorStartupMap`만 GreyBoxTest_1로 변경, `GameDefaultMap`은 유지 | 둘 다 바꿀지 확인 |
| D9 | 원본 보관 | `RawContent/GreyBox/GreyBoxMap001.blend` + `.fbx` (둘 다 LFS 규칙 있음) | `Content/` 밖이라 UE가 건드리지 않음 |

---

## 3. 단계별 작업

### Phase 0 — 준비 (프로그래머)

1. 브랜치 `feature/greybox-test-1` 생성.
2. `RawContent/GreyBox/` 만들고 `GreyBoxMap001.blend` 복사. `.gitattributes`에 `*.blend`, `*.fbx` LFS 규칙이 이미 있다.
3. `Tools/Blender/` 폴더 생성 (익스포트 스크립트 위치).

### Phase 1 — Blender 익스포트 (자동)

스크립트 `Tools/Blender/export_greybox.py`, 실행:

```bash
"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe" -b RawContent\GreyBox\GreyBoxMap001.blend --python Tools\Blender\export_greybox.py
```

스크립트가 할 일:

- 대상: `MESH` 타입 전부에서 **Player, Player.001 제외**. Camera/Light/Empty는 타입 필터로 빠짐.
- **모디파이어 적용** (Escalator_Ceiling Solidify, Train Bevel, Floor_B2_002 Geometry Nodes).
- `bpy.ops.export_scene.fbx(use_selection=True, object_types={'MESH'}, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE', use_mesh_modifiers=True, bake_space_transform=False)`.
- 오브젝트 이름은 그대로 두되, 이름의 `*`, ` - `, `.` 는 UE 에셋 이름에 부적합하므로 스크립트에서 `Floor_10x12`, `Elevator_3x3x4` 식으로 정리한다 (원본 .blend는 수정하지 않음).
- 출력: `RawContent/GreyBox/GreyBoxMap001.fbx`.
- 검증: 로그에 오브젝트 수 **32개**, 바운딩 박스 100 × 138 × 34 m가 찍히는지 확인.

### Phase 2 — UE 임포트 (에디터)

1. 콘텐츠 브라우저 `Content/Art/GreyBox/GreyBoxMap001/` 생성 (아트 소유 폴더, README 규칙).
2. **File → Import Into Level…** 로 FBX 선택. Interchange 옵션:
   - Static Mesh: Combine Meshes **끔**, Import Materials 끔, Import Textures 끔.
   - Collision: 자동 생성 켬. 이후 전체 메시를 선택해 **Collision Complexity = Use Complex Collision As Simple**로 일괄 변경 (Property Matrix). 계단·경사로·L자 바닥이 있어 단순 박스 콜리전으로는 트레이스가 틀어진다. 그레이박스라 삼각형 수가 적어 비용은 무시할 수준.
   - Scene: 액터 계층 유지. Import Into Level은 루트 액터 아래 32개 StaticMeshActor를 원래 트랜스폼으로 놓는다.
3. 스케일 검증: `Floor_10x12` 액터 바운즈가 **2000 × 2400 × 200 cm**, F0 바닥 상면 z **1300**인지 확인. Y 반전 확인: `Floor_B1_001`이 UE Y **+100 ~ +1700**에 있어야 한다.
4. 액터 폴더 정리: `GreyBox/Stage001/F0`, `/B1`, `/B2`, `/Movable`, `GreyBox/Stage000`.
5. 머티리얼: `Content/Art/GreyBox/Materials/M_GreyBox` (단색, Roughness 0.8) + 인스턴스 `MI_GreyBox_F0`, `_B1`, `_B2`, `_Stage000`, `_Movable`을 층별로 색만 다르게. 층 구분이 눈에 보여야 그리드 디버그가 편하다.
6. Train 액터: 콜리전 프리셋을 **Visibility 채널 Ignore**로 바꿔 그리드 트레이스와 클릭 트레이스에서 빠지게 한다 (이동체이므로).

### Phase 3 — 빈 레벨 GreyBoxTest_1

1. File → New Level → **Empty Open World** → `Content/Maps/GreyBoxTest_1` 저장. OFPA로 `Content/__ExternalActors__/Maps/GreyBoxTest_1/`가 생긴다.
2. World Settings: GameMode Override = `GridTestGameMode`, World Partition → **Enable Streaming 끔** (138 m 맵, 그리드 트레이스와 PIE에서 액터가 언로드되면 안 됨).
3. Phase 2를 이 레벨에서 수행 (Import Into Level은 현재 열린 레벨에 배치한다).
4. `PlayerStart` 배치: **(300, 0, 1350)**, F0 바닥 위 (Blender Player 구 위치). Pawn은 BeginPlay에서 셀 바닥 + `HeightAboveFloor`로 스냅한다.
5. `Config/DefaultEngine.ini`: `EditorStartupMap=/Game/Maps/GreyBoxTest_1.GreyBoxTest_1` (D8).

### Phase 4 — 기본 라이팅

프로젝트 설정이 이미 정적 라이팅 끔, Lumen GI/Reflection, VSM이라 전부 **Movable**로 둔다.

| 액터 | 설정 시작값 |
|---|---|
| DirectionalLight | Movable, Intensity 8 lux, Rotation Pitch −60 / Yaw −30 (벽이 12 m라 각도가 낮으면 B1이 통째로 그늘), Atmosphere Sun Light 켬 |
| SkyLight | Movable, **Real Time Capture** 켬, Intensity 1.0 |
| SkyAtmosphere | 기본값 |
| ExponentialHeightFog | Density 0.005 (기본 0.02는 138 m 맵에서 B2가 뿌옇게 됨) |
| PostProcessVolume | **Infinite Extent** 켬, Exposure Min/Max EV100 같은 값으로 고정 (시작 9, PIE에서 보고 조정). 아이소메트릭 카메라가 층을 오갈 때 밝기가 튀지 않게 |

지하층(B1/B2) 천장이 없어 햇빛이 닿는다. 그늘이 너무 짙으면 B1/B2 위에 RectLight 2~3개 추가 (선택).

### Phase 5 — 그리드 배치와 생성

1. `GridActor` 배치: **Location (−7000, −2400, −700)**. 정수 m 정렬 필수 (1.4절).
2. 속성:

| 속성 | 값 | 이유 |
|---|---|---|
| SizeInCells | **(100, 138)** | 바운딩 박스 전체 |
| RegionHeight | **2100** | −700 ~ +1400: B2 승강장(−600)부터 F0(+1300)까지 첫 히트로 잡힘 |
| MaxStepHeight | **100** | 챌판 1 m |
| MaxSlopeAngle | **45** | 에스컬레이터 42° |
| ClearanceHeight / HalfWidth | 180 / 40 | 개찰구 1 m 틈 통과. 005~008 어긋남이 아트에서 안 고쳐지면 30 |
| bDrawCellCoords | 켬 (튜닝 중) | |

3. **Generate Grid** → 로그 통계 확인. 기대: walkable 수천, stepBreaks는 선로 가장자리·Store/Elevator 지붕·벽 위에서만.
4. Box Marker(Blocked): Store 지붕 (6×6), Elevator 지붕 (6×6), Stage000 Escalator_Step 위 등 고립 지붕 — 클릭 시 "No route" 대신 "Blocked by designer"가 뜨게.
5. 개찰구 005~008 틈이 Blocked면 아트에 **y −48.18 → −48.00 스냅** 요청. 임시로는 Cell Marker(Walkable) 4개.
6. `Log Debug Report`로 F0 (3,0) → B2 승강장 남쪽 경로가 나오는지 확인. `DebugPathStart/Goal`을 셀 좌표로: F0 시작점 (73, 24), B2 목표 (40, 100) 부근.

### Phase 6 — Pawn을 공으로 (코드)

`Source/LetsTakeTheSubway/Player/GridPawn.h/.cpp` 수정:

- 루트 `UCapsuleComponent` → `USphereComponent` (반지름 50, 충돌은 그대로 `NoCollision`). 헤더의 `Capsule` 멤버·전방 선언을 `Sphere`로 교체.
- `BodyMesh`: `/Engine/BasicShapes/Sphere.Sphere` (지름 100 cm), 스케일 `(1, 1, 1)`. 머티리얼은 기존 `BasicShapeMaterial` 유지하거나 `MI_GreyBox_Movable` 소프트 참조.
- `HeightAboveFloor` 기본값 **88 → 50** (주석 "Matches the capsule half height"도 갱신).
- 옵션 — 굴림: `Tick`에서 이동 벡터 `Delta`로 `Axis = Up × Delta.Normalized`, `Angle = |Delta| / 50 cm` 만큼 `BodyMesh->AddWorldRotation`. 스프링암은 이미 절대 회전이라 카메라에 영향 없음. 굴림은 `BodyMesh`에만 적용하고 루트는 회전시키지 않는다.
- `MovementTestMap`에서도 같은 Pawn을 쓰므로 그쪽도 공이 된다. 의도된 변경.

빌드:

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" LetsTakeTheSubwayEditor Win64 Development -project="C:\Users\User\Desktop\Lets-take-the-subway\LetsTakeTheSubway.uproject" -waitmutex
```

### Phase 7 — 검증 체크리스트 (PIE)

- [ ] F0 시작 셀에 공이 바닥 위 50 cm에 놓인다 (로그 `standing on cell`).
- [ ] F0 → Stiar_B1_001 → B1 클릭 이동. 계단 7단을 끊김 없이 내려간다.
- [ ] 개찰구 001~004 틈(1 m) 통과, 개찰구 자체는 Blocked.
- [ ] Stiar_B1_002 → F0_005 (8 m 단) 올라간다.
- [ ] 에스컬레이터 경사로로 B2 승강장 남쪽 도달.
- [ ] Store/Elevator 지붕, Train 지붕, 선로(−10 m) 클릭 시 거부 메시지가 뜬다.
- [ ] 층 이동 시 노출이 튀지 않는다. B1/B2 바닥이 식별 가능한 밝기.
- [ ] `MovementTestMap`이 여전히 동작한다 (회귀).
- [ ] 저장 후 재시작해도 그리드 데이터가 유지된다 (`bRegenerateOnPlay = false`).

### Phase 8 — 커밋 단위

1. `Tools/Blender/export_greybox.py` + `RawContent/GreyBox/*` (LFS).
2. `Content/Art/GreyBox/**` 메시·머티리얼.
3. `Content/Maps/GreyBoxTest_1.umap` + `__ExternalActors__` + `DefaultEngine.ini`.
4. `GridPawn` 공 변경 (코드만, 별도 커밋).
5. 그리드 마커·튜닝값 (레벨 액터 파일).

---

## 4. 생성·수정 파일 목록

| 경로 | 종류 | 소유 |
|---|---|---|
| `RawContent/GreyBox/GreyBoxMap001.blend`, `.fbx` | 신규 (LFS) | 아트 |
| `Tools/Blender/export_greybox.py` | 신규 | 프로그래머 |
| `Content/Art/GreyBox/GreyBoxMap001/SM_*` (32개) | 신규 | 아트 |
| `Content/Art/GreyBox/Materials/M_GreyBox`, `MI_GreyBox_*` | 신규 | 아트 |
| `Content/Maps/GreyBoxTest_1.umap` + `Content/__ExternalActors__/Maps/GreyBoxTest_1/**` | 신규 | 공용 |
| `Config/DefaultEngine.ini` | 수정 (EditorStartupMap) | 프로그래머 |
| `Source/.../Player/GridPawn.h`, `GridPawn.cpp` | 수정 (구 Pawn) | 프로그래머 |
| `Docs/Plans/GreyBoxTest_1.md` | 이 문서 | |

---

## 5. 리스크와 열린 질문

1. **층 겹침** — B2 승강장 북쪽 345셀이 1차에서 빠진다. 열차 승차 동선이 그쪽이면 2차(6절)를 앞당겨야 한다.
2. **아트 수정 요청 2건** — TicketGate_B1_005~008 y 오프셋 0.18 m, Stiar_B1_002 y 오프셋 0.212 m. 정수 m 스냅이면 해결.
3. **공 지름** — 원본 2 m vs 셀 1 m. 반지름 50으로 가되 아트가 2 m를 원하면 메시 스케일만 바꾸면 된다.
4. **WP 여부** — README는 WP+OFPA를 요구하지만 기존 테스트 맵은 아니다. 이 레벨부터 규칙을 적용할지 확인.
5. **MCP** — 이 세션에서는 `unreal-mcp`가 연결되지 않았다(에디터 미실행). 에디터를 켜면 Phase 3~5의 액터 배치·속성 설정을 MCP로 자동화할 수 있다. 임포트는 에디터 메뉴로 하는 편이 옵션 제어가 확실하다.
6. **Elevator / Train** — 현재는 정적 박스. 층 이동 수단으로 쓰려면 Conditional 룰 또는 링크 셀이 필요 (2차).

---

## 6. 2차 — 층별 그리드와 링크 셀 (설계 스케치)

1차가 검증되면 겹침 구간을 위해:

- `AGridActor`에 `LayerName`(F0/B1/B2)과 층별 `Origin Z / RegionHeight`를 두고 3개 배치. 각 그리드는 자기 밴드에서만 트레이스 (B2: −700 ~ −100, B1: 0 ~ 600, F0: 700 ~ 1400).
- `AGridPawn::Grid`를 "현재 층"으로 취급. `FindGrid` 대신 PlayerStart 위치의 Z로 시작 층 선택.
- 새 마커 `AGridLinkMarker`: 셀 쌍 (그리드 A의 셀 ↔ 그리드 B의 셀). 계단 상·하단, 에스컬레이터 양끝, 엘리베이터 문에 배치. Pawn이 링크 셀에 도착하면 `Grid`를 교체하고 상대 셀로 이어서 이동.
- 경로 탐색: 층 내 A* 유지, 층 간은 링크를 통한 2단계 탐색 (링크 수가 적어 BFS면 충분).
- 컨트롤러 클릭: 히트 Z로 어느 층 그리드인지 판별 후 `WorldToCell`.

---

## 7. 구현 결과 (2026-09-03)

Phase 0~7을 같은 날 실행했다. 에디터 작업은 열려 있던 Unreal 에디터의 MCP 서버(`http://127.0.0.1:8000/mcp`)에 HTTP로 직접 붙어 수행했다.

### 7.1 계획과 달라진 점

| 항목 | 계획 | 실제 | 이유 |
|---|---|---|---|
| FBX | 오브젝트 전체를 FBX 1개 | **오브젝트당 FBX 1개** (`RawContent/GreyBox/FBX/SM_*.fbx`, 32개) + `GreyBoxMap001_manifest.json` | 회전·스케일을 Blender에서 정점에 구워 노드 트랜스폼을 항등으로 만들면 임포터(레거시 FBX / Interchange)의 "노드 트랜스폼 굽기" 차이가 사라진다. 배치는 매니페스트의 위치만 쓴다 |
| 임포트 | Import Into Level | 메시 임포트(`StaticMeshTools.import_file`) 후 **매니페스트 위치로 액터 스폰** | MCP에 씬 임포트 도구가 없음. 결과는 동일(32개 StaticMeshActor, 원위치) |
| 빈 레벨 | New Level → Empty Open World | `/Engine/Maps/Templates/OpenWorld`를 복제한 뒤 **랜드스케이프·구름·라이트·PlayerStart 등 전부 삭제** | MCP에 새 레벨 도구가 없음. 결과는 WP + OFPA 빈 레벨 (`__ExternalActors__` 43개 파일) |
| WP 스트리밍 | Enable Streaming 끔 | **못 끔** — `bEnableStreaming`이 MCP 오브젝트 도구에 노출되지 않음. 대신 GridActor를 `bIsSpatiallyLoaded=false`로 설정 | PIE 로그에서 32개 패키지가 시작 시 전부 로드됨을 확인. 나중에 World Settings에서 수동으로 끌 것 |
| 머티리얼 | `M_GreyBox` + MI 5개 | `/Engine/BasicShapes/BasicShapeMaterial`의 **MI 5개만** (`Content/Art/GreyBox/Materials/MI_GreyBox_F0/_B1/_B2/_Stage000/_Movable`, Roughness 0.8) | 별도 마스터 머티리얼이 필요 없었음 |
| Train | Visibility 채널 Ignore | 콜리전 프로파일 변경이 도구로 적용되지 않아 **Box Marker(Blocked, 8×23셀)** 로 지붕 셀을 막음 | 클릭 시 "Blocked by designer"로 거부됨 |
| Player 위치 | (300, 0, 1350) | 동일. PIE 로그 `GridPawn_0: standing on cell (73,24)` | |
| 노출 | EV100 9 | **EV100 7** (min = max) | 9는 너무 어두웠음 |

### 7.2 그리드 생성 통계 (최종)

| 항목 | 값 |
|---|---|
| 셀 | 100 × 138 = 13,800 |
| Walkable | 4,567 |
| Blocked | 307 (slope 19, clearance 16, 마커 272) |
| NoFloor | 8,926 |
| StepBreaks | 406 (선로 가장자리·벽 위·지붕 경계) |
| 마커 | Store 지붕 (80,29) 8×2, Elevator 지붕 (88,29) 6×2, Escalator_Ceiling 지붕 (82,8) 6×10, Train 지붕 (61,41) 8×23 |

Store/Elevator 지붕 마커는 처음에 6줄로 잡았다가 **F0 바닥 셀(y 19~28)까지 막는 실수**를 스크린샷에서 발견해 F0 밖 2줄(y 29~30)로 줄였다. 위층이 덮는 구간은 단일 그리드에서 아래층이 아예 보이지 않으므로 마커도 위층 기준으로 잡아야 한다.

### 7.3 코드 변경

`GridPawn.h/.cpp`: 루트 `UCapsuleComponent` → `USphereComponent`(반지름 `BallRadius` 50), 바디 메시 `/Engine/BasicShapes/Sphere`, `HeightAboveFloor` 88 → 50, `bRollWhileMoving`(기본 켬)으로 수평 이동 거리 / 반지름만큼 바디 메시 굴림. Live Coding(`LiveCoding.Compile`)으로 컴파일해 PIE에서 확인했다.

### 7.4 후속 수정 (같은 날 저녁)

| 문제 | 원인 | 조치 |
|---|---|---|
| 에디터에서 그리드 오버레이가 안 보임 | `bDrawCellCoords`를 켜면 라벨 거리(`CoordLabelMaxDistance` 25 m)를 `FDebugRenderSceneProxy::FarClippingDistance`에 넣는데, 이 값은 **셀 쿼드까지** 잘라낸다. 카메라가 25 m 이상 물러나면 그리드 전체가 사라짐 | `FGridLabelDrawHelper`(커스텀 delegate helper)가 라벨만 거리로 걸러내고 프록시는 클리핑하지 않게 변경 |
| 지붕 마커가 엉뚱한 셀을 막음 | `AGridBoxMarker`는 **액터 위치가 사각형 중심**(`GetMinCell` = 위치 − 크기/2). 최소 셀 중심에 놓아 네 마커가 모두 반 칸씩 어긋나 F0 바닥 셀을 막고 열차 지붕은 열림 | 네 마커를 중심 좌표로 재배치. 마커 배치 공식: `원점 + (minCell + size/2) × 100` |
| 서쪽 계단(Stiar_B1_002) 한 단이 끊김 | 챌판 1 m = `MaxStepHeight` 100과 정확히 같아 트레이스 Z의 부동소수점 오차로 한 곳이 `> 100` | `MaxStepHeight` 100 → **110** |
| Live Coding 후 PIE 크래시 | 컴포넌트 클래스 레이아웃 변경(helper 멤버 추가)을 Live Coding으로 반영한 뒤 PIE 월드 복제에서 `Cast … to Actor failed` | 에디터 종료 후 `Build.bat` 정식 빌드로 해결. **멤버를 추가·삭제하는 변경은 Live Coding 대신 정식 빌드** |

최종 통계: walkable 4,504 · blocked 303 (마커 272) · stepBreaks 402.

### 7.5 그레이박스 연결 구조 (Log Debug Report로 검증)

| 경로 | 결과 |
|---|---|
| F0 (73,24) → F0 (90,20) | 21스텝 ✓ |
| B1_001 (85,35) → 계단 B1_001 → B2_001 승강장 (85,50) | 37스텝 ✓ |
| B1_002 (45,80) → 개찰구 005~008 → B1_003 (20,68) | 37스텝 ✓ (틈 셀 일부는 클리어런스로 막히지만 통과 가능한 틈이 있음) |
| B1_003 (20,68) → 계단 B1_002 → F0_005 (5,68) | 15스텝 ✓ (MaxStepHeight 110 이후) |
| B1_002 (45,80) → 에스컬레이터 → B2_002 승강장 (37,100) | 28스텝 ✓ |
| **F0 → B1** | **도달 불가** — F0(+13)로 오르내리는 지오메트리가 없음. Elevator(z 1~9)가 유일한 후보이며 링크 셀(2차)이 필요 |
| **B1_001 ↔ B1_002 / B2_001 ↔ B2_002** | **도달 불가** — 역이 선로를 사이에 둔 **동쪽 절반(F0·B1_001·B2_001)** 과 **서쪽 절반(B1_002/003·F0_005·B2_002)** 으로 나뉘며, 둘을 잇는 것은 **열차**뿐 |

즉 현재 PlayerStart(F0)에서는 F0 위 25 × 10셀만 돌아다닐 수 있다. 이동 시스템 전체를 지금 테스트하려면 PlayerStart를 B1_001(예: 셀 (77,33) = UE (700, 900, 150))로 옮기고, F0 출발은 엘리베이터 링크가 생긴 뒤 되돌리는 편이 낫다. 어느 쪽으로 할지는 결정이 필요하다.

### 7.6 남은 일

1. PlayerStart 위치 결정 (7.5절).
2. World Settings → World Partition → **Enable Streaming 끄기** (수동).
3. 아트 수정 요청: TicketGate_B1_005~008 y −0.18 m, Stiar_B1_002 y −0.212 m 스냅. 반영되면 `export_greybox.py` 재실행 → 해당 SM 재임포트.
4. 2차: 층별 그리드 + 링크 셀 (6절). 엘리베이터(F0↔B1)와 열차(B2_001↔B2_002)가 첫 링크 대상.
5. `MovementTestMap.umap`이 에디터 실행 직후(16:01) 다시 저장되어 작업 트리에 변경으로 남아 있다. 이 작업과 무관하므로 커밋하지 않았다.
