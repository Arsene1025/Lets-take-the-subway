# Lets Take the Subway

Unreal Engine **5.8.2** 기반 3D 프로젝트입니다. 현재는 아무 것도 구현되지 않은 빈 프로젝트이며,
협업 폴더 규칙과 Claude Code MCP 연결만 설정되어 있습니다.

---

## 1. 시작하기

### 준비물

| 항목 | 버전 |
|---|---|
| Unreal Engine | 5.8.2 |
| Visual Studio | 2022 (Game development with C++ 워크로드) |
| Git LFS | 3.x 이상 |

### 클론

```bash
git lfs install
git clone https://github.com/Arsene1025/Lets-take-the-subway.git
cd Lets-take-the-subway
```

`git lfs install`을 먼저 하지 않으면 `.uasset` 같은 바이너리 파일이 텍스트 포인터로 받아집니다.
이미 그렇게 받았다면 `git lfs pull`로 복구하세요.

### 열기

- **아트 / 기획**: `LetsTakeTheSubway.uproject`를 더블클릭하면 됩니다.
  최초 실행 시 "모듈을 빌드해야 합니다" 창이 뜨면 프로그래머에게 문의하세요.
  (`Binaries/`는 저장소에 포함되지 않습니다.)
- **프로그래머**: `.uproject` 우클릭 → *Generate Visual Studio project files* → 솔루션 열기 → `Development Editor` 빌드.

명령줄 빌드:

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" LetsTakeTheSubwayEditor Win64 Development -project="%CD%\LetsTakeTheSubway.uproject" -waitmutex
```

---

## 2. 폴더 규칙 (중요)

아트·기획의 작업이 프로그래머 작업과 충돌하지 않도록 **소유 폴더**를 분리합니다.
본인 소유가 아닌 폴더의 파일은 임의로 옮기거나 이름을 바꾸지 않습니다.

| 경로 | 소유 | 내용 |
|---|---|---|
| `Content/Art/` | 아트 | 메시, 머티리얼, 텍스처, 애니메이션, VFX, UI 리소스, 사운드 |
| `Content/Design/` | 기획 | DataTable, DataAsset, Curve, 밸런스 값, 기획용 프로토타입 |
| `Content/Core/` | 프로그래머 | C++ 클래스를 상속한 BP, GameMode, Input, 시스템 에셋 |
| `Content/Maps/` | 공용 | 레벨 |
| `Content/Developers/<이름>/` | 개인 | 개인 실험용. 여기 있는 것은 아무도 참조하지 않음 |
| `Source/`, `Config/`, `*.uproject` | 프로그래머 | C++ 코드, 프로젝트 설정 |

### 참조 방향

- `Art`, `Design` → `Core` 참조: **허용** (예: 프로그래머가 만든 BP를 아트가 상속)
- `Core` → `Art`, `Design` 하드 참조: **지양**. 필요하면 DataAsset이나 소프트 레퍼런스로 받습니다.
  이렇게 해야 아트가 에셋을 교체해도 코드가 깨지지 않습니다.

### 충돌 방지

- `.uasset` / `.umap`은 **머지가 불가능합니다.** 같은 파일을 두 사람이 동시에 수정하면 한쪽 작업이 사라집니다.
- 레벨은 **World Partition + One File Per Actor(OFPA)** 로 만듭니다. 액터별로 파일이 나뉘어 동시 작업이 가능해집니다.
- 작업 전 `git pull`, 작업 후 바로 push 하는 습관을 권장합니다.
- 개인 실험은 `Content/Developers/<이름>/`에서 하고, 확정되면 소유 폴더로 옮깁니다.

---

## 3. Claude Code MCP 연결

UE 5.8에 내장된 Epic 공식 **Unreal MCP** 플러그인을 사용합니다. 별도 설치는 필요 없습니다.

구성:

- `LetsTakeTheSubway.uproject` — `ModelContextProtocol`, `AllToolsets` 플러그인 활성화 (에디터 전용)
- `Config/DefaultEditorPerProjectUserSettings.ini` — 에디터 실행 시 MCP 서버 자동 시작
- `.mcp.json` — Claude Code가 붙을 주소 `http://127.0.0.1:8000/mcp`

사용 순서:

1. 언리얼 에디터로 프로젝트를 엽니다. MCP 서버가 자동으로 뜹니다.
   Output Log에서 `LogModelContextProtocol: MCP Server started on http://127.0.0.1:8000` 확인.
2. 저장소 루트에서 Claude Code를 실행합니다.
3. 처음 실행하면 프로젝트 MCP 서버를 사용할지 묻습니다. 승인하면 `.claude/settings.local.json`에 기록됩니다.
   이 파일은 개인 설정이라 저장소에 올라가지 않습니다.
4. `claude mcp list`로 `unreal-mcp`가 연결되었는지 확인할 수 있습니다.

수동 제어가 필요하면 에디터 콘솔에서:

```
ModelContextProtocol.StartServer
ModelContextProtocol.StopServer
ModelContextProtocol.GenerateClientConfig ClaudeCode
```

주의:

- 이 플러그인은 Epic이 **Experimental**로 표기한 기능입니다. 패키징 빌드에는 포함하지 않습니다.
- 서버는 `127.0.0.1`만 받습니다. 인증이 없으므로 포트를 외부에 노출하지 마세요.
- 에디터가 켜져 있어야만 MCP 도구가 동작합니다.

---

## 4. 아직 하지 않은 것

- GameMode, 캐릭터, 입력 매핑, 레벨 등 게임 구현 일체
- GitHub CODEOWNERS 및 브랜치 보호 규칙 (팀원 계정 확정 후 설정)
- 원격 저장소의 Git LFS 활성화 및 스토리지 할당량 확인
