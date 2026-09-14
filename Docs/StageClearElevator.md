# 스테이지 클리어 엘리베이터 — 세팅 가이드

작성일 2026-09-11. 대상: 기획·레벨 디자이너.
구현 배경과 설계 근거는 `Docs/Plans/BoardingRefactor.md` 10절을 본다.

퍼즐을 풀고 엘리베이터를 타면 올라가다 멈추고, 거기서 컷신이나 다음 레벨로 넘어가는 흐름을 만드는
방법이다. 엘리베이터를 새로 만들 필요는 없다. 쓰던 `PuzzleElevatorDock`에 스위치 하나를 켜면 된다.

---

## 0. 먼저 알아 둘 것 — 무엇이 "클리어"를 발동하는가

**바닥 셀을 밟는 것으로는 클리어 연출이 일어나지 않는다.** 컷신이 나오려면 네 가지가 순서대로
일어나야 한다.

1. 엘리베이터가 구조물 위에 **완전히** 겹친다 (한 칸이라도 걸치면 안 된다)
2. 플레이어가 **차체를 클릭**한다
3. 폰이 가장 가까운 문 앞까지 걸어가 **실제로 탑승**한다
4. 차체가 설정한 높이만큼 **움직인다**

넷을 다 지나야 컷신 함수가 불린다. StageClear 셀은 "이제 저 승강기가 출구다"라고 표시만 해 주는
선택 사항이며, 그것 없이도 동작한다(1절).

---

## 1. Dock의 Detail 설정

레벨에서 출구로 쓸 `PuzzleElevatorDock`를 고르고 Detail 패널의 **Elevator Dock | Stage Clear**를 연다.

| 칸 | 기본값 | 뜻 |
|---|---|---|
| **Is Clear** | 꺼짐 | 켜면 이 구조물이 엔딩 승강기가 된다. 짝 Dock이 없어도 탑승이 되고, 도착하면 컷신이 불린다 |
| **Clear On Stage Clear** | 꺼짐 | 켜면 플레이어가 StageClear 셀을 밟는 순간 Is Clear가 저절로 켜진다 |
| **Clear Travel Height** | 1500 | 컷신이 터지기까지 움직일 거리(cm). 1500 = 15 m |
| **Clear Travel Up** | 켜짐 | 올라갈지 내려갈지 |
| **Clear Travel Speed** | 0 | 0이면 위쪽 Travel Speed를 쓴다. 엔딩은 100~150이 보기 좋다 |

클리어 조건을 정하는 방법은 둘 중 하나다.

**가장 간단한 방법은 Is Clear를 처음부터 켜 두는 것이다.** 퍼즐을 풀어야만 차체를 구조물까지 밀 수
있다면, "구조물에 올려놓고 탄다" 자체가 이미 클리어 조건이다. 추가 저작이 없다.

**다른 조건을 쓰려면 Clear On Stage Clear를 켠다.** 이때는 `GridCellMarker`로 셀 하나를 StageClear로
지정하고 그리드를 다시 생성해야 한다. **지정하지 않으면 영영 켜지지 않는다.** 지금
`Subway_Stage1`에는 StageClear 셀이 하나도 없다(그리드 통계의 `stageClear` 항목으로 확인할 수 있다).

Is Clear는 Blueprint Read/Write이므로 다른 조건이 필요하면 아무 블루프린트에서나 켜도 된다.

### Clear Travel Height를 얼마로 둘까

이 값이 **컷신이 터지는 지점**을 정한다. 천장을 뚫고 올라가는 그림을 보이고 싶지 않다면 천장까지의
높이보다 조금 작게 잡는다. 화면이 어두워지는 연출을 쓴다면 그 사이에 가려지므로 크게 신경 쓰지
않아도 된다.

---

## 2. 배치할 때 주의할 것 — 차체의 "층"

구조물은 **자기 층에 있는 차체만** 자기 것으로 친다. 그래서 배치가 어긋나면 클릭해도
`Push the elevator onto the dock first.`가 뜬다.

차체의 층은 **BeginPlay 때 발밑 셀의 바닥 높이**로 정해지고, 밀어도 유지된다. 그러므로:

- **평지에 놓인 구조물**(샤프트가 아닌 보통 바닥)은 아무 문제 없다. 차체를 그 위에 놓으면 층이 같다.
- **샤프트 위 구조물**은 다르다. 샤프트 셀은 아래층 높이로 구워지므로, 차체를 에디터에서 샤프트에
  직접 놓으면 **아래층 차체**가 된다. 위층 구조물이 받지 않는다.
  위층 구조물에 태우려면 차체를 **위층 바닥에 놓고 플레이 중에 밀어 넣어야** 한다. 그것이 원래
  의도한 퍼즐 흐름이기도 하다.

엔딩 승강기를 새로 만드는 경우라면 **평지에 구조물을 놓는 편이 단순하다.** 짝 Dock도, 샤프트도
필요 없다.

### 문 앞에 설 자리가 있어야 한다

폰이 걸어가 탈 칸이 하나도 없으면 `There is no door to walk to from this floor.`가 뜬다.
차체의 `Door Axis`가 가리키는 양쪽 면 앞의 칸이 **걸을 수 있고 차체와 같은 층**이어야 한다.
`Door Axis`가 Y면 남북 면 앞, X면 동서 면 앞이다.

---

## 3. 컷신 연결

두 가지 길이 있다. **이미 배치한 구조물을 그대로 쓸 거라면 레벨 블루프린트 쪽이 편하다.**

### 3-A. 레벨 블루프린트 (권장)

1. 뷰포트에서 그 Dock 액터를 선택한다.
2. 툴바 Blueprints → **Open Level Blueprint**.
3. 그래프 빈 곳에서 우클릭 → 맨 위의 `Add Event for <액터 이름>` → **On Clear Cutscene**.
4. 노드에 Dock, Elevator, Pawn 핀이 나온다. 안 써도 된다.

### 3-B. Dock을 블루프린트로 파생

여러 스테이지에서 같은 연출을 재사용할 때 쓴다. Blueprint Class를 만들고 부모로
`PuzzleElevatorDock`를 고른 뒤, My Blueprint 패널의 **Override** 목록에서
**On Stage Clear Cutscene**을 고른다.

대신 레벨에 이미 놓인 액터를 이 블루프린트로 교체해야 하고, Target Dock 같은 설정을 다시 넣어야 한다.

> 둘은 같은 순간에, **한 번만** 발생한다. 편한 쪽 하나만 쓰면 된다.

---

## 4. 연출과 다음 레벨

On Clear Cutscene 뒤에 이렇게 잇는다.

```
On Clear Cutscene
  → Get Player Camera Manager   (Player Index 0)
  → Start Camera Fade
        From Alpha        0.0
        To Alpha          1.0
        Duration          2.0
        Color             검정
        Should Fade Audio  체크
        Hold When Finished 체크      ← 끄면 페이드가 풀려 화면이 다시 밝아진다
  → Delay   2.0
  → Open Level (by Object Reference)    Level = 다음 레벨
```

Open Level은 **by Object Reference** 쪽을 쓴다. 맵 에셋을 직접 참조하므로 나중에 맵 이름을 바꿔도
링크가 끊기지 않는다. by Name은 문자열이라 오타를 에디터가 잡아 주지 않는다.

`Start Camera Fade`와 `Open Level`은 둘 다 엔진 기본 노드다. 플러그인이나 모듈 추가가 필요 없다.

### 올라가는 동안 어두워지게 하려면

지금 구조는 다 올라간 **뒤에** 컷신이 터진다. 올라가는 내내 서서히 어두워지게 하려면 페이드를 한
단계 앞으로 당긴다. 레벨 블루프린트에서 **차체**(`PuzzleElevatorBlock`)를 선택해 그 액터의
**On Boarded** 이벤트를 추가하고 거기서 Start Camera Fade를 돌린다. Dock의 On Clear Cutscene에서는
Open Level만 하면 된다.

---

## 5. 주의할 것 두 가지

**차체와 폰을 파괴하거나 Unpossess 하지 말 것.** 폰은 타고 있던 탈것이 사라지면 스스로 그리드로
내려선다. 컷신 도중에 그러면 엘리베이터 안에 있어야 할 폰이 허공에서 걸어 나온다. Open Level은
월드가 통째로 사라지므로 상관없다.

**컷신을 붙이지 않으면 플레이어가 갇힌다.** 엔딩 승강기는 목표 높이에서 멈춘 채 조작이 잠긴 상태로
유지된다. 화면을 넘겨 주는 것은 전적으로 블루프린트의 몫이다. 아직 안 붙였으면 출력 로그에 이 줄이
뜬다. 설정은 맞았고 연출만 비어 있다는 뜻이다.

```
PuzzleElevatorDock_4: no cutscene is bound. Override OnStageClearCutscene or bind OnClearCutscene.
```

---

## 6. 마우스 없이 시험하기 — `ltts.ElevatorRide`

탑승은 원래 마우스로만 시작되므로, 연출을 붙이기 전에 승강 흐름만 먼저 확인하고 싶을 때 쓰는
콘솔 명령을 두었다. 열차의 `ltts.TrainArrive`와 같은 자리다.

```
ltts.ElevatorRide                  구조물 위에 올라가 있는 차체를 탄다
ltts.ElevatorRide ElevatorCar2     이름으로 골라서 탄다
```

**클릭과 같은 함수를 부른다.** 그래서 이 명령이 통과하면 마우스로도 통과한다. 구조물 판정, 문 앞
셀 찾기, 걸어가기, 탑승, 승강, 컷신 호출까지 전부 실제 경로를 지난다.

플레이 중에만 동작한다. 에디터 상태에서는 `run this in play mode.`가 뜬다.

---

## 7. 동작 확인 (2026-09-11)

평지 B1에 임시 구조물을 놓고 `Is Clear` + 600 cm + 150 cm/s로 시험했다. 로그가 순서대로 나왔다.

```
PuzzleElevatorDock_4: 4x4 floor tile at cell (115,5). Elevator dock at Z 770,
                      stage-clear exit: rides 600 cm up then the cutscene.
PuzzleElevatorDock_4: stage-clear exit at Z 770; rides 600 cm up at 150 cm/s, then the cutscene.

ltts.ElevatorRide: asking to board ElevatorCar2.
PuzzleElevatorBlock_1: GridPawn_0 boarded from cell (118,9); doors face the AxisY axis.
GridPawn_0: boarding PuzzleElevatorBlock_1 from cell (118,9).
PuzzleElevatorBlock_1: stage-clear ride from Z 770 to Z 1370 at 150 cm/s; it will hold there.
PuzzleElevatorBlock_1: holding at Z 1370 with GridPawn_0 aboard; the cutscene takes over.
PuzzleElevatorDock_4: stage-clear cutscene starts with GridPawn_0 aboard PuzzleElevatorBlock_1.
PuzzleElevatorDock_4: no cutscene is bound. Override OnStageClearCutscene or bind OnClearCutscene.
```

폰이 문 앞 (118,9)까지 스스로 걸어가 탔고, 770에서 1370까지 600 cm 올라간 뒤 멈춘 채 컷신 훅이
한 번 불렸다. 마지막 줄은 아직 블루프린트를 붙이지 않아서 나온 안내다.

시험에 쓴 임시 구조물은 지웠고 차체는 원래 자리로 되돌렸다. `Subway_Stage1`에는 아직 엔딩 승강기로
지정된 구조물이 없다.

---

## 8. 자주 보게 될 거절 문구

| 화면에 뜨는 말 | 뜻 | 고치는 법 |
|---|---|---|
| Push the elevator onto the dock first. | 차체가 구조물에 완전히 안 겹쳤거나, 층이 다르다 | 2절 참고 |
| There is no door to walk to from this floor. | 문 앞에 걸을 수 있는 칸이 없다 | Door Axis를 승강장 쪽으로 돌리거나 문 앞 바닥을 만든다 |
| There is no way to reach a door from here. | 문 앞 칸까지 가는 길이 없다 | 길이 막혔는지, 층이 끊겼는지 본다 |
| The elevator is busy. | 이미 승강 중이다 | 기다린다 |
| Wait until you have stopped walking. | 폰이 걷는 중이다 | 멈춘 뒤 다시 누른다 |
