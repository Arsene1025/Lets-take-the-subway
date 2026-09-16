# 바닥 타일 아트 메시 — 회전판과 엘리베이터 구조물

작성 2026-09-16. 대상 코드는 `Source/LetsTakeTheSubway/Puzzle/PuzzleFloorTile.h/.cpp`,
`PuzzleRotationTile.cpp`, `PuzzleElevatorDock.cpp`.

## 0. 왜

ART_2가 스테이지 2에서 회전판과 엘리베이터 구조물의 메시를 바꾸다 말았다(커밋 `1e40e7f`,
"스테이지 2 에셋 교체 중"). 아티스트는 레벨 인스턴스의 `PadMesh`에 아트 메시를 직접 꽂고
액터 스케일로 크기를 맞췄는데, `APuzzleFloorTile::RefreshVisual()`이 `OnConstruction`마다
패드 스케일을 `(Span, Span, 0.04)`로 덮어써서 그 방식으로는 끝나지 않았다. 스테이지 1은
아예 손대지 않은 그레이박스 큐브였다.

코드가 아트 메시를 정식으로 받는 자리를 만들어 두 스테이지를 한 번에 끝냈다.

## 1. 결정

| 항목 | 결정 | 왜 |
|---|---|---|
| 컴포넌트 | **기존 `PadMesh`에 메시만 갈아 끼운다** | 블록과 달리 타일에는 남겨 둘 프록시가 없다. 패드는 애초에 콜리전이 없어 커서 판정에도 그리드 생성에도 관여하지 않는다. 별도 컴포넌트를 두면 맵에 저장된 옛 `PadMesh` 오버라이드가 보이는 채로 남는다 |
| 슬롯 이름 | `ArtMesh` / `ArtMeshOffset` / `ArtMaterial` | 블록의 `ArtMesh` 패턴(`PuzzleBlock.h`)과 같은 이름·같은 규약 |
| 기본값 | 파생 생성자에서 `ConstructorHelpers`로 **하드 참조** | 소프트 참조로 두면 애셋이 옮겨졌을 때 조용히 회색 큐브가 된다. 하드 참조는 에디터 시작 로그에 즉시 뜬다. 같은 생성자가 이미 GreyBox 머티리얼을 하드 참조하고 있어 패턴도 일관된다. README의 "Core→Art 하드 참조 지양"에서 벗어나는 예외이며, 근거는 이 문단이다 |
| XY 스케일 | 코드가 **영역 크기에 맞춰 계산** | `SizeInCells`가 4가 아닌 회전판에도 같은 메시를 쓸 수 있다. `ArtMeshOffset`의 스케일은 그 위에 곱해진다 |
| 머티리얼 | `ArtMaterial`을 슬롯 0에 **덮어쓴다** | 층 구분과 도킹 상태를 색으로 읽는 것이 타일의 기능이다. 블록의 `ArtFallbackMaterial`(빈 슬롯만 채움)과 다른 점이다. 아트 머티리얼을 살리려면 인스턴스에서 `ArtMaterial`을 비운다 |

## 2. 스펙 (사용자 지정, 2026-09-16)

| 타일 | 메시 | 머티리얼 | `ArtMeshOffset` Z |
|---|---|---|---|
| `APuzzleRotationTile` | `/Game/Art/JW_asset/SM_EV_PLATE001` | `/Game/Art/GreyBox/Materials/MI_GreyBox_F0` | **5** |
| `APuzzleElevatorDock` | `/Game/Art/JW_asset/elevator/SM_ElevatorPlace_001` | 같음 | **20** |

**크기.** 두 메시 모두 로컬 바운드가 XY ±200, 즉 **400 cm = 4셀**이다. 기본 `SizeInCells` 4에서
계산된 배율이 정확히 1.0이 된다.

> 요청서의 "스케일 전체 0.25"는 **옛 코드가 강제하던 4배를 상쇄하는 값**이었다(400 × 4 × 0.25
> = 400). 강제 스케일이 사라진 지금은 1.0이 같은 결과다. 사용자가 2026-09-16에 "최종 월드
> 스케일 1.0"으로 확정했다.

**Z 오프셋.** `SM_EV_PLATE001`은 두께 5 cm(Z ±2.5)뿐이라 그레이박스 패드와 같은 5 cm에 띄운다 —
에디터 그리드 오버레이(바닥 위 2 cm)와 같은 평면을 피하기 위해서다. `SM_ElevatorPlace_001`은
피벗이 메시 한가운데(Z −19.8 ~ +21.6)라 20을 올려야 밑면이 바닥에 닿는다.

## 3. 구조물의 도킹 색

`APuzzleElevatorDock::RefreshDockedLook()`은 차체가 올라와 있는지에 따라 패드 슬롯 0을
갈아 끼운다. 이것이 "여기 올려놓으면 된다"와 "이제 탈 수 있다"를 가르는 유일한 신호라 아트
메시로 바뀐 뒤에도 남겼다.

| 상태 | 머티리얼 |
|---|---|
| 비어 있음 (`IdleMaterial`) | `MI_GreyBox_F0` — **`ArtMaterial`과 같은 자산이어야 한다** |
| 차체가 올라옴 (`ReadyMaterial`) | `MI_GreyBox_Movable` |

Idle이 `ArtMaterial`과 어긋나면 `RefreshVisual`이 F0을 깔고 `RefreshDockedLook`이 다른 색으로
되돌려, 차체가 떠난 뒤 판 색이 돌아오지 않는다. 예전 Idle은 `MI_GreyBox_B2`였다.

## 4. 액터 스케일은 언제나 1이다

타일의 판정 영역은 `SizeInCells × CellSize`로만 정해진다(`BeginPlay`의 `Region`). 액터
스케일은 보이는 판만 줄이고 판정은 그대로 두므로, 스케일이 걸려 있으면 **눈에 보이는 것과 실제
판정되는 영역이 어긋난다.**

스테이지 2에는 옛 강제 스케일을 상쇄하려던 액터 스케일이 10개 남아 있었고(회전판 2개 0.25,
구조물 8개 0.2, 그중 둘은 Z까지 2.2·2.8로 어긋나 있었다), 새 코드에서는 이중 축소가 되어
판이 100 cm·80 cm로 줄었다. 전부 1.0으로 되돌리고 맵을 저장했다.

다시 생기지 않도록 두 가지를 넣었다.

- `PostEditMove`가 회전과 함께 **스케일도 1로 되돌린다**. 에디터에서 타일을 옮기기만 해도 고쳐진다.
- `BeginPlay`가 스케일이 1이 아니면 경고를 남긴다(`LogLTTSGrid`).

## 5. 적용 현황

| 레벨 | 회전판 | 구조물 | 맵 저장 |
|---|---|---|---|
| `Subway_Stage1` | 4 | 5 | **불필요** — 전부 코드 기본값이라 맵이 더럽혀지지 않는다 |
| `Subway_Stage2` | 6 | 8 | **저장함** — 액터 스케일 10개를 1.0으로 되돌렸다 |

`Config/DefaultGame.ini`에 `+DirectoriesToAlwaysCook=(Path="/Game/Art/JW_asset")`을 더했다.
네이티브 생성자 기본값은 어떤 패키지에도 저장되지 않고, 배치 액터는 기본값과 같으면 델타를
쓰지 않아 맵에 참조가 남지 않는다. 그래서 쿠커가 이 메시들을 보지 못한다. 폰의 그레이박스
메시(`/Engine/BasicShapes`)를 같은 이유로 이미 등록해 둔 전례가 있다.

## 6. 검증 (2026-09-16)

- 빌드: `Build.bat LetsTakeTheSubwayEditor Win64 Development` 성공, 경고 0.
- 스테이지 1 타일 9개, 스테이지 2 타일 14개 모두 `PadMesh`가 새 메시, `RelativeScale3D`
  `(1,1,1)`, 슬롯 0 `MI_GreyBox_F0`. 액터 바운드 XY **400 × 400 cm**.
- PIE(스테이지 1): 9개 전부 `4x4 floor tile at cell (...)`로 등록. 스케일 경고 없음,
  타일 겹침 경고 없음, disabled 없음.
- 뷰포트 캡처로 회전 화살표 판과 구조물 난간이 바닥에 맞게 놓인 것을 확인.
- 주석 처리된 `CornerMesh`(디버그용 모서리 큐브)는 정식 빌드 뒤 액터에서 사라졌다.

**사람이 봐야 할 것:** 구조물 메시가 바닥에서 41 cm 솟아 있다. 차체가 그 위에 도킹했을 때
난간과 겹쳐 보이지 않는지 아트와 확인이 필요하다. 필요하면 `ArtMeshOffset`의 Z로 조정한다.
