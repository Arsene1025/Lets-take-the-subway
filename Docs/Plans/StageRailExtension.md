# Subway_Stage1·Stage2 노선 연장 대응 계획 — 아트 레이아웃 변경 점검과 그리드·열차 재조정

작성일 2026-09-14. 대상: `Content/Maps/Main/Subway_Stage1.umap`, `Subway_Stage2.umap`.
원인 커밋: `fbc5583` "St1, St2 레일 길이 수정"(ART_2, 15:51) → `6fd3d31` 병합.
직전 기준 커밋: `29d44af`(Stage1은 `24a5585`와 동일 blob, Stage2는 `47c0f89`와 동일 blob).
점검 방법: 에디터(MCP)에서 현재 값 실측 + LFS 이전 blob 바이너리 파싱으로 old/HEAD 액터 트랜스폼 전수 비교.
선행 문서: `Subway_Stage1.md`(Stage1 배치 기준값), `BoardingRefactor.md`, `Docs/StageZone.html`.

---

## 0. 한 장 요약

| 항목 | Stage1 | Stage2 |
|---|---|---|
| 아트가 바꾼 것 | 서쪽 역 전체(127개 액터)를 **+Y 8797.55** 이동, 그 사이에 레일 세그먼트 1개(+8800) 추가 | 입구 역 **(−4740, −9625)**, 서쪽 역 **(−12207, +4473)**, 동서 레일·`Train_Line2` **+Y 4810** 로 각각 이동, 레일 세그먼트 2개 추가 |
| 딸려간 프로그래머 액터 | **`StationGrid`**(Y +8797.55), 구역 볼륨 3·4, `CAM_Isometric3·4`, 서쪽 도크·엘리베이터·회전판·마커·블록 | **`StationGrid`**(−16947, −5152 — 어느 그룹과도 다른 값), `PlayerStart`·`StageInfo`·`StartingPoint001`(입구와 함께), `Train_Line2`(레일과 함께) |
| 결과 | 그리드 Y 범위가 `[6297, 18797]`이 되어 **동쪽 역·PlayerStart가 그리드 밖**. walkable 4271 → 2142 | 그리드 X 범위가 `[−40574, −10774]`가 되어 **입구·중앙 역·PlayerStart가 그리드 밖**. walkable 12830 → 2085 |
| 그리드 재생성 | 아트 세션에서 자동 재생성됨(15:14:31, 잘못된 원점으로) | 동일(15:36:37) |
| 셀 좌표 저작값 | 열차 `Stops` 2개, 도크 `TargetFloorCell` — 값은 그대로지만 원점이 바뀌어 의미가 달라짐 | 열차 `Stops` 4개 — 동일 |
| 기타 | `CAM_Isometric1` 위치·회전 변경, `Train_Line1` 배치 Y 8420 → 4950 | 퍼즐 블록 `Vending_SM_L2_VendingMachine3` 삭제, `GridBoxMarker6·7` Z가 10으로 떨어짐 |
| 열차 문 방향 | 그대로 맞음(동쪽 승강장) | **두 대 모두 승강장 반대편을 봄** → yaw 반전 필요 |
| 승강장–선로 간격 | 새 레일이 역보다 2.45 cm 더 감(무시 가능) | 입구 승강장 340 cm, 서쪽 승강장 337 cm 떨어짐(낮은 턱이 사이에 있음) → **자동 탑승 셀 계산 실패** |

핵심 판단: **단순 롤백은 불가**하다. 역 구조물 자체가 이동했으므로 프로그래머 액터는 새 지오메트리 기준으로 다시 맞춘다.
다만 Stage1 동쪽 역은 1 cm도 안 움직였으므로 **그리드 원점을 원래 값으로 되돌리기만 하면 동쪽 셀 번호가 전부 살아난다.**

---

## 1. 점검 결과 상세

### 1.1 Stage1 — old(`29d44af`) → HEAD

| 액터 | old | HEAD | 비고 |
|---|---|---|---|
| `StationGrid` 위치 | (−11500, −2500, −250) | **(−11500, 6297.55, −250)** | 140×125, RegionHeight 1100, Step 100, Slope 45 — 수치는 동일 |
| 그리드 통계 | walkable 4271 / blocked 6863 / noFloor 6366 / override 447 / stepBreaks 151 | walkable 2142 / blocked 5644 / noFloor 9714 / override 205 / stepBreaks 68 | `LastGenerated` 2026.09.14-15.14.31 |
| `Train_Line1` | (−1250, 8420, −124) yaw −90 | (−1250, 4950, −124) yaw −90 | BeginPlay에서 `Stops[0]` 위치로 스냅하므로 XY는 무의미. `Stops` 값 변화 없음: [0] `Platform_B2_East` (102,62) Exit (110,73)/(114,73), [1] `Tunnel_North` (102,112) |
| 구역 볼륨 `Stage3`/`Stage4` | (−3710, 8470) / (−9120, 8360) | (−3710, 17267.55) / (−9120, 17157.55) | 서쪽 역과 함께 이동 — **조치 불필요** |
| `CAM_Isometric3`/`4` | Y 8763 / 8673 | Y 17560.55 / 17470.55 | 함께 이동 — 조치 불필요 |
| `CAM_Isometric1` | (480, −277, 1020) rot (−35.26, −135, 0) | **(1850.7, 1294.8, 2173.4) rot (−32.26, −136.6, 0)** | 평행이동이 아니라 임의 변경. 의도 확인 필요(2.6) |
| `BP_RailTrack4`, `SM_Floor_Rail2` | 없음 | 신규, 기존 레일의 +Y 8800 | RailCount 22, RailSpacing 400 — 기존과 동일 |
| 서쪽 도크·차체·회전판·마커·블록 | — | 전부 +Y 8797.55 | `PuzzleElevatorDock2` `TargetFloorCell` (62,118)은 셀 값이라 재지정 필요 |
| `PlayerStart`, 동쪽 액터 전부 | — | 변화 없음 | 구역 1 볼륨 안에 있음 |

서쪽 역 이동량 8797.55 cm = 87.98 셀. 액터가 셀 중심(.5)에 있었으므로 새 셀 번호는 **+88**이며, 정확히 셀 경계(.0)에 있던 액터만 87이 될 수 있다(엘리베이터 도크는 풋프린트 nudge 덕에 88).

### 1.2 Stage2 — old(`47c0f89`) → HEAD

| 액터 | old | HEAD | 비고 |
|---|---|---|---|
| `StationGrid` 위치 | (−23627, 4376, 8070) | **(−40574, −776, 8070)** | 298×168, RegionHeight 1350 — 수치 동일 |
| 그리드 통계 | walkable 12830 / blocked 817 / noFloor 36417 / override 207 / stepBreaks 909 | walkable 2085 / blocked 266 / noFloor 47713 / override 207 / stepBreaks 96 | `LastGenerated` 15.36.37 |
| 입구 역(L1 개찰구·PlayerStart·StartingPoint001·StageInfo, 89+11개) | X 4823 부근(남북 레일 **동쪽**) | X 83 부근, Y −4699 (레일 **서쪽**, 남쪽 끝) | 델타 (−4740, −9625) |
| 서쪽 역(135+14개) | X −22000 부근 | X −35600 ~ −31400 | 델타 (−12207, +4473), L1 일부는 (−12247, +4448)로 40/25 cm 어긋남 |
| 동서 레일 `BP_RailTrack2`·`SM_Floor_Rail002`·`Train_Line2` | Y 16570 / 16596 | Y 21380 / 21406 | 델타 +4810. 서쪽 역(+4473)과 **337 cm 차이** |
| 중앙 역(승강장 B·C, L1_002, 블록 40개, StartingPoint002) | — | 변화 없음 | |
| `BP_RailTrack3`(남북 연장), `BP_RailTrack4`(동서 연장) | 없음 | 신규 | 남북 레일 Y −835~20780, 동서 레일 X −35717~−8428 |
| `Vending_SM_L2_VendingMachine3` (`BP_Block_VendingMachine_Small`) | (4123, 10576, 9120) yaw 90 | **삭제** | 입구 역과 함께 옮겼다면 (−617, 951, 9120) |
| `GridBoxMarker6`·`7` | Z 8330.1 / 8080 | **Z 10** | 마커는 XY만 쓰므로 동작엔 무해 |
| `Stops` | 변화 없음 | | Line1 [0] `S2_PlatformA` (256,65), [1] `S2_PlatformB` (256,144) Exit (246,144) / Line2 [0] `S2_PlatformD` (22,122) Exit (22,116), [1] `S2_PlatformC` (130,122) |

현재 Stage2 지오메트리(에디터 실측, L1 상면 8320 / L2 상면 9120):

| 구역 | 범위 | 비고 |
|---|---|---|
| 입구 L1 | X −1967~1233, Y −5049~−849 | 개찰구 Y −3449, PlayerStart (83, −4699), 에스컬레이터 Y −1620 |
| 승강장 A′ (L2, 신규 위치) | X −2007~1233, Y −849~3311 | 남북 레일 바닥(X 1573~2373)과 **340 cm 간격**, 사이에 Z 8764 턱 |
| 남북 레일 | X 1573~2373, Y −849~20976 | `Train_Line1` X 2008 |
| 승강장 B (L2, 불변) | X −2627~1573, Y 17135~20976 | 레일과 맞닿음. StartingPoint002 (−2427, 18776) |
| L1_002 (불변) | X −8427~−2627, Y 17576~20976 | 에스컬레이터 2·3 |
| 승강장 C (L2, 불변) | X −12627~−8427, Y 16976~20976 | 동서 레일 바닥(Y 20986~21786)과 맞닿음 |
| 동서 레일 | X −35717~−8428, Y 20986~21786 | `Train_Line2` Y 21406 |
| 승강장 D (L2, 서쪽 역) | X −35634~−31434, Y 16649~20649 | 레일과 **337 cm 간격**, 사이에 Z 8764 턱. StartingPoint003 (−34034, 19049) |
| 서쪽 입구 L1 | X −35634~−31434, Y 10249~16649 | 개찰구 Y 12449, 박스 마커 18개 |

### 1.3 열차 문 방향 (`GridTrain.cpp` "문은 로컬 +Y 면")

| 열차 | yaw | 문이 향하는 월드 방향 | 승강장 위치 | 판정 |
|---|---|---|---|---|
| Stage1 `Train_Line1` | −90 | +X(동) | 동쪽 승강장 | OK |
| Stage2 `Train_Line1` | −90 | +X(동) | A′·B 모두 **서쪽** | **yaw +90으로 반전** |
| Stage2 `Train_Line2` | 0 | +Y(북) | C·D 모두 **남쪽**(레일이 북쪽으로 이동) | **yaw 180으로 반전** |

### 1.4 자동 탑승 셀 계산의 한계 (`ComputeBoardingCells`)

문 위치에서 `BodyWidth/2 + 0.75셀` 바깥부터 **최대 3칸**을 보며 첫 번째 walkable 셀을 고른다. Stage2 A′·D처럼 승강장과 선로 사이에 낮은 턱(Z 8764, 승강장보다 356 cm 아래)이 3~4셀 폭으로 있으면, 턱 셀이 walkable로 잡혀 **폰이 갈 수 없는 탑승 셀**이 된다(계산상 A′: 1612 → 1512(턱) 채택, D: 21015 → 20915(턱) 채택). 승강장 셀은 5번째 칸에야 나온다. → 2.5절 코드 보완 또는 아트 수정 필요.

### 1.5 이동 시간

기본 속도 곡선(Accel 0.30, Decel 0.15, Ease 2, MinSpeedFactor 0.05)은 구간 길이에 **비례**해서 가감속하므로, 통과 시간 ≈ **4.1 × 거리 / Speed**.

| 구간 | 거리 | 현재(Speed 1200) 예상 |
|---|---|---|
| Stage1 동쪽 승강장 → 서쪽 승강장 (3750 → 17550) | 138 m | ≈ 47 s (이전 50 m → 17 s) |
| Stage2 A′ → B (Y 1226 → 18826) | 176 m | ≈ 60 s |
| Stage2 D → C (X −33577 → −10577) | 230 m | ≈ 79 s |

---

## 2. 수정 계획

순서대로 진행한다. 1단계가 끝나야 셀 번호가 확정되므로 2·3단계의 셀 값은 1단계 후 에디터에서 다시 확인한다.

### 2.1 Stage1 그리드 복구·확장 (에디터, 저장 필요)

1. `StationGrid` 위치 → **(−11500, −2500, −250)** (원래 값). 동쪽 역 셀 번호가 전부 복구된다.
2. `SizeInCells` → **(140, 215)** (Y 최대 19000 — 서쪽 구역 볼륨 상단 18536, 레일 끝 18375를 덮음). 셀 30,100개.
3. `Generate Grid` → 저장. `Log Debug Report`로 walkable ≈ 4271 전후, override ≈ 447 확인. `serialized grid is … expected` 에러가 없어야 한다.
4. 서쪽 셀 값 재지정(+88): `PuzzleElevatorDock2.TargetFloorCell` (62,118) → **(62,206)**. 나머지 도크는 `TargetFloorCell`이 (−1,−1)이라 해당 없음. 마커·블록·회전판·`ElevatorRegion`(동쪽)은 위치 기반이라 자동.
5. `Train_Line1.Stops[1]` `Tunnel_North` `StopCell` (102,112) → **(102,200)** (이동한 서쪽 승강장 Y 15929~18317 옆, 월드 Y 17550). 연출용 정차역이므로 `ExitCell`은 그대로 (−1,−1).
6. `Train_Line1` 액터는 BeginPlay에서 `Stops[0]`로 스냅하므로 위치는 손대지 않는다(Z −124, yaw −90만 유지).

### 2.2 Stage2 그리드 재배치 (에디터, 저장 필요)

중앙 역(승강장 B·C, 블록 40개)은 안 움직였으므로 **원래 원점에서 정수 셀만큼만 옮겨** 중앙 정렬을 보존한다. 입구·서쪽 블록은 어차피 셀 격자에 안 맞게 이동했으므로 2.4에서 재스냅한다.

1. `StationGrid` 위치 → **(−35827, −5324, 8070)** = 원래 원점 − (122셀, 97셀). 커버 X −35827~, Y −5324~.
2. `SizeInCells` → **(390, 278)** → X 최대 3173(L2 턱 2727), Y 최대 22476(북쪽 벽 22154). 셀 108,420개(ClampMax 512 이내). 생성 시간이 기존 50k셀의 두 배쯤 걸린다.
3. `Generate Grid` → 저장. walkable은 이전 12830 근처여야 한다(입구·서쪽 역 이동으로 다소 달라질 수 있음).
4. 기존 셀 → 새 셀 변환은 **(+122, +97)**. 중앙 역 저작값은 이 규칙으로 옮긴다.

### 2.3 Stage2 열차 재저작

| 열차 | 변경 | 값 |
|---|---|---|
| `Train_Line1` | 회전 | yaw −90 → **+90** (문이 서쪽 A′·B를 보게) |
| | `Stops[0]` `S2_PlatformA` | `StopCell` **(378, 65)** — 월드 (2023, 1226), 승강장 A′ 중심 Y 1231. `ExitCell` **(368, 65)**(승강장 위 X 1023) |
| | `Stops[1]` `S2_PlatformB` | `StopCell` (256,144) → **(378, 241)**, `ExitCell` (246,144) → **(368, 241)** — 월드 위치는 이전과 동일 |
| `Train_Line2` | 회전 | yaw 0 → **180** (문이 남쪽 C·D를 보게) |
| | `Stops[0]` `S2_PlatformD` | `StopCell` **(22, 267)** — 월드 (−33577, 21426). `ExitCell` (22,116) → **(22, 258)**(승강장 D 위 Y 20526) |
| | `Stops[1]` `S2_PlatformC` | `StopCell` **(252, 267)** — 월드 (−10577, 21426). `ExitCell` **(252, 258)** |

`BoardingCells`는 네 역 모두 비워 둔 채 자동 계산에 맡긴다(2.5 보완 전제). `DisembarkCount` 3 유지. 두 열차 모두 Z 8988.15 유지.

### 2.4 Stage2 블록·마커·시작점 정리

1. 삭제된 `Vending_SM_L2_VendingMachine3` 복구: `BP_Block_VendingMachine_Small`을 **(−617, 951, 9120) yaw 90**(입구 역 델타 적용값)에 배치 — 아트가 의도적으로 뺀 것인지 먼저 확인(3절 Q3).
2. `GridBoxMarker6`·`7` Z → 8330.1 (정리용).
3. 퍼즐 블록 78개(+복구 1개) **전부 셀 중심으로 재스냅 + 겹침 검사** 스크립트 실행(`47c0f89` 때 한 것과 같은 절차, 이번엔 `Tools/Editor/`에 스크립트를 남긴다). 입구 16개는 (−0.40, −0.25)셀, 서쪽 22개는 최대 0.5셀 어긋나 있어 그대로 두면 BeginPlay에서 튀거나 벽에 걸린다.
4. `PlayerStart` (83, −4699, 8412)는 새 그리드 안이며 L1 입구 위 — 유지. `StageInfo`(StageIndex 2, ZoneCameras 없음 → 폰 카메라) 유지.

### 2.5 코드 보완 (`Source/LetsTakeTheSubway/Vehicle/GridTrain.h/.cpp`)

1. **탑승 셀 탐색 강화** — `ComputeBoardingCells`
   - 바깥 탐색 칸수 3 → 프로퍼티 `BoardingSearchCells`(기본 **6**).
   - 후보 셀의 바닥 Z가 `TrackZ ± BoardingFloorTolerance`(기본 **150 cm**) 안에 있을 때만 채택. Stage1 승강장 +98, Stage2 승강장 +132는 통과하고 Stage2 턱 −224는 걸러진다.
   - 이 두 가지로 A′(5번째 칸)·D(5번째 칸) 모두 승강장 셀을 잡는다. 기존 맵(그레이박스·Stage1)은 첫 칸에서 그대로 잡히므로 영향 없음.
2. **이동 시간 튜닝** — 코드 변경 없이 인스턴스 값으로 먼저 맞춘다. 목표 통과 시간 ≤ 15 s 기준 예시: `Speed` 2500, `AccelFraction` 0.12, `DecelFraction` 0.08 → Stage1 ≈ 13 s, Stage2 Line1 ≈ 17 s, Line2 ≈ 22 s. 부족하면 거리 기반 가감속(`AccelDistance`/`DecelDistance` cm)을 추가하는 2차 작업으로 넘긴다.
3. **재발 방지** — 두 맵의 `StationGrid`·열차·구역 볼륨·`PlayerStart`에 **Lock Actor Movement**(`bLockLocation`)를 켠다. 아트가 박스 선택으로 역을 옮길 때 그리드 액터가 딸려가는 사고가 이번이 처음이 아니다(`24a5585` 참고). 추가로 `AGridActor::PostEditMove`에서 이동량이 1셀을 넘으면 경고 로그를 찍는다(선택).

### 2.6 아트 팀 확인·요청 사항

1. **Stage2 승강장–선로 간격**: A′(340 cm), D(337 cm)를 B·C처럼 선로 바닥에 맞닿게 옮겨 달라고 요청. 서쪽 역은 레일과 같은 +4810이 아니라 +4473으로 옮겨져 생긴 차이로 보인다. 2.5-1로 코드가 버티긴 하지만 시각적으로도 3.4 m 틈이 남는다.
2. Stage2 서쪽 역 L1 그룹이 L2 그룹과 40/25 cm 어긋난 것(14개 액터) — 100 cm 배수로 정렬 요청.
3. Stage1 새 레일이 역보다 2.45 cm 더 나간 것 — 무시 가능, 참고만.
4. `CAM_Isometric1` 변경(1.1)이 의도한 재프레이밍인지 확인. 아니면 (480, −277, 1020) rot (−35.264, −135, 0)으로 복구.
5. `Vending_SM_L2_VendingMachine3` 삭제가 의도인지 확인.

### 2.7 문서

- 본 문서 3절에 실측 결과·최종 값 추가.
- `Docs/Plans/Subway_Stage1.md` 9.3의 그리드 크기(140×125 → 140×215)와 `Tunnel_North` 셀 갱신.
- Stage2 전용 기록이 없으므로 본 문서가 Stage2 기준값 문서를 겸한다.

---

## 3. 검증

도구가 없으므로 로그 기반이다. PIE → 출력 로그 `LogLTTSGrid` 필터.

| 확인 | 기대 |
|---|---|
| `serialized grid is N cells but AxB was expected` | 없음 |
| `stop N '…' cell (x,y) is outside the grid` | 없음 |
| `stop N '…': K boarding cell(s)` | Stage1 [0] ≥ 1 (이전 20), Stage2 A′·B·C·D 모두 ≥ 1 |
| `covers cell (x,y), which is not walkable floor` / `already taken by` | 없음 |
| `starts outside every stage zone` (Stage1) | 없음 |
| `has no camera for zone N` (Stage1) | 없음 |
| `Log Debug Report` 통계 | Stage1 walkable ≈ 4271 / override ≈ 447, Stage2 walkable ≈ 12830 |

수동: `ltts.GridDebug 2`로 오버레이 확인, `ltts.TrainArrive`로 정차 위치·문 방향 육안 확인, Stage1은 동쪽 승강장 탑승 → 서쪽 도착, Stage2는 A′ 탑승 → B 하차, D 탑승 → C 하차를 실제로 타 보고 통과 시간을 잰다. Stage1 구역 1→2→3→4 카메라 전환도 다시 밟는다.

## 4. 결정 사항 (2026-09-14 사용자 답변)

- 열차는 **모든 구간 10초**. 곡선은 Accel 0.2 · Decel 0.15 · Ease 2 · MinSpeedFactor 0.25(시간 계수 1.70)로 두고 열차별 `Speed = 1.70 × 구간 거리 / 10`.
- `CAM_Isometric1` 변경과 `Vending_SM_L2_VendingMachine3` 삭제는 **아트 커밋 그대로 유지**(2.4-1, 2.6-4·5 취소).
- Lock Actor Movement는 그리드·열차·구역 볼륨·StageInfo·PlayerStart.
- `Tunnel_North`는 계획 기본값 (102,200). 승강장 간격은 코드 보완으로 해결했고 아트 요청은 선택 사항으로 남긴다.

## 5. 구현 결과 (2026-09-14)

### 5.1 레벨 (저장 완료)

| 항목 | Stage1 | Stage2 |
|---|---|---|
| `StationGrid` | (−11500, −2500, −250), **140×215** | **(−35827, −5324, 8070), 390×278** |
| 생성 통계 | walkable 4127 · blocked 11849 · noFloor 14124 · override 447 · stepBreaks 155 | walkable 14437 · blocked 961 · noFloor 93022 · override 207 · stepBreaks 892 |
| 열차 | `Train_Line1` `Tunnel_North` (102,200), Speed 2350 | `Train_Line1` yaw **90**, A′ (378,65)/Exit (368,65), B (378,241)/Exit (368,241), Speed 2990 · `Train_Line2` yaw **180**, D (22,267)/Exit (22,258), C (252,267)/Exit (252,258), Speed 3910 |
| 기타 셀 값 | `PuzzleElevatorDock2` `TargetFloorCell` (62,206) | `GridBoxMarker6`·`7` Z 8330.1 |
| 블록 | 변경 없음 | 이동으로 벽에 걸린 4개를 한 칸 안쪽으로: `Vending_SM_L1_VendingMachine004` +X, `Vending_SM_L2_VendingMachine2` −Y, `Pillar_SM_L2_Pillar4` +X, `Vending_SM_L1_VendingMachine5` +X (각 100 cm) |
| PlayerStart | 이동 잠금 켬 | 이동 잠금 켬 |

그리드 재생성은 디테일 패널 `Generate Grid`로 했다. MCP로 위치·`SizeInCells`를 바꿔도 `PostEditMove`/`PostEditChangeProperty`가 불리지 않아 자동 재생성되지 않는다.

### 5.2 코드

- `AGridTrain` — `BoardingSearchCells`(기본 6), `BoardingFloorTolerance`(기본 150 cm) 추가. `ComputeBoardingCells`가 선로 높이 ±허용치 밖의 걸을 수 있는 셀(턱)을 건너뛰고 더 바깥을 본다.
- `AGridActor`·`AGridTrain`·`AStageZoneVolume`·`AStageInfo` — 생성자에서 `bLockLocation = true`(에디터 전용). 기존 배치 인스턴스에도 적용됨을 에디터 메뉴(Actor > Transform > Lock Item Movement)로 확인했다. `PlayerStart`는 엔진 클래스라 인스턴스마다 수동으로 켰다.

### 5.3 검증 (PIE, `LogLTTSGrid`)

| 확인 | 결과 |
|---|---|
| 그리드 밖 정차역 / 셀 수 불일치 | 없음 |
| 구간 시간(출발 로그 → 문 열림 로그) | Stage1 **10.00 s**, Stage2 Line1 **10.01 s**, Line2 **10.01 s** |
| 탑승 셀 | Stage1 `Platform_B2_East` 20칸(이전과 동일). Stage2 A′ 18 · B 18 · D 19 · C 19칸, 모두 승강장 높이(보완 전 A′·D는 턱 위였음) |
| 시작 구역·카메라 (Stage1) | 구역 1, `CAM_Isometric1` |
| 블록 겹침 | 없음 |

### 5.4 남은 경고 (이번 변경과 무관, 위치가 바뀌지 않은 블록)

- Stage1: `BP_Block_LBench_C_7`, `BP_Block_VendingMachine_C_9`·`C_5`, `BP_RotatingPillar_Station_C_17`(동쪽), `BP_RotatingPillar_Station_C_15`(서쪽)이 걸을 수 없는 셀에 걸침. `C_9` 기둥 부착 면 없음, `PuzzleElevatorDock_5`·`6` 겹침·목표 없음. `Subway_Stage1.md` 9.6-1과 같은 계열.
- Stage2: 중앙 역 `Vending_SM_L1_VendingMachine4`(`BP_Block_VendingMachine_Small_C_9`)가 셀 (313,229)에 걸침. 중앙 역은 이동하지 않았으므로 이전부터 있던 경고다.
- Stage1 `Tunnel_North`는 승강장이 없어 탑승 셀 0칸(연출용, 이전과 같음).

### 5.5 직접 확인이 필요한 것

- 실제로 열차를 타고 내리는 흐름: Stage1 동쪽 → 서쪽, Stage2 A′ → B, D → C. 하차 셀이 승강장 가장자리와 가까워 폰이 벽에 걸리지 않는지.
- Stage2에서 한 칸 옮긴 블록 4개가 아트 배치와 1 m 어긋나 보이는지.
- 아트 요청(선택): Stage2 A′·D 승강장과 선로 사이 3.4 m 틈, 서쪽 역 L1 그룹의 40/25 cm 어긋남.
