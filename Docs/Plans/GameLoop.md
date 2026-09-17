# 게임 루프 완성 계획 — 타이틀 → 시작 컷씬 → 튜토리얼 → Stage1 → Stage2 → 엔딩 컷씬 → 타이틀

작성일 2026-09-17. 대상: `Source/LetsTakeTheSubway/UI/UIManagerSubsystem.h/.cpp`, `UI/UISettings.h`,
`UI/CutScene/CutSceneWidget.h/.cpp`(신규), `Cutscene/CutsceneCellTrigger.h/.cpp`(신규),
`Content/UI/WBP_CutScene`, `Content/UI/WBP_MainUI`, `Content/UI/CutSceneData/DA_CutScene_Intro`·`DA_CutScene_Ending`,
`Content/Maps/Main/Subway_Stage2.umap`, `Config/DefaultGame.ini`.
선행 문서: `UIConnection.md`(UI 매니저·입력 차단·MainUI 시작 버튼), `BusStopCutscene.md`(Stage1→2 버스 컷씬),
`TutorialMap.md`(튜토리얼→Stage1), `../StageClearElevator.md`(엔딩 승강기 훅).

---

## 0. 요청 해석과 현재 상태

요청한 흐름: **타이틀 → 시작 컷씬 → 튜토리얼 → 스테이지 1 → 스테이지 2 → (엔딩 볼륨 진입) 엔딩 컷씬 → 타이틀.**
컷씬 그림은 `C:\Users\User\Downloads\컷신\컷신\UI_Open_1~4.png`(시작), `UI_End_1~4.png`(엔딩), 모두 2560×1440.
UI는 `WBP_CutScene`을 쓴다.

2026-09-17 조회(소스, 애셋 문자열, 에디터 MCP로 위젯 그래프 읽음):

| 구간 | 지금 | 해야 할 일 |
|---|---|---|
| 타이틀 → 튜토리얼 | `WBP_MainUI` `startBtn` → `Open Level (by Name)`(`PlayLevelName` = `L_ToonSample_Tutorial`). 컷씬 없음 | 시작 컷씬을 끼운다 |
| 튜토리얼 → Stage1 | `PuzzleElevatorDock_1.ClearNextLevel` = `Subway_Stage1` (TutorialMap.md 4절) | 없음 |
| Stage1 → Stage2 | Stage1 레벨 BP: `OnDockClearCutscene` → 카메라 페이드 → `L_ToonSample_BusStop_Start` → (디렉터) `_Arrived` → `Subway_Stage2` | 없음. **버스 정류장 컷씬은 그대로 둔다**(7절 Q1) |
| Stage2 → 엔딩 | 없음. Stage2에는 `StageZoneVolume` 8개, `GuideCellTrigger`, `SoundCellTrigger`만 있다 | 엔딩 볼륨(액터) 신규 + 배치 |
| 엔딩 → 타이틀 | 없음 | 엔딩 컷씬 뒤 `Subway_Title` 열기 |
| 컷씬 위젯 | `WBP_CutScene`: 디자이너에 배경 이미지·`skipBtn`, 변수 `cutSceneData`·`frameIndex`. **`InitializeInt` 그래프가 미완성**: 0번은 `cutSceneData`를 비우고(null) 프레임 하나만 그리고 루프가 없으며, 1번은 `DA_CutScene_Ending`을 넣고 끝난다. 비교식도 `Length < frameIndex`로 뒤집혀 있다. `skipBtn`·Default 핀은 `EndCutscene`+`CallUIClosed` | 재생 논리를 새로 만든다(1절 결정 1) |
| 컷씬 데이터 | `DA_CutScene_Intro`·`DA_CutScene_Ending`(`UCutSceneDatabase`) 둘 다 **Frames 비어 있음**(파일 1.7 KB). 그림은 아직 프로젝트에 없음 | 텍스처 8장 임포트, Frames 채움 |
| C++ | `UUIManagerSubsystem::PlayCutscene(int32)`가 위젯을 만들고 `InitializeInt`를 부른다. `EndCutscene`은 위젯만 지운다. **끝난 뒤 할 일(레벨 열기)이 없다.** `ClearAllCachedWidgets`가 `CutsceneWidget`을 빼먹어 맵이 바뀌어도 포인터가 남는다. ESC는 컷씬 중 무시(`HandleEscapeKey`) | 컷씬 종료 → 다음 레벨, 맵 전환 정리, 콘솔 명령 |
| 패키징 | `MapsToCook`에 Title·Tutorial·Stage1·BusStop_Start·BusStop_Arrived·Stage2 모두 있음. `GameDefaultMap` = `Subway_Title` | 없음 |

---

## 1. 결정

1. **컷씬 재생 논리는 C++ 위젯 베이스 `UCutSceneWidget`에 두고, `WBP_CutScene`은 그 자식으로 재부모화한다.**
   BP 그래프가 미완성이고, 프레임 루프·스킵 가드·타이머를 BP `Delay`로 짜면 스킵 뒤에도 잠재 노드가 남아 `EndCutscene`이 두 번 불리는 문제가 생긴다.
   디자이너(배경 이미지, `skipBtn`, 폰트)는 그대로 쓴다. 이벤트 그래프의 `InitializeInt`·`skipBtn` 이벤트와 변수 두 개는 부모가 대신하므로 지운다.
   대안(BP 그래프를 MCP `write_graph_dsl`로 다시 짜기)은 7절 Q3.
2. **"컷씬이 끝나면 어느 레벨을 열지"는 UI 매니저가 안다.** `PlayCutscene(ECutsceneKind Kind, TSoftObjectPtr<UWorld> NextLevel)` 하나로
   시작(타이틀 → 튜토리얼)과 엔딩(Stage2 → 타이틀)을 같은 경로로 처리한다. `EndCutscene`(스킵 포함)이 위젯을 지우고 `OnCutsceneEnded`를 방송한 뒤 레벨을 연다.
3. **시작 컷씬은 타이틀 레벨 위에서 재생하고 끝나면 튜토리얼을 연다.** 튜토리얼 레벨 시작 때 재생하면 레벨 시작 판정·진입 가이드·PathUI·구역 카메라 블렌드가
   컷씬 아래에서 동시에 돌아간다. 타이틀에는 UI만 있고 `ATitleGameMode`가 이미 입력을 UI 전용으로 둔다.
4. **엔딩 볼륨은 셀 트리거 `ACutsceneCellTrigger`.** `AGuideCellTrigger`와 같은 방식(박스 → 셀 목록, `AGridActor::OnPawnEnteredCell`)이다.
   폰에 콜리전이 없어 일반 오버랩 볼륨은 반응하지 않고, `ZoneProbe`는 StageZone 채널 전용이다. 기획이 배치하는 것은 박스 하나이므로 "볼륨"으로 보인다.
5. **컷씬 종류는 enum `ECutsceneKind { Intro, Ending }`, 데이터 애셋은 `UUISettings`에 등록.** 위젯이 정수 스위치로 애셋을 고르지 않고,
   매니저가 설정에서 애셋을 찾아 위젯 `Play(Data)`에 넘긴다. 컷씬이 늘면 enum과 설정 항목만 는다.
6. **텍스처는 `Content/UI/CutSceneData/`에 원본 이름 그대로**(`UI_Open_1`~`4`, `UI_End_1`~`4`). UI 팀의 `UI_HELP_IMG`와 같은 설정: `TC_Default`(BC1, 알파 없음), `NoMipmaps`, LODGroup `UI`, sRGB.
   2560×1440은 4의 배수라 BC 압축이 된다. `UserInterface2D`(무압축 RGBA)는 장당 14.7 MB × 8 = 118 MB라 쓰지 않는다.
7. **프레임 시간은 기본 4초**(`FCutsceneFrame::Duration`, 애셋에서 조정). 4장 = 16초. 스킵 버튼은 항상 보인다.
8. 버스 정류장 컷씬(Stage1 → Stage2 사이)은 요청 흐름에 없지만 이미 붙어 있고 동작하므로 **그대로 둔다**(Q1).

---

## 2. 흐름

```
Subway_Title (ATitleGameMode, MainUI)
  startBtn → UIManager.PlayCutscene(Intro, L_ToonSample_Tutorial)
    → WBP_CutScene(UCutSceneWidget) 뷰포트 Z 100, CallUIOpened
    → UI_Open_1 → 4초 → UI_Open_2 → … → UI_Open_4 → 4초   (skipBtn: 즉시 종료)
    → Finish → UIManager.EndCutscene → CallUIClosed, OnCutsceneEnded → OpenLevel(Tutorial)
L_ToonSample_Tutorial ── Dock.ClearNextLevel ──► Subway_Stage1
Subway_Stage1 ── 레벨 BP(OnDockClearCutscene, 페이드) ──► BusStop_Start ──► BusStop_Arrived ──► Subway_Stage2
Subway_Stage2
  플레이어가 CutsceneCellTrigger_Ending 셀에 들어섬(한 번만)
    → UIManager.PlayCutscene(Ending, Subway_Title)
    → UI_End_1 → … → UI_End_4  (skipBtn)
    → EndCutscene → OpenLevel(Subway_Title) → ATitleGameMode가 MainUI를 다시 띄움
```

---

## 3. 변경 내용

### 3.1 데이터 — 텍스처와 데이터 애셋

1. `Downloads\컷신\컷신\*.png` 8장을 `Content/UI/CutSceneData/`로 임포트(에디터 MCP `TextureTools` 또는 콘텐츠 브라우저). 설정은 1절 6번.
2. `DA_CutScene_Intro.Frames` = `UI_Open_1`~`4`, `DA_CutScene_Ending.Frames` = `UI_End_1`~`4`. `Duration` 4.0.
3. `DefaultGame.ini` `[/Script/LetsTakeTheSubway.UISettings]`에 추가:
   ```
   CutsceneWidget=/Game/UI/WBP_CutScene.WBP_CutScene_C
   IntroCutscene=/Game/UI/CutSceneData/DA_CutScene_Intro.DA_CutScene_Intro
   EndingCutscene=/Game/UI/CutSceneData/DA_CutScene_Ending.DA_CutScene_Ending
   ```
   (`CutsceneWidget` 항목은 헤더에만 있고 ini에 값이 없다. 지금 `PlayCutscene`을 불러도 "위젯 클래스가 지정되지 않았습니다"로 끝난다.)

### 3.2 `UI/CutScene/CutSceneWidget.h/.cpp` — `UCutSceneWidget : UUserWidget` (신규)

```cpp
UCLASS(Abstract)
class UCutSceneWidget : public UUserWidget
{
    /** 전체 화면 그림. 디자이너의 Image 위젯 이름과 같아야 한다(에디터에서 bgImage인지 Image인지 확인, 7절 Q4). */
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> bgImage;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> skipBtn;

    /** 프레임 사이 검정 크로스페이드(초). 0이면 즉시 바뀐다. */
    UPROPERTY(EditDefaultsOnly) float FrameFadeSeconds = 0.3f;

    void Play(const UCutSceneDatabase* Data);   // 매니저가 부른다. 프레임 0부터.
    UPROPERTY(BlueprintAssignable) FOnCutSceneFinished OnFinished;   // 끝나거나 스킵되면 한 번

protected:
    virtual void NativeConstruct() override;    // skipBtn.OnClicked 바인딩, CallUIOpened
    virtual void NativeDestruct() override;     // 타이머 정리
    void ShowFrame(int32 Index);                // SetBrushFromSoftTexture → Duration 타이머 → 다음
    void Finish();                              // bFinished 가드, 타이머 정리, OnFinished
};
```

- 텍스처는 `Play`에서 `FStreamableManager::RequestAsyncLoad`로 전부 미리 요청하고, 첫 프레임만 `LoadSynchronous`. 2560×1440 BC1 8장이라 동기 로드도 짧지만 프레임 전환 순간의 히치를 피한다.
- `CallUIOpened`/`CallUIClosed`는 지금 BP와 같은 순서로 부른다: 생성 때 열림, `Finish`에서 닫힘. 컷씬 중 `AGridPlayerController`가 이동키·드래그를 놓고(UIConnection.md 8.1), `HandleEscapeKey`는 사이드 메뉴를 열지 않는다.
- 크로스페이드는 `bgImage`의 `ColorAndOpacity` 알파를 `NativeTick`에서 보간한다(위젯 애니메이션 애셋 없이). 선택 사항이며 `FrameFadeSeconds` 0으로 끌 수 있다.

### 3.3 `UUIManagerSubsystem` 변경

```cpp
UENUM(BlueprintType) enum class ECutsceneKind : uint8 { Intro, Ending };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCutsceneEnded, ECutsceneKind, Kind);

/** 컷씬을 띄우고, 끝나면(스킵 포함) NextLevel을 연다. NextLevel이 비어 있으면 레벨을 바꾸지 않는다. 이미 재생 중이면 무시. */
UFUNCTION(BlueprintCallable, Category = "CutScene")
void PlayCutscene(ECutsceneKind Kind, TSoftObjectPtr<UWorld> NextLevel);

UFUNCTION(BlueprintPure, Category = "CutScene") bool IsCutscenePlaying() const;
UPROPERTY(BlueprintAssignable) FOnCutsceneEnded OnCutsceneEnded;
```

- 기존 `PlayCutscene(int32)`는 지우지 않고 **주석 처리**해 남긴다(팀 규칙). 호출하는 곳이 없어 빌드에 영향 없음.
- `PlayCutscene`: 설정에서 `Kind`에 맞는 데이터 애셋을 `LoadSynchronous` → 위젯 생성(`CutsceneWidget`, Z 100) → `Cast<UCutSceneWidget>` 후 `OnFinished`에 `EndCutscene` 바인딩 → `Play(Data)`. `PendingLevel = NextLevel`, `ActiveKind = Kind`.
  위젯이 `UCutSceneWidget`이 아니면(재부모화 전) 경고만 남기고 곧바로 `EndCutscene`한다. 컷씬이 빠져도 루프는 끊기지 않는다.
- `EndCutscene`: 위젯 `RemoveFromParent` → `CutsceneWidget = nullptr` → `OnCutsceneEnded.Broadcast(Kind)` → `PendingLevel`을 지역 변수로 옮기고 비운 뒤 `OpenLevelBySoftObjectPtr`. 두 번 불려도 레벨을 두 번 열지 않는다.
- `ClearAllCachedWidgets`에 `CutsceneWidget` 제거와 `PendingLevel` 초기화를 넣는다(현재 누락).
- `HandleEscapeKey`: 컷씬 중 ESC를 **스킵**으로 쓴다(`EndCutscene`, true 반환). 지금은 false를 돌려 아무 일도 없다. 선택 사항(P2).
- 콘솔 `ltts.Cutscene Intro|Ending [레벨경로]`: 어느 레벨에서든 컷씬을 띄워 본다. 인자 없는 레벨은 "열지 않음".

### 3.4 `UUISettings` 추가

```cpp
UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UCutSceneDatabase> IntroCutscene;
UPROPERTY(Config, EditAnywhere, Category = "Data") TSoftObjectPtr<UCutSceneDatabase> EndingCutscene;
```

### 3.5 `Cutscene/CutsceneCellTrigger.h/.cpp` — `ACutsceneCellTrigger : AActor` (신규, "엔딩 볼륨")

`AGuideCellTrigger`를 본떠 만든다(박스 → `BeginPlay`에서 셀 목록, `OnPawnEnteredCell`, `AGridPawn`만 반응).

| 속성 | 뜻 |
|---|---|
| `Kind` (`ECutsceneKind`) | 재생할 컷씬. Stage2 엔딩은 `Ending` |
| `NextLevel` (`TSoftObjectPtr<UWorld>`) | 컷씬 뒤 열 레벨. Stage2 엔딩은 `Subway_Title`. 비우면 레벨을 바꾸지 않는다 |
| `Delay` (초, 기본 0) | 셀에 들어선 뒤 컷씬까지 기다리는 시간. 걷던 폰이 멈추는 모습을 보여 주고 싶을 때 |
| `bOnce` (기본 true) | 한 번만 발동. 타이틀로 돌아가면 레벨이 새로 열리므로 다음 판에는 다시 발동한다 |

발동: `UUIManagerSubsystem::Get(this)->PlayCutscene(Kind, NextLevel)`. 폰은 `CallUIOpened`로 입력이 막혀 그 자리에 선다. 파괴·언포제스하지 않는다.
카메라 페이드는 넣지 않는다. 위젯이 화면을 완전히 덮고, 컷씬이 끝난 뒤 곧바로 레벨이 바뀐다.

### 3.6 `WBP_MainUI` (블루프린트, 작은 변경)

- 변수 `PlayLevelName`(Name) → `PlayLevel`(Soft Object Reference: World, 기본값 `L_ToonSample_Tutorial`). BusStopCutscene.md 6.3의 "by Object Reference" 규칙과 같다.
- `OnClicked(startBtn)`: `Open Level (by Name)` → `Get UIManagerSubsystem(GetOwningPlayer)` → `PlayCutscene(Intro, PlayLevel)`.
  MCP `write_graph_dsl`로 고칠 수 있다(그래프를 읽어 둔 상태). 시작 레벨을 바꾸는 방법은 그대로 "변수 기본값만".

### 3.7 `WBP_CutScene` (블루프린트)

- Class Settings → Parent Class = `CutSceneWidget`. 디자이너의 Image 위젯과 `skipBtn` 이름이 `BindWidget` 이름과 같은지 확인(Q4).
- 이벤트 그래프의 `EventInitializeInt`·`OnClicked(skipBtn)` 삭제, 변수 `cutSceneData`·`frameIndex` 삭제. `UIInitializable` 인터페이스는 다른 UI와 같은 관례이므로 남겨도 된다(부르지 않는다).
- 재부모화 전이라도 3.3의 안전장치 덕에 루프는 끊기지 않는다(컷씬만 건너뛴다).

### 3.8 레벨 — `Subway_Stage2`

- `CutsceneCellTrigger_Ending` 배치: `Kind` Ending, `NextLevel` `Subway_Title`. **위치는 기획이 정한다**(Q2). 박스는 셀 경계에 맞추고 폰이 반드시 지나는 폭으로.
- 월드 세팅 GameMode는 `GridTestGameMode` 그대로.

### 3.9 타이틀로 돌아온 뒤의 초기화

- `UGuideDataSubsystem::ShownGuides`는 게임 인스턴스에 남아 두 번째 판에는 가이드가 뜨지 않는다. `PlayCutscene(Intro, …)` 시작 때 `ResetShownGuides()`를 부른다(Q5).
- `UUIManagerSubsystem::OpenUICount`·위젯은 `HandleMapLoaded`가 이미 0으로 되돌린다. 사운드·설정 서브시스템은 그대로 두는 것이 맞다.
- 타이틀 BGM(`BGM_Title` AmbientSound)은 시작 컷씬 동안 계속 난다. 끄려면 컷씬 시작 때 페이드 아웃(Q6).

---

## 4. 변경 파일

| 파일 | 변경 |
|---|---|
| `UI/CutScene/CutSceneWidget.h/.cpp` | 신규. 프레임 재생·스킵·크로스페이드 |
| `UI/UIManagerSubsystem.h/.cpp` | `ECutsceneKind`, `PlayCutscene(Kind, NextLevel)`, `OnCutsceneEnded`, `IsCutscenePlaying`, `EndCutscene`이 레벨 열기, `ClearAllCachedWidgets` 보완, ESC 스킵, 콘솔 명령. 기존 `PlayCutscene(int32)` 주석 처리 |
| `UI/UISettings.h` | `IntroCutscene`, `EndingCutscene` |
| `Cutscene/CutsceneCellTrigger.h/.cpp` | 신규. 엔딩 볼륨 |
| `Config/DefaultGame.ini` | `CutsceneWidget`, `IntroCutscene`, `EndingCutscene` |
| `Content/UI/CutSceneData/UI_Open_1~4`, `UI_End_1~4` | 신규 텍스처 |
| `Content/UI/CutSceneData/DA_CutScene_Intro`, `DA_CutScene_Ending` | Frames 채움 |
| `Content/UI/WBP_CutScene` | 부모 클래스 교체, 그래프·변수 정리 |
| `Content/UI/WBP_MainUI` | `PlayLevel` 변수, 시작 버튼 → `PlayCutscene` |
| `Content/Maps/Main/Subway_Stage2.umap` | `CutsceneCellTrigger_Ending` 배치 |

`Build.cs`는 바뀌지 않는다(UMG는 이미 의존 모듈).

---

## 5. 검증 (PIE)

1. **콘솔**: `Subway_Stage1`에서 `ltts.Cutscene Intro` → 4장이 4초 간격으로 넘어가고 스킵 버튼·ESC로 끝나는지, 끝난 뒤 이동키·클릭이 돌아오는지(`[UIManager] Input mode: game and UI` 로그).
   `ltts.Cutscene Ending /Game/Maps/Main/Subway_Title` → 끝나면 타이틀이 열리는지.
2. **타이틀부터 끝까지**: `Subway_Title`에서 Play → 시작 버튼 → 시작 컷씬 → 튜토리얼 → (엘리베이터) Stage1 → (엘리베이터) 버스 두 맵 → Stage2 → 엔딩 셀 → 엔딩 컷씬 → 타이틀.
   Stage 이동은 `ltts.ElevatorRide`(StageClearElevator.md 6절)로 줄일 수 있다.
3. **두 번째 판**: 타이틀에서 다시 시작 → 튜토리얼 가이드 팝업이 다시 뜨는지(3.9), 엔딩 트리거가 다시 발동하는지.
4. **경계 상황**: 컷씬 중 ESC(스킵만 되고 사이드 메뉴는 안 뜸), 스킵 연타(레벨이 한 번만 열림, `EndCutscene` 로그 1회), 프레임이 비어 있는 데이터 애셋(경고 후 곧바로 종료),
   `WBP_CutScene` 재부모화 전 상태(경고 후 곧바로 다음 레벨).
5. 빌드 경고 0, 텍스처 8장이 `UI` LOD 그룹·NoMipmaps인지 애셋 등록부에서 확인.

---

## 6. 순서

1. 텍스처 임포트, 데이터 애셋 Frames 채움, ini 값(3.1). — 코드와 독립이라 먼저.
2. `UUISettings` → `UCutSceneWidget` → `UUIManagerSubsystem` 순으로 C++(3.4 → 3.2 → 3.3). 빌드.
3. `WBP_CutScene` 재부모화·그래프 정리(3.7). 콘솔 명령으로 컷씬 단독 검증(5절 1).
4. `ACutsceneCellTrigger`(3.5) 빌드, `Subway_Stage2` 배치(3.8).
5. `WBP_MainUI` 시작 버튼(3.6), 가이드 초기화(3.9).
6. 전체 루프 검증(5절 2~4), 이 문서에 구현 기록 추가.

---

## 7. 열린 질문 / 가정

- **Q1. 버스 정류장 컷씬 유지.** 요청 흐름에는 "스테이지 1 → 스테이지 2"만 있다. 이미 붙어 있는 버스 두 맵을 그대로 두는 것으로 가정했다. 빼려면 Stage1 레벨 BP의 `Open Level` 대상을 `Subway_Stage2`로 바꾸면 된다(한 노드).
- **Q2. 엔딩 볼륨 위치.** Stage2 어느 셀인지 기획 지정이 필요하다. 지정 전에는 임시로 8구역(`StageZoneVolume8`) 끝 셀에 두고 검증한다.
- **Q3. 재생 논리를 C++로.** `WBP_CutScene`은 UI 담당 소유다. BP 그래프를 다시 짜는 쪽을 원하면 3.2 대신 MCP로 그래프를 쓰되, 스킵 뒤 `Delay` 잔존을 막기 위한 `bSkipped` 변수와 `IsValid(self)` 검사가 추가로 필요하다.
- **Q4. 디자이너 위젯 이름.** 그래프에서는 `Image`, 애셋 문자열에는 `bgImage`가 보인다. 재부모화 때 실제 이름에 `BindWidget` 이름을 맞춘다.
- **Q5. 두 번째 판의 가이드 팝업.** 다시 보여 주는 것으로 가정했다(`ResetShownGuides`). 한 번 본 가이드는 영영 안 보이게 하려면 이 줄만 뺀다.
- **Q6. 타이틀 BGM.** 시작 컷씬 동안 계속 나는 것으로 가정했다. 컷씬용 BGM(`SW_BGM_Cutscene.wav`는 커밋 제외 대상)을 쓸지는 사운드 담당과 정한다.
- **Q7. 프레임 시간 4초·크로스페이드 0.3초**는 임시값이다. 데이터 애셋과 위젯 기본값에서 바꾼다.
- 가정: 텍스처는 BC1(`TC_Default`). 그림에 그라데이션 띠가 보이면 `UserInterface2D`로 바꾼다(메모리 118 MB).

---

## 8. 구현 기록 (2026-09-17)

사용자 결정: Q1 버스 컷씬 유지, Q2 엔딩 볼륨은 8구역 중앙에 임시로 작게, Q3 C++로, Q4 이름 맞춤, Q5 가이드 다시 보여 줌, Q6 타이틀 BGM 계속.

### 8.1 계획과 달라진 점
- **디자이너 위젯 이름.** `WBP_CutScene`에는 Image가 둘이었다: `bgImage`(검정 바탕, ColorAndOpacity 0,0,0,1)와 `ScaleBox` 안의 `Image`(그림).
  C++ `BindWidget`은 그림 쪽을 `pictureImage`로 잡고, 디자이너의 `Image`를 `pictureImage`로 이름 바꿨다. `bgImage`는 건드리지 않는다.
- **그림 종횡비.** `SetBrushFromTexture(…, bMatchSize = false)`면 브러시가 디자이너 기본(32×32)으로 남아 ScaleBox가 정사각형으로 늘렸다(PIE에서 확인).
  C++는 `bMatchSize = true`로, 디자이너 `pictureImage` 브러시 Image Size도 2560×1440으로 두었다.
- **MainUI 변수.** `PlayLevelName`(Name)을 지우고, `startBtn` → `PlayCutscene(Intro, NextLevel)` 노드의 **Next Level 핀 기본값**에
  `L_ToonSample_Tutorial`을 넣었다. 소프트 오브젝트 핀이라 하드 로드가 없다. 시작 레벨을 바꾸려면 그 핀만 고친다.
  (MCP `add_object_variable`은 하드 참조 변수만 만들어 월드를 통째로 로드하므로 쓰지 않았다.)
- **트리거 저장.** 첫 배치 뒤 `save_assets`가 true를 돌려줬지만 umap에 액터가 없었다(원인 불명). 다시 배치하고 저장해 파일에서 확인했다.
- **ESC 스킵**(P2)과 콘솔 `ltts.Cutscene`은 구현했다.

### 8.2 레벨·애셋에 넣은 값
| 대상 | 값 |
|---|---|
| `Content/UI/CutSceneData/UI_Open_1~4`, `UI_End_1~4` | 2560×1440, `TC_Default`, `TMGS_NoMipmaps`, `TEXTUREGROUP_UI`, NeverStream, sRGB |
| `DA_CutScene_Intro` / `DA_CutScene_Ending` | Frames 4장씩, Duration 4.0 |
| `DefaultGame.ini` | `CutsceneWidget`, `IntroCutscene`, `EndingCutscene` |
| `WBP_CutScene` | 부모 `CutSceneWidget`, 이벤트 그래프의 InitializeInt·skipBtn 노드 24개와 변수 2개 삭제, `Image` → `pictureImage` |
| `WBP_MainUI` | `startBtn` → `GetOwningPlayer` → `GetUIManagerSubsystem` → `PlayCutscene(Intro, L_ToonSample_Tutorial)` |
| `Subway_Stage2` `CutsceneCellTrigger_Ending`(폴더 Gameplay) | 위치 (−33510, 10770, 8472) = `StageZoneVolume8` 중심, 박스 150×150×100(3×3 셀), Kind Ending, NextLevel `Subway_Title` |

### 8.3 검증 결과 (PIE)
- 빌드 성공, 경고 0 (두 번: 클래스 추가, `pictureImage` 이름 변경).
- `Subway_Title`에서 `ltts.Cutscene Intro /Game/Maps/Main/L_ToonSample_Tutorial` → 4장 재생 → `Cutscene Intro ended; opening …Tutorial` → 튜토리얼 로드, `stage 0 'Tutorial'` 로그. 입력 모드 UI only → game and UI 복귀.
- `Subway_Stage2`에서 트리거를 PlayerStart 위로 옮겨 두고 PIE → `CutsceneCellTrigger_0: player entered cell (359,10)` → 엔딩 컷씬 → 스킵 → `Subway_Title` 로드, `TitleGameMode_0: WBP_MainUI_C shown`. 확인 후 트리거를 8구역 중앙으로 되돌렸다.
- 원래 위치의 트리거는 PIE 시작 로그 `cutscene ECutsceneKind::Ending on 9 cell(s)`로 셀 등록을 확인했다. **그 9칸이 걸어갈 수 있는 칸인지는 손으로 확인해야 한다**(Q2 임시 위치).
- 에디터가 백그라운드일 때 PIE 틱이 느려 4장(16초)에 44~50초가 걸렸다. 논리 순서에는 영향이 없다(BusStopCutscene.md 7절과 같은 현상).

### 8.4 남은 것
- 엔딩 볼륨 위치·크기는 기획이 정한다(지금은 8구역 중앙 3×3 셀).
- 프레임 시간(4초)·크로스페이드(0.3초)는 임시값.

### 8.5 버그: 타이틀 플레이 버튼 무반응 (2026-09-17 오후 수정)
- **증상.** 타이틀에서 플레이를 눌러도 아무 일이 없었다. 로그에 `[UIManager]` 줄이 하나도 없었다.
- **원인.** 에디터를 재시작하자 `WBP_MainUI`에서 `OnClicked(startBtn)` 이벤트와 `PlayCutscene` 노드가 통째로 사라져 있었다.
  기존 `Open Level` 노드는 8.1에서 지웠으므로 버튼에 아무 동작도 남지 않았다. 처음 MCP로 노드를 만들 때 `WBP_MainUI` 편집 탭이
  열려 있었고, 저장은 true를 돌려줬지만 재로드 뒤에는 노드가 없었다(정확한 원인은 확인하지 못했다).
  8.3의 확인은 같은 에디터 세션 안에서 그래프를 읽은 것이라 이 누락을 잡지 못했다.
- **수정.** 이벤트를 다시 바인딩하고 `GetOwningPlayer → GetUIManagerSubsystem → PlayCutscene(Intro, L_ToonSample_Tutorial)`를 다시 연결해
  컴파일·저장했다.
- **확인.** 에디터를 재시작해 새로 불러온 그래프에 연결이 남아 있음을 확인했다. PIE에서 뷰포트의 플레이 버튼을 실제로 클릭해
  `Cutscene Intro starts` → 4장 재생 → `Intro ended; opening …Tutorial` → `stage 0 'Tutorial'`까지 로그로 확인했다.
- **교훈.** 블루프린트 그래프를 MCP로 고친 뒤에는 에디터를 재시작해 재로드된 그래프로 확인한다. 파일 문자열 검사는 FName 번호가
  따로 저장되어 노드 유무를 판정할 수 없다.
