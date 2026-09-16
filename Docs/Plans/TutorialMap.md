# 튜토리얼 맵 플레이 가능화 — L_ToonSample_Tutorial

작성일 2026-09-16. 출처: 사용자 요청(채팅) 4건.
대상 레벨: `Content/Maps/Main/L_ToonSample_Tutorial.umap`(아트 배치, git 미추적 새 파일).
선행 문서: `Subway_Stage1.md`(그리드·엘리베이터 배치 절차), `UIConnection.md`(StageInfo·구역 카메라).

## 1. 요청과 결과

| # | 요청 | 결과 |
|---|---|---|
| 1 | 그리드 시스템으로 이동 가능하게 | `TutorialGrid` 149x20칸, 전부 walkable. 게임 모드를 `GridTestGameMode`로 |
| 2 | 카메라 전환을 위해 StageInfo 추가 | `StageInfo`(StageIndex 0) + 구역 볼륨 3개 + 카메라 3대 연결 |
| 3 | 튜토리얼 클리어 시 Stage1 로드 | 엔딩 도크에 `ClearNextLevel` = Subway_Stage1 (코드 추가) |
| 4 | `BP_elevator`를 스테이지 엘리베이터로 교체 | `APuzzleElevatorBlock` + 아트 `BP_elevator`(Stage1 `ElevatorCar2`와 같은 설정) |

## 2. 맵 원래 상태 (2026-09-16 조회)

- 걷는 바닥은 `Ground`(`/Engine/BasicShapes/Cube`) 한 장. X −13848~1152, Y −407~1593, 윗면 Z 75. 그 아래 `Floor`(템플릿 바닥, Z 0)는 배경.
- `PlayerStart` (650, 650, 167) 동쪽 끝. 플레이어는 서쪽으로 간다.
- 아트 `BP_elevator_C_1` (−5320, 593, 70) yaw 90 — 게임 로직 없는 아트 액터였다.
- `PuzzleElevatorDock_1` (−7740, 610, 70) — 이미 `bIsClear`·`bClearOnStageClear` 켜짐, `ClearTravelHeight` 100, `GridTraceIgnore` 태그.
- `CAM_Isometric1~3` X −2787 / −5547 / −8547, 직교(OrthoWidth 3000), pitch −35.264 yaw −45. 셋 다 `AutoActivateForPlayer` Player0.
- 월드 세팅 게임 모드 `BP_gamemode_test`(부모 `GameModeBase`) — 그리드 폰이 생기지 않는다.
- 원본 백업: 세션 스크래치패드 `backup/L_ToonSample_Tutorial.orig.umap`(맵이 git에 없어서 만들었다).

## 3. 코드 변경

| 파일 | 변경 |
|---|---|
| `Puzzle/PuzzleElevatorDock.h/.cpp` | `ClearNextLevel`(TSoftObjectPtr<UWorld>)·`ClearNextLevelDelay`(기본 2초). 엔딩 승강기가 멈춰 연출 훅을 부른 뒤 타이머로 `OpenLevelBySoftObjectPtr`. EndPlay에서 타이머 해제 |
| `Stage/StageInfo.h` | `StageIndex` ClampMin 1 → 0. 튜토리얼은 0 |
| `Grid/GridActor.cpp` | 콘솔 명령 `ltts.GenerateGrid` — 디테일 패널 Generate Grid 버튼과 같다. 자동화 도구가 CallInEditor 버튼을 누를 수 없어서 추가 |

## 4. 레벨 배치

| 액터 | 값 |
|---|---|
| 월드 세팅 | `DefaultGameMode` = `GridTestGameMode` |
| `CAM_Isometric1~3` | `AutoActivateForPlayer` Disabled (구역 카메라와 싸우지 않게) |
| `TutorialGrid`(신규, 폴더 Gameplay) | 원점 (−13840, −390, −100), 149x20, RegionHeight 600, Step 100, Slope 45. 원점은 도크 셀 경계에 맞췄다 |
| `ElevatorCar_Tutorial`(신규) | `APuzzleElevatorBlock` (−5340, 610, 75) yaw 90 → 셀 (83,8). 4x4, `DoorAxis`/`MoveAxis` AxisY(yaw 90이라 월드 X로 밀린다), Height 300, `bParkOnFloorTile`, `VisualActorClass` `BP_elevator`. 원래 아트 액터는 삭제 |
| `PuzzleElevatorDock_1` | 셀 (59,8). `ClearNextLevel` = `/Game/Maps/Main/Subway_Stage1`. 나머지 아트 설정은 그대로 |
| `StageInfo`(신규) | StageIndex 0, StageName Tutorial, ZoneCount 3, ZoneCameras = CAM_Isometric1·2·3. PathUI·진입 가이드는 비움 |
| `StageZone1~3`(신규, 폴더 Gameplay/Zones) | 경계 X −3187 / −6067. 카메라가 바닥(Z 75)에서 보는 중심 X(−1807 / −4567 / −7567)의 중간값 |

엘리베이터는 셀 (83,8)에서 서쪽으로 24칸 밀면 도크 (59,8)에 올라간다. 도크 위에서 타면 100 cm 올라간 뒤 2초 후 Subway_Stage1이 열린다.

## 5. 확인
- 빌드 성공, 경고 0.
- 맵 저장 → 다른 맵 열었다 다시 열어 값 유지 확인.
- PIE: `stage 0 'Tutorial', 3 zones; player starts in zone 1`, `view target -> CAM_Isometric1`, 도크 셀 (59,8), 엘리베이터 셀 (83,8) 1/4 회전, 경고 0.
- **손으로 확인할 것:** 걸어서 구역 2·3으로 갈 때 카메라 전환, 엘리베이터를 서쪽으로 끌기, 도크 위에서 타면 Stage1 열림.

## 6. 남은 점
- 플레이어 시작점 X 650은 1구역 카메라 화면 중심에서 약 20 m 동쪽이라 화면 가장자리 밖일 수 있다. 카메라 위치나 시작점 조정은 아트 쪽 판단.
- `PuzzleRegion`이 없어 엘리베이터 이동 제한이 그리드 범위뿐이다(`region none` 로그). 필요하면 구간을 추가한다.
- MainUI 시작 버튼(`PlayLevelName`)은 아직 Subway_Stage1을 연다. 튜토리얼부터 시작하려면 값을 `L_ToonSample_Tutorial`로 바꾼다.

## 7. 2구역 진입 가이드 (2026-09-16)

요청: 기획 시트의 첫 번째 가이드 팝업을 튜토리얼 2구역에 들어가자마자 띄운다.

- 시트 1행 [엘리베이터 이동] = `EGuideType::Tutorial1`(`DA_GuideDatabase`의 Tutorial1 문구와 같다).
- 코드 변경 없이 `AGuideCellTrigger` `GuideTrigger_Zone2_Tutorial1`(폴더 Gameplay/Guides)을 놓았다.
  중심 (−4627, 610, 175), 박스 X 1440 / Y 1000 / Z 200 — 2구역 볼륨과 같은 X 범위(−6067 ~ −3187)로 그리드 폭 전체를 덮는다.
- 플레이어 폰이 이 범위의 셀에 처음 들어서는 순간 한 번 뜬다. 1구역 쪽에서 오면 X −3190 셀(2구역 경계 바로 안)이 첫 칸이다.
- 맵 저장 확인(08:43:26 저장, 파일에 액터 이름 존재). 트리거 등록 로그는 PIE에서 아직 보지 않았다.
- 같은 PIE 로그에서 튜토리얼 클리어 → `opening /Game/Maps/Main/Subway_Stage1 in 2.0 s` → Stage1 로드가 실제로 동작한 것을 확인했다.
