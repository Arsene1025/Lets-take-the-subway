# 사운드 시스템 — 폴리 적용과 마스터 볼륨

작성일 2026-09-15. 대상: `Source/LetsTakeTheSubway/Sound/`(신규), `UI/UIManagerSubsystem`, `Vehicle/GridTrain`,
`Puzzle/PuzzleElevatorBlock`·`PuzzleBlock`·`PuzzleRotationTile`·`PuzzleRotatingObstacle`·`PuzzleLever`·`PuzzleElevatorDock`,
`Player/GridPlayerController`·`GridTestGameMode`, `NPC/GridNPC`, `Grid/GridActor`, `Config/DefaultEngine.ini`·`DefaultGame.ini`,
`Content/Art/Sound/`(신규, 아트 소유)·`Content/Core/Sound/`(신규)·`Content/Design/Sound/`(신규).
입력 자료: `C:\Users\User\Downloads\Lets-take-the-subway_WAV_foley\` (WAV 15개 + `sound.png` 배치표).
선행 문서: `BusStopCutscene.md`(시퀀스 규칙), `../StageClearElevator.md`(클리어 훅), `RotatingObstacle.md`.

---

## 0. 왜

| 요청 | 현재 상태 |
|---|---|
| 폴리 WAV 15개를 배치표대로 게임에 넣는다 | 프로젝트에 사운드 코드가 한 줄도 없다. `Source`에 `USoundBase`·`UAudioComponent` 참조 0건, `Content`에 오디오 애셋 0개, `DefaultEngine.ini`에 `[AudioSettings]` 섹션 없음 |
| UI의 MasterVolume 슬라이더 하나로 전체 음량을 조절한다 | `WBP_MainUI`·`WBP_SideMenuUI`에 설정 패널(`settingPanel`, `UI_VOL_SLIDER`, `Volume` 슬라이더, `OpenSettingPanel`/`CloseSettingPanel`/`ApplySettings` 함수)이 이미 있고 `GetGameUserSettings`를 부른다(PROG_2, 커밋 264556c). 슬라이더 값이 어디로도 가지 않는다 |
| UIManager에 설정값을 받아 실제 음향을 바꾸는 함수를 둔다 | `UUIManagerSubsystem`은 위젯 열기·닫기만 한다 |
| 지금 없는 사운드(하차 안내 방송, 스테이지 클리어)를 나중에 코드 수정 없이 넣을 수 있어야 한다 | — |

## 1. 결정

1. **사운드는 이름(FName) 키로 부르고, 키 → 애셋 매핑은 데이터 애셋 하나(`DA_SoundLibrary`)가 갖는다.** 코드는 `Train.DoorOpen` 같은 키만 알고 애셋을 하드 참조하지 않는다(README의 Core → Art 참조 금지 규칙). 애셋이 없는 키는 조용히 건너뛰므로, 아직 없는 사운드(하차 방송, 클리어)의 키를 미리 박아 두고 파일이 오면 애셋만 끼운다.
2. **재생·볼륨은 `UGameSoundSubsystem`(`UGameInstanceSubsystem`) 하나가 맡는다.** 스테이지 1 → 버스 정류장 컷씬 → 스테이지 2로 레벨이 바뀌어도 살아남아야 마스터 볼륨이 유지된다(`UStageSubsystem` 헤더의 "레벨을 넘어 남는 것은 GameInstance 서브시스템에" 규칙과 같다).
3. **마스터 볼륨은 SoundClass 트리 + SoundMix 오버라이드로 건다.** 루트 `SC_Master` 아래 `SC_SFX`·`SC_Ambience`·`SC_UI`·`SC_Music`을 둔다. 지금은 슬라이더가 하나라 루트만 만지지만, 나중에 카테고리 슬라이더를 추가할 때 애셋을 다시 분류하지 않아도 된다.
4. **저장은 `UGameUserSettings` 서브클래스(`ULTTSGameUserSettings`)에 `MasterVolume` Config 프로퍼티를 더해서 한다.** 위젯 BP가 이미 `GetGameUserSettings`·`ApplySettings`를 쓰고 있고, 엔진이 `Saved/Config/.../GameUserSettings.ini`에 알아서 써 준다. SaveGame을 새로 만들지 않는다.
5. **UIManager는 슬라이더 값을 받아 사운드 서브시스템에 넘기는 창구만 한다**(`ApplyMasterVolume` / `GetMasterVolume` / `CommitSettings`). UI 코드는 오디오 API를 직접 만지지 않는다.
6. **발동 지점은 각 게임플레이 액터의 상태 전이 함수에 한 줄씩 넣는다.** 별도의 "사운드 이벤트 버스"를 만들지 않는다. 전이 함수(`EnterMoving`, `EnterDoorsOpening`, `StartSlide`, `TryRotate` 커밋 …)가 이미 유일한 진실이라 거기서 부르는 것이 가장 짧고 어긋나지 않는다.
7. **각 액터는 자기가 내는 사운드의 키를 `EditAnywhere FName`으로 갖는다.** 기본값은 표준 키. 디자이너가 특정 인스턴스(큰 회전 장애물 vs 1x1 기둥)만 다른 소리로 바꾸고 싶으면 키를 바꾸고 라이브러리에 항목을 더하면 되고, 코드는 손대지 않는다.
8. **개찰구 소리는 새 저작 액터 `ASoundCellTrigger`로 낸다.** 행인(`AGridNPC`)은 콜리전이 없어 볼륨 오버랩이 안 되고, 셀 규칙(`UGridCellRule`)은 부수 효과가 금지되어 있다. 그리드에 "폰이 셀에 들어왔다" 방송을 하나 더하고 트리거가 그것을 듣는다.
9. **BGM은 이번 범위 밖**이지만 `SC_Music`과 `PlayMusic` 슬롯 자리를 남긴다(4절).
10. 버스 정류장 컷씬의 문·주행 소리는 코드가 아니라 **시퀀스의 Audio Track**으로 아트가 넣는다(4절). 이번 표에는 지하철 소리만 있으므로 구현 범위에서 제외한다.

## 2. 입력 자료 실측

### 2.1 WAV 파일 (헤더에서 읽음)

| 파일 | 채널 / 샘플레이트 / 비트 | 길이 | 비고 |
|---|---|---|---|
| WAV_01_SubwaySound | 2ch 48 kHz 24bit | 9.91 s | 열차 진입 (원샷) |
| WAV_02_SubwayStop | 2ch 48 kHz 24bit | 2.31 s | "고오오오 착" — 정차 직전에 시작해야 끝이 맞는다 |
| WAV_03_TrainNotification | 2ch 44.1 kHz 16bit | 2.41 s | 승강장 알림음 |
| WAV_04_ | — | — | **파일 없음.** 열차 하차 안내 방송. 키만 예약 |
| WAV_05_SubwayInside | 2ch 44.1 kHz 16bit | 11.71 s | 탑승 중 루프 |
| WAV_06_SubwayDoorOpen | 2ch 44.1 kHz 16bit | 1.00 s | 문 열림 |
| WAV_07_SubwayDoorClose | 2ch 44.1 kHz 16bit | 1.47 s | 문 닫힘 |
| WAV_08_EVmoving | 2ch 44.1 kHz 16bit | 4.01 s | 엘리베이터 이동 루프 |
| WAV_09_EVarrived | 2ch 44.1 kHz 16bit | 1.58 s | 엘리베이터 도착 |
| WAV_10_TurnstileCard | 2ch 44.1 kHz 16bit | 0.22 s | 개찰구 삑 |
| WAV_11_Click | 2ch 44.1 kHz 16bit | 0.25 s | 클릭 |
| WAV_12_PuzzleDrag | 2ch 44.1 kHz 16bit | 0.44 s | 드래그 한 칸 |
| WAV_13_Jamming(EPIC STOCK MEDIA Sound Effects) | 2ch **192 kHz** 24bit | 0.41 s | 파일 이름에 괄호·공백 → 임포트 전 이름 변경. 192 kHz는 48 kHz로 리샘플 권장(용량·디코딩) |
| WAV_14_ElevatorSpin | 2ch 44.1 kHz 16bit | 0.34 s | 회전판 (RotateDuration 0.4 s와 근접) |
| WAV_15_PillarSpin | 2ch 44.1 kHz 16bit | 1.12 s | 기둥 회전 (RotateDuration 0.6 s보다 길다 — 여운으로 허용) |
| WAV_16_PillarJamming | 2ch 44.1 kHz 16bit | 1.18 s | 기둥 회전 실패 |
| (없음) | — | — | 목적지 도착 / 스테이지 클리어. 키만 예약 |

프로젝트 오디오 설정은 48 kHz(`AudioSampleRate=48000`)다. 44.1 kHz 파일은 엔진이 런타임에 리샘플하므로 그대로 넣어도 되지만, 아트가 일괄 48 kHz로 맞춰 주면 가장 깨끗하다.

### 2.2 배치표(`sound.png`) 요약

- **인게임 세상 속 소리** (01~10): 열차 진입은 "왼쪽에서 오른쪽으로 들어온다"는 공간감. 알림음은 열차가 화면에 들어오기 직전. 문 소리는 애니메이션과 싱크. 엘리베이터 이동·도착. 개찰구는 행인이 지날 때.
- **게임적 연출 소리** (11~16 + 클리어): 클릭, 드래그(슬라이드), 장애물끼리 접한 채 드래그(막힘), 회전판, 기둥 회전, 기둥 회전 막힘, 스테이지 클리어.

### 2.3 코드의 발동 지점 후보 (읽은 것)

| 사건 | 지금 코드 |
|---|---|
| 열차 출발 / 주행 / 정차 → 문 열림 → 문 닫힘 | `AGridTrain::EnterMoving`(786) → `Tick` Moving(1024~1048, `MoveDistance/MoveLength`) → `EnterDoorsOpening`(828) → `EnterDoorsOpen`(842, `OnDoorsOpened` 방송) → `EnterDoorsClosing`(977) |
| 플레이어 탑승 | `AGridTrain::TryBoard` → `Rider = Pawn`(1213). 하차는 `EnterDoorsOpen`에서 `Rider.Reset()`(972) |
| 엘리베이터 승강 | `APuzzleElevatorBlock::Tick` `WaitingForRider → Moving`(407) → 목표 도달 시 `Holding`(427, `OnHoldReached`) 또는 `Dwelling`(439) → `FinishTravel`(`OnArrived`) |
| 블록 한 칸 슬라이드 | `APuzzleBlock::StartSlide`(512), 이어 밀기는 `Tick`(622)에서 다시 `StartSlide` |
| 드래그 막힘 | `AGridPlayerController::UpdateDrag` 거부 분기(1375~1383, 방향당 한 번만 `ShowFeedback`) |
| 회전판 회전 | `APuzzleRotationTile::TryRotate` 커밋 패스(BeginRotation 루프 뒤 `bRotating = true`) |
| 기둥·장애물 회전 | `APuzzleRotatingObstacle::TryRotate` 커밋 패스(`BeginRotation(Pivot, …)`) — 기둥은 이 클래스를 상속 |
| 레버 회전 실패 | `APuzzleLever::TryTurn`(159) → `Target->TryRotate` false. 컨트롤러 `UpdateLeverDrag`(1219)가 사유를 띄운다 |
| 클릭 | `AGridPlayerController::OnPressed`(521) |
| 행인 셀 진입 | `AGridNPC::Tick` `CurrentCell = NextCell`(526). **그리드에 알리지 않는다** |
| 플레이어 셀 진입 | `AGridPawn` → `AGridActor::NotifyPawnEnteredCell`(GridActor.cpp:401) — StageClear 셀이면 `OnStageClear` 방송 |
| 스테이지 클리어 | `AGridTestGameMode::HandleStageClear`, `APuzzleElevatorDock::HandleHoldReached`(618) → `OnClearCutscene` |

## 3. 설계

### 3.1 구조

```
[위젯 BP: Volume 슬라이더] --OnValueChanged--> UUIManagerSubsystem::ApplyMasterVolume(v)
                                                         |
[위젯 BP: 패널 닫기/확인] ----------------------------> UUIManagerSubsystem::CommitSettings()
                                                         v
                                  UGameSoundSubsystem (GameInstance)
                                    ├─ SetMasterVolume(v): SMix_Master 위 SC_Master 오버라이드
                                    ├─ Save(): ULTTSGameUserSettings.MasterVolume → GameUserSettings.ini
                                    ├─ Play2D(Key) / PlayAtLocation(Key, Loc) / PlayAttached(Key, Comp, bLoop)
                                    └─ USoundLibrary(DA_SoundLibrary): Key → SoundWave, 볼륨·피치·감쇠
                                                         ^
   AGridTrain, APuzzleElevatorBlock, APuzzleBlock, APuzzleRotationTile, APuzzleRotatingObstacle,
   APuzzleLever, AGridPlayerController, AGridTestGameMode, APuzzleElevatorDock, ASoundCellTrigger
   (각자 EditAnywhere FName 키를 들고 상태 전이에서 한 줄씩 부른다)
```

### 3.2 애셋과 폴더

| 경로 | 소유 | 내용 |
|---|---|---|
| `Content/Art/Sound/SFX/` | 아트 | 임포트한 SoundWave. 이름은 배치표 번호를 살려 `SW_01_SubwaySound` … `SW_16_PillarJamming`(접두 `WAV_` → `SW_`, 13번은 괄호 제거). 임포트 시 **Sound Class**를 아래 표대로 지정, 루프 파일은 **Looping** 켬 |
| `Content/Core/Sound/` | 프로그래머 | `SC_Master`, `SC_SFX`, `SC_Ambience`, `SC_UI`, `SC_Music`(SoundClass), `SMix_Master`(SoundMix), `ATT_World`(SoundAttenuation, 3.6절) |
| `Content/Design/Sound/` | 기획 | `DA_SoundLibrary`(USoundLibrary). 키 → 애셋 매핑. 기획·아트가 직접 편집한다 |

SoundClass 배정: 01·02·03·05·06·07·08·09·10 → `SC_Ambience`(세상 속 소리), 11 → `SC_UI`, 12·13·14·15·16·클리어 → `SC_SFX`. 지금은 전부 `SC_Master`의 자식이라 슬라이더 하나가 다 잡는다.

SoundCue나 MetaSound는 만들지 않는다. 볼륨·피치 배율은 라이브러리 항목이 갖고, 루프 여부는 SoundWave의 Looping 플래그로 정한다. 랜덤 변주가 필요해지면 그때 항목 하나를 SoundCue로 바꾸면 된다(라이브러리는 `USoundBase`를 받는다).

### 3.3 키와 라이브러리 (`Sound/SoundLibrary.h`, `Sound/SoundKeys.h`)

```cpp
USTRUCT(BlueprintType)
struct FSoundLibraryEntry
{
    UPROPERTY(EditAnywhere) TSoftObjectPtr<USoundBase> Sound;          // 비어 있으면 그 키는 아직 없는 소리
    UPROPERTY(EditAnywhere, meta=(ClampMin=0)) float Volume = 1.0f;
    UPROPERTY(EditAnywhere, meta=(ClampMin=0.1)) float Pitch = 1.0f;
    UPROPERTY(EditAnywhere) TSoftObjectPtr<USoundAttenuation> Attenuation;  // 3D 재생 때만. 비우면 SoundSettings의 기본
    UPROPERTY(EditAnywhere) float MinRetriggerSeconds = 0.0f;           // 같은 키를 이 간격 안에 다시 부르면 무시 (클릭·삑 연타 방지)
};

UCLASS()
class USoundLibrary : public UDataAsset
{
    UPROPERTY(EditAnywhere, meta=(ForceInlineRow)) TMap<FName, FSoundLibraryEntry> Entries;
};
```

`SoundKeys.h`는 코드가 쓰는 키의 정본이다. 오타를 막기 위한 상수이지 enum이 아니다 — 기획이 BP나 트리거에서 코드에 없는 새 키를 써도 된다.

| 키 | 파일 | 분류 |
|---|---|---|
| `Train.Notification` | 03 | Ambience |
| `Train.Approach` | 01 | Ambience (3D, 열차에 부착) |
| `Train.Stop` | 02 | Ambience (3D, 열차에 부착) |
| `Train.DoorOpen` / `Train.DoorClose` | 06 / 07 | Ambience (3D, 열차에 부착) |
| `Train.Inside` | 05 (루프) | Ambience (2D) |
| `Train.ArrivalAnnouncement` | **04 없음** | Ambience — 키만 예약 |
| `Elevator.Moving` | 08 (루프) | Ambience (3D, 엘리베이터에 부착) |
| `Elevator.Arrived` | 09 | Ambience (3D) |
| `World.Turnstile` | 10 | Ambience (`ASoundCellTrigger`) |
| `UI.Click` | 11 | UI (2D) |
| `Puzzle.Slide` | 12 | SFX (3D, 블록에 부착) |
| `Puzzle.Jam` | 13 | SFX |
| `Puzzle.TileRotate` | 14 | SFX |
| `Puzzle.PillarRotate` | 15 | SFX |
| `Puzzle.PillarJam` | 16 | SFX |
| `Stage.Clear` | **없음** | SFX — 키만 예약 |

`DA_SoundLibrary`에는 예약 키도 Sound가 빈 채로 넣어 둔다. 그래야 기획이 "어떤 소리가 비어 있는지"를 애셋에서 바로 본다.

### 3.4 `UGameSoundSubsystem` (`Sound/GameSoundSubsystem.h/.cpp`)

| 함수 | 뜻 |
|---|---|
| `static Get(WorldContext)` | 다른 서브시스템과 같은 관례 |
| `Initialize` | `USoundSettings`에서 라이브러리·믹스·클래스를 동기 로드, 항목의 SoundWave를 **비동기 일괄 프리로드**(전부 합쳐 10 MB 미만). `PostLoadMapWithWorld`에 저장된 볼륨을 다시 거는 핸들러를 건다 |
| `PlaySound2D(FName Key)` | `UGameplayStatics::PlaySound2D`. UI·알림·"안에서 듣는" 소리 |
| `PlaySoundAtLocation(FName Key, FVector)` | 한 번 나고 끝나는 3D 소리 |
| `UAudioComponent* PlayAttached(FName Key, USceneComponent* Attach)` | 움직이는 것에 붙이는 소리(열차, 엘리베이터, 블록). 루프는 SoundWave의 Looping으로 정해지고, 호출자가 컴포넌트를 들고 있다가 `StopAttached(Comp, FadeOut)`로 끝낸다 |
| `UAudioComponent* PlayLoop2D(FName Key)` / `StopLoop(Comp, FadeOut)` | 열차 안 소리처럼 위치 없는 루프 |
| `SetMasterVolume(float 0..1)` / `GetMasterVolume()` | 즉시 적용(믹스 오버라이드, 페이드 0.1 s). 디스크에 쓰지 않는다 |
| `SaveSettings()` | `ULTTSGameUserSettings::MasterVolume`에 쓰고 `SaveSettings()` |
| `ResolveEntry(Key, Entry&)` | 라이브러리 조회. 키가 없으면 Warning 한 번(키당), Sound가 비어 있으면 Verbose 한 번. 둘 다 재생하지 않고 false |

전부 `BlueprintCallable`이다. 위젯이나 아트 BP가 `UI.Click`을 직접 부를 수 있어야 하고(4절), 콘솔 `ltts.Sound Play <Key>` / `ltts.Sound Volume <0..1>`이 검증용으로 같은 함수를 탄다(프로젝트의 `ltts.*` 관례).

재생 시 볼륨은 `Entry.Volume`만 넘긴다. 마스터 볼륨은 믹스가 곱하므로 여기서 다시 곱하지 않는다(두 번 곱하면 슬라이더가 제곱으로 듣는다).

`USoundSettings`(`Sound/SoundSettings.h`, `UDeveloperSettings`, Config=Game — `UUISettings`와 같은 틀):

| 프로퍼티 | 값 |
|---|---|
| `Library` | `/Game/Design/Sound/DA_SoundLibrary` |
| `MasterMix` | `/Game/Core/Sound/SMix_Master` |
| `MasterClass` | `/Game/Core/Sound/SC_Master` |
| `DefaultAttenuation` | `/Game/Core/Sound/ATT_World` |
| `DefaultMasterVolume` | 1.0 (저장된 값이 없을 때) |

### 3.5 마스터 볼륨

**클래스·믹스.** `SC_Master`(루트) → `SC_SFX`, `SC_Ambience`, `SC_UI`, `SC_Music`. `SMix_Master`는 비어 있는 믹스이고 `DefaultEngine.ini`의 `DefaultBaseSoundMix`로 등록해 항상 켜져 있게 한다. 런타임은 `UGameplayStatics::SetSoundMixClassOverride(World, SMix_Master, SC_Master, Volume, 1.0f, 0.1f, /*bApplyToChildren*/ true)` 한 줄이다. `DefaultSoundClassName=SC_Master`도 함께 걸어, 아트가 클래스 지정을 잊은 웨이브도 슬라이더 아래에 들어오게 한다.

**저장.** `ULTTSGameUserSettings : UGameUserSettings`(`Sound/LTTSGameUserSettings.h`):

```cpp
UPROPERTY(Config, BlueprintReadWrite) float MasterVolume = 1.0f;
```

`DefaultEngine.ini` `[/Script/Engine.Engine] GameUserSettingsClassName=/Script/LetsTakeTheSubway.LTTSGameUserSettings`. 위젯 BP의 기존 `GetGameUserSettings` 노드는 그대로 두어도 되고(부모 타입으로 받는다), 값 읽기는 UIManager를 통한다.

**UIManager.** `UUIManagerSubsystem`에 다음을 더한다. 로컬 플레이어 서브시스템이므로 `GetLocalPlayer()->GetGameInstance()->GetSubsystem<UGameSoundSubsystem>()`로 닿는다.

| 함수 | 위젯 BP가 부르는 때 |
|---|---|
| `float GetMasterVolume() const` (BlueprintPure) | `OpenSettingPanel`에서 슬라이더 초기값 |
| `void ApplyMasterVolume(float Volume01)` | 슬라이더 `OnValueChanged` — 드래그 중 즉시 들린다 |
| `void CommitSettings()` | `CloseSettingPanel` / `ApplySettings` — 디스크에 쓴다 |

슬라이더가 매 프레임 값을 보내도 디스크는 닫을 때 한 번만 쓴다. 슬라이더 값 0~1은 선형 볼륨으로 그대로 쓴다. 귀에 너무 급하게 들리면 `ApplyMasterVolume` 안에서 `Volume01 * Volume01`로 바꾸는 것만으로 조정되고, 저장값은 슬라이더 값 그대로라 UI가 흔들리지 않는다.

위젯 쪽 작업(PROG_2 소유, 노드 세 개): 슬라이더 `OnValueChanged` → `ApplyMasterVolume`, `OpenSettingPanel` 첫 줄 → `GetMasterVolume`으로 슬라이더 값 세팅, `CloseSettingPanel` → `CommitSettings`. `WBP_MainUI`와 `WBP_SideMenuUI` 둘 다 같은 패널을 갖고 있으므로 둘 다 손본다.

### 3.6 사운드별 발동 지점

3D 재생은 전부 `ATT_World` 감쇠를 쓴다. 카메라가 아이소메트릭이라 리스너(= 현재 뷰 타깃인 구역 카메라)가 승강장에서 멀다. 감쇠 반경은 **안쪽 3000 cm / 바깥 12000 cm**에서 시작해 PIE에서 조정한다. 열차의 "왼쪽에서 오른쪽" 공간감은 열차 액터에 컴포넌트를 붙이는 것만으로 나온다 — 리스너가 카메라라 화면 좌우와 팬이 일치한다. 구역 카메라가 없어 폰 카메라를 쓰는 레벨에서는 팬이 폰 기준이 되지만 허용한다.

| 키 | 어디서 | 방식 | 세부 |
|---|---|---|---|
| `Train.Notification` | `AGridTrain::Tick` Moving | 2D 원샷 | 남은 거리(`MoveLength - MoveDistance`)가 `NotificationDistance`(기본 **0 = 출발 즉시**) 이하가 되는 첫 프레임. 정차역마다 `FGridTrainStop::bArrivalSounds`(기본 true)로 끌 수 있다 — 화면 밖 회차역에서 알림음이 나면 안 된다 |
| `Train.Approach` | 같은 곳 | 열차 부착 원샷 | 남은 거리가 `ApproachSoundDistance`(기본 0 = 출발 즉시) 이하일 때. 9.9 s짜리 원샷이라 구간이 길면 도착 전에 끝난다. Stage1 구간(50 m, 약 8 s)에는 0이 맞고, 긴 구간은 값을 올려 "화면에 잡히는 지점"에 맞춘다. `EnterDoorsOpening`에서 아직 나고 있으면 0.3 s 페이드로 끊는다 |
| `Train.Stop` | 같은 곳 | 열차 부착 원샷 | 남은 거리가 `StopSoundDistance`(기본 **1500 cm**) 이하일 때. 감속 구간(`DecelFraction` 0.15 × 50 m ≈ 7.5 m)과 클립 2.3 s를 맞추기 위한 값이며 PIE에서 조정 |
| `Train.DoorOpen` | `EnterDoorsOpening` | 열차 부착 원샷 | `bMatchDoorTimingToAnimation`이 열림 시간을 애니메이션 길이에 맞추므로 단계 시작에 틀면 싱크가 맞는다 |
| `Train.DoorClose` | `EnterDoorsClosing` | 열차 부착 원샷 | 같음 |
| `Train.Inside` | `EnterMoving`에서 `Rider`가 있으면 `PlayLoop2D`; `EnterDoorsOpening`에서 `StopLoop`(0.5 s 페이드) | 2D 루프 | 첫 역 `Idle → DoorsOpening`에는 Rider가 없으므로 나지 않는다. `EndPlay`에서도 끊는다 |
| `Train.ArrivalAnnouncement` | `EnterDoorsOpening`, `Rider`가 있을 때만 | 2D 원샷 | 파일이 오면 라이브러리만 채운다 |
| `Elevator.Moving` | `APuzzleElevatorBlock::Tick` `WaitingForRider → Moving` 전이에서 `PlayAttached`(루프); 목표 도달 분기(`Holding`/`Dwelling` 직전)에서 `StopAttached`(0.2 s) | 부착 루프 | `FinishTravel`·`EndPlay`에서도 끊는다(콘솔로 중단될 때) |
| `Elevator.Arrived` | 같은 목표 도달 분기 | 부착 원샷 | 클리어 승강(Holding)에도 난다. 원치 않으면 `bHoldAtTarget`일 때 건너뛰는 분기 하나 |
| `World.Turnstile` | `ASoundCellTrigger`(3.7) | 위치 원샷 | 트리거의 `MinRetriggerSeconds` 0.15 s — 행인 무리가 한 번에 지나면 삑이 겹친다 |
| `UI.Click` | `AGridPlayerController::OnPressed` 첫 줄 | 2D 원샷 | 월드 클릭. **UMG 버튼**은 코드 없이 버튼 스타일의 Pressed Sound에 `SW_11_Click`을 넣는다(SoundClass `SC_UI`라 슬라이더가 잡는다) |
| `Puzzle.Slide` | `APuzzleBlock::StartSlide` 성공 직후 | 블록 부착 원샷 | 이어 밀기는 `Tick`이 `StartSlide`를 다시 부르므로 칸마다 한 번 — 배치표의 "드르륵". 너무 잦으면 라이브러리 `MinRetriggerSeconds`로 솎는다 |
| `Puzzle.Jam` | `UpdateDrag` 거부 분기(`ShowFeedback(Reason)` 옆) | 블록 부착 원샷 | 이미 방향당 한 번으로 걸러져 있어 연타되지 않는다 |
| `Puzzle.TileRotate` | `APuzzleRotationTile::TryRotate` 커밋 패스 끝 | 타일 위치 원샷 | 검사 패스에서 거부되면 나지 않는다 |
| `Puzzle.PillarRotate` | `APuzzleRotatingObstacle::TryRotate` 커밋 패스 | 장애물 부착 원샷 | 기둥·큰 장애물 공통. 인스턴스별 키 오버라이드로 갈라진다(3.8) |
| `Puzzle.PillarJam` | `APuzzleLever::TryTurn`에서 `Target->TryRotate`가 false를 돌려줄 때 | 장애물 위치 원샷 | "옆에 서라"·"이 휠은 한쪽만"처럼 레버 자체의 거부에는 나지 않는다 — 걸린 것은 장애물이다 |
| `Stage.Clear` | `AGridTestGameMode::HandleStageClear`와 `APuzzleElevatorDock::HandleHoldReached`(`OnClearCutscene` 방송 직전) | 2D 원샷 | 게임 모드는 레벨에 `bClearOnStageClear`를 켠 구조물이 있으면 내지 않는다. 그 레벨에서 셀은 출구가 열렸다는 뜻이고 클리어 소리는 구조물이 엔딩 승강을 마칠 때 난다 |

### 3.7 `ASoundCellTrigger` (`Sound/SoundCellTrigger.h/.cpp`)

| 프로퍼티 | 뜻 |
|---|---|
| `Box` (UBoxComponent, 에디터 표시용) | 덮는 영역. BeginPlay에서 그리드에 물어 셀 목록으로 바꾼다(`AGridActor::WorldToCell`) |
| `SoundKey` | 기본 `World.Turnstile` |
| `bTriggerOnNPC` / `bTriggerOnPlayer` | 기본 true / true |
| `bSpatial` | true면 들어선 폰의 위치에서 3D, false면 2D |
| `MinRetriggerSeconds` | 라이브러리 값과 별개로 트리거 자체의 간격 |

그리드에 방송을 하나 더한다: `AGridActor::OnPawnEnteredCell(APawn*, FIntPoint)`. `NotifyPawnEnteredCell`(GridActor.cpp:401)이 StageClear 판정 전에 이것을 먼저 방송하고, `AGridNPC::Tick`의 `CurrentCell = NextCell`(526) 뒤에 `Grid->NotifyPawnEnteredCell(this, NextCell)`을 부른다. StageClear 처리는 이미 `AGridPawn` 캐스트로 행인을 거르므로(`GridTestGameMode.cpp`, `PuzzleElevatorDock.cpp:602`) 행인이 부르기 시작해도 클리어가 오작동하지 않는다.

트리거는 개찰구 한 줄에 박스 하나로 덮는다. Stage2의 `SM_L1_TicketGate*` 열이 대상이다.

### 3.8 액터별 키 오버라이드 (확장 패턴)

각 액터에 `Category = "Sound"`로 키 프로퍼티를 둔다. 기본값은 `SoundKeys.h`의 표준 키.

| 액터 | 프로퍼티 |
|---|---|
| `AGridTrain` | `FTrainSoundKeys { Notification, Approach, Stop, DoorOpen, DoorClose, Inside, ArrivalAnnouncement }` + `NotificationDistance`, `ApproachSoundDistance`, `StopSoundDistance` |
| `FGridTrainStop` | `bArrivalSounds` |
| `APuzzleElevatorBlock` | `MovingSoundKey`, `ArrivedSoundKey` |
| `APuzzleBlock` | `SlideSoundKey`, `JamSoundKey` |
| `APuzzleRotationTile` | `RotateSoundKey` |
| `APuzzleRotatingObstacle` | `RotateSoundKey`, `JamSoundKey` (기둥은 상속) |
| `AGridPlayerController` | `ClickSoundKey` |
| `APuzzleElevatorDock` / `AGridTestGameMode` | `ClearSoundKey` |

키를 `None`으로 두면 그 인스턴스는 소리를 내지 않는다.

### 3.9 Config

`DefaultEngine.ini`:

```ini
[/Script/Engine.Engine]
GameUserSettingsClassName=/Script/LetsTakeTheSubway.LTTSGameUserSettings

[/Script/Engine.AudioSettings]
DefaultSoundClassName=/Game/Core/Sound/SC_Master.SC_Master
DefaultBaseSoundMix=/Game/Core/Sound/SMix_Master.SMix_Master
```

`DefaultGame.ini`:

```ini
[/Script/LetsTakeTheSubway.SoundSettings]
Library=/Game/Design/Sound/DA_SoundLibrary.DA_SoundLibrary
MasterMix=/Game/Core/Sound/SMix_Master.SMix_Master
MasterClass=/Game/Core/Sound/SC_Master.SC_Master
DefaultAttenuation=/Game/Core/Sound/ATT_World.ATT_World
```

`LetsTakeTheSubway.Build.cs`: 추가 모듈 없음(`Engine`에 오디오가 있다). `AudioMixer`는 필요 없다.

## 4. 나중에 사운드를 더하는 방법

| 상황 | 할 일 | 코드 |
|---|---|---|
| 예약된 키에 파일이 왔다 (04 하차 방송, 클리어) | `Content/Art/Sound/SFX/`에 임포트, SoundClass 지정, `DA_SoundLibrary`의 그 키에 애셋 연결 | 없음 |
| 기존 액터에 다른 소리를 쓰고 싶다 (큰 장애물은 다른 회전음) | 라이브러리에 새 키 추가, 그 액터 인스턴스의 키 프로퍼티를 바꾼다 | 없음 |
| 특정 셀을 지날 때 소리 (개찰구 외: 에스컬레이터 입구, 출구 계단) | `ASoundCellTrigger` 배치, 키 지정 | 없음 |
| UI 위젯 소리 | 버튼 스타일 Pressed/Hovered Sound, 또는 BP에서 `GameSoundSubsystem::PlaySound2D(Key)` | 없음 |
| 컷씬(버스) 소리 | 레벨 시퀀스에 Audio Track. 이 트랙의 사운드도 SoundClass가 `SC_Master` 아래면 슬라이더가 잡는다 | 없음 |
| 새 게임플레이 사건 (예: 폰이 벽에 부딪힘 `Bump`) | `SoundKeys.h`에 키 추가, 그 액터에 키 프로퍼티와 호출 한 줄 | 세 줄 |
| BGM | `SC_Music` 아래 웨이브, `UGameSoundSubsystem::PlayMusic(Key, CrossfadeSeconds)`(2D 루프 슬롯 하나, 레벨을 넘어 이어짐) | 함수 하나 |
| 카테고리 슬라이더 (효과음/배경음 분리) | `ULTTSGameUserSettings`에 값 추가, `SetSoundMixClassOverride`를 `SC_SFX`·`SC_Ambience`에 한 번씩 더 | 각 몇 줄 |

## 5. 구현 순서

각 단계가 끝나면 빌드가 되고 PIE가 돈다. 커밋은 사용자가 지시할 때만 한다.

1. **기반 (코드만, 소리는 아직 없음)**
   `Sound/SoundKeys.h`, `SoundLibrary.h`, `SoundSettings.h/.cpp`, `LTTSGameUserSettings.h/.cpp`, `GameSoundSubsystem.h/.cpp`, 콘솔 `ltts.Sound`. `DefaultEngine.ini`·`DefaultGame.ini`. 빌드 확인.
2. **애셋** (에디터 MCP가 살아 있으면 HTTP로, 아니면 에디터에서 손으로)
   `Content/Core/Sound/`에 SoundClass 5개·SoundMix·Attenuation. WAV 15개 임포트 → `Content/Art/Sound/SFX/`(13번 이름 변경), SoundClass·Looping 지정. `Content/Design/Sound/DA_SoundLibrary` 생성, 17개 키 등록(예약 2개는 빈 채로).
   `ltts.Sound Play UI.Click`으로 재생 확인, `ltts.Sound Volume 0.2`로 믹스 확인.
3. **UIManager + 위젯** — `GetMasterVolume` / `ApplyMasterVolume` / `CommitSettings`. 위젯 BP 노드 3개(`WBP_MainUI`, `WBP_SideMenuUI`). 게임 재시작 후 값 유지 확인(`Saved/Config/Windows/GameUserSettings.ini`).
4. **열차** — `AGridTrain` 키 구조체·거리 프로퍼티, `EnterMoving`/`Tick`/`EnterDoorsOpening`/`EnterDoorsClosing`/`EndPlay`. Stage1에서 진입·정차·문·탑승 루프 확인, 거리값 조정.
5. **퍼즐** — `PuzzleBlock::StartSlide`, 컨트롤러 `UpdateDrag` 거부, `RotationTile::TryRotate`, `RotatingObstacle::TryRotate`, `PuzzleLever::TryTurn`, `OnPressed`. Stage1 퍼즐로 확인.
6. **엘리베이터·클리어** — `PuzzleElevatorBlock::Tick` 이동·도착, `HandleStageClear`·`HandleHoldReached`.
7. **개찰구** — `AGridActor::OnPawnEnteredCell`, `AGridNPC` 알림, `ASoundCellTrigger`. Stage2 개찰구 열에 배치.
8. 문서 갱신: 이 문서 2절에 실측값(감쇠 반경, 거리값), README 폴더 표에 `Content/*/Sound/` 추가.

## 6. 검증

- `ltts.Sound Play <Key>`: 라이브러리 17개 키 전부 돌려 빈 키 2개만 Verbose 로그가 남는지.
- `au.Debug.Sounds 1`(엔진): 활성 사운드 목록에서 클래스가 `SC_*`인지, 마스터 0일 때 볼륨이 0인지.
- 슬라이더: 드래그 중 즉시 반영, 패널 닫기 → 재시작 → 값 유지. `MasterVolume=0`에서 완전 무음.
- 레벨 전환: Stage1 → BusStop_Start → BusStop_Arrived → Stage2를 지나도 볼륨 유지(GameInstance 서브시스템 + `PostLoadMapWithWorld` 재적용).
- 열차: Stage1 정차역에서 알림 → 진입 → 정차 → 문 열림 → (탑승) → 문 닫힘 → 안 소리 → 다음 역 문 열림에서 안 소리 끊김. 팬이 화면 좌우와 맞는지.
- 퍼즐: 한 칸 밀기마다 드래그음, 벽에 대고 밀면 막힘음 한 번, 회전판·기둥·기둥 막힘.
- 엘리베이터: 이동 루프가 도착에서 끊기고 도착음. 콘솔 `ltts.ElevatorRide`로도 같음.
- 개찰구: Stage2에서 행인이 게이트 열을 지날 때 삑, 무리가 지나도 겹침이 `MinRetriggerSeconds`로 눌리는지.
- 패키징: `DirectoriesToAlwaysCook`은 필요 없다 — 라이브러리가 `USoundSettings`에서 참조되고 웨이브는 라이브러리가 참조하므로 쿠커가 따라간다. 단 `DefaultBaseSoundMix`·`DefaultSoundClassName`은 ini 참조라 쿡되므로 별도 조치 없음.

## 7. 확인이 필요한 것

1. **볼륨 곡선**: 선형(기본)으로 시작한다. 낮은 구간이 너무 급하면 제곱으로 바꾼다.
2. **`UI.Click` 범위**: 월드 클릭 전부(빈 바닥 포함)에 낸다고 가정했다. 탈것·블록 클릭에만 내고 싶으면 `OnPressed`에서 픽 종류로 거른다.
3. **`Train.Approach` 시작점**: 기본 "출발 즉시"다. Stage2처럼 열차가 화면 밖 먼 곳에서 출발하는 구간은 `ApproachSoundDistance`를 올려야 한다. PIE에서 잡는다.
4. **첫 역**: 레벨 시작 시 열차는 이미 서 있다가 문만 연다(`StartDelaySeconds`). 알림·진입음 없이 문 소리만 나는 것이 맞는지.
5. **`Elevator.Arrived`가 클리어 승강(허공 정지)에도 나는지**: 기본 난다. 컷씬이 이어지므로 빼는 게 나을 수 있다.
6. **13번 파일**의 "EPIC STOCK MEDIA" 표기: 라이선스 확인은 아트 몫. 애셋 이름에서는 뺀다.

## 8. 이번 범위에서 뺀 것

- BGM, 카테고리별 슬라이더(자리만 남김, 4절).
- 버스 컷씬 소리(시퀀스 Audio Track, 아트).
- 에스컬레이터·폰 부딪힘(`Bump`) 소리 — 배치표에 없다. 키 추가 절차는 4절.
- 사운드 오클루전·리버브 플러그인 — `DefaultEngine.ini`의 `ReverbPlugin=` 등은 비워 둔 채 둔다.

## 9. 구현 현황 (2026-09-15)

| 항목 | 결과 |
|---|---|
| C++ 기반 | `Sound/` 신규 9개 파일: `SoundKeys.h`, `SoundLibrary.h`, `SoundSettings.h`, `LTTSGameUserSettings.h/.cpp`, `GameSoundSubsystem.h/.cpp`(콘솔 `ltts.Sound List/Play/Volume/Save`), `SoundCellTrigger.h/.cpp` |
| 발동 지점 | 3.6절 표 전부: `AGridTrain`, `APuzzleBlock`, `APuzzleRotationTile`, `APuzzleRotatingObstacle`, `APuzzleLever`, `APuzzleElevatorBlock`, `APuzzleElevatorDock`, `AGridTestGameMode`, `AGridPlayerController` |
| 셀 진입 방송 | `AGridActor::OnPawnEnteredCell` 추가, `AGridNPC`가 셀에 들어설 때 `NotifyPawnEnteredCell`을 부른다 |
| UIManager | `GetMasterVolume` / `ApplyMasterVolume` / `CommitSettings` (`UIManagerSubsystem.cpp`는 원래 인코딩 CP949 유지) |
| Config | `DefaultEngine.ini`(GameUserSettingsClassName, DefaultSoundClassName, DefaultBaseSoundMix), `DefaultGame.ini`(`[/Script/LetsTakeTheSubway.SoundSettings]`) |
| 빌드 | 게임 타깃, 에디터 타깃 모두 성공. 프로젝트 파일 경고 없음 |
| 애셋 | `Content/Core/Sound/`: `SC_Master`(자식 `SC_SFX`·`SC_Ambience`·`SC_UI`·`SC_Music`), `SMix_Master`, `ATT_World`(구, 안쪽 3000 cm + 감쇠 9000 cm). `Content/Art/Sound/SFX/`: `SW_01`~`SW_16` 15개, 클래스 지정, `SW_05`·`SW_08` 루프. `Content/Design/Sound/DA_SoundLibrary`: 키 17개, `Train.ArrivalAnnouncement`·`Stage.Clear`는 빈 채 |
| 위젯 | `WBP_SideMenuUI`, `WBP_MainUI`: `Slider`를 변수로 노출, `OnValueChanged(Slider)` -> `ApplyMasterVolume`, `OpenSettingPanel` 끝에 `Slider.Value = GetMasterVolume`, `CloseSettingPanel` 끝에 `CommitSettings`. 슬라이더 범위 0~1 |
| 개찰구 | `Subway_Stage2`에 `SoundTrigger_TicketGate_A`(게이트 001~006, 30셀)·`_B`(게이트 007~015, 44셀), 아웃라이너 폴더 `Sound`. Stage2는 액터별 파일 방식이 아니라 `Subway_Stage2.umap` 자체가 바뀌었다 |

### PIE 확인 (Subway_Stage2)

- 시작 로그: `Sound library DA_SoundLibrary: 15 sound(s) loaded, 2 key(s) still without a sound.`, 트리거 두 개 등록. 사운드 경고·오류 0건.
- `ltts.Sound Volume 0.3` -> `ltts.Sound Save` -> `Saved/Config/WindowsEditor/GameUserSettings.ini`의 `[/Script/LetsTakeTheSubway.LTTSGameUserSettings] MasterVolume`에 기록. 확인 뒤 1.0으로 되돌려 저장했다.
- 재생 중인 오디오 컴포넌트를 PIE 월드에서 셌다: 출발 직후 `SW_03_TrainNotification` 2개(2D), 두 열차에 붙은 `SW_01_SubwaySound`, `ltts.RotateObstacle` 직후 기둥에 붙은 `SW_15_PillarSpin` 7개.
- 귀로 듣는 확인은 하지 못했다(자동화 환경).

### 남은 확인 (사람이 PIE에서 들어 봐야 하는 것)

1. **감쇠 반경.** 시작 시점 구역 카메라에서 두 열차까지 약 215 m였다. `ATT_World`의 120 m 밖이라 그 순간의 진입음은 들리지 않는다. 플레이어가 있는 구역의 열차만 들리는 것이 의도라면 그대로 두고, 아니면 `ATT_World`의 감쇠 거리를 늘린다.
2. **Stage2 알림음 중복.** 두 열차가 동시에 출발해 2D 알림음이 겹쳐 난다. 화면 밖 역은 정차역의 `bArrivalSounds`를 끈다.
3. **`StopSoundDistance` 1500 cm**가 "착"과 정차 순간을 맞추는지.
4. 볼륨 곡선(선형), 클릭음 범위, `Elevator.Arrived`의 클리어 승강 포함 여부(7절).
