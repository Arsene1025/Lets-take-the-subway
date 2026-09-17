// Fill out your copyright notice in the Description page of Project Settings.


#include "UIManagerSubsystem.h"
#include "UISettings.h"
#include "UIInitializable.h"
#include "CutScene/CutSceneWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Sound/GameSoundSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Guide/GuideDataSubsystem.h"
#include "Stage/StageInfo.h"
#include "Stage/StageSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Player/GridPlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Widgets/SViewport.h"


namespace
{
    /**
     * [2026/09/17] ESC로 사이드 메뉴를 여닫기 위한 슬레이트 입력 전처리기.
     *
     * 플레이어 컨트롤러의 입력 액션으로는 안 된다: 사이드 메뉴가 열리면 입력 모드가 UI 전용
     * (FInputModeUIOnly)이 되어 게임 뷰포트가 키를 받지 않으므로 닫는 ESC가 전달되지 않는다.
     * 전처리기는 포커스와 입력 모드에 관계없이 슬레이트가 키를 나눠 주기 전에 먼저 받고,
     * 처리했다고 답하면 위젯이나 에디터(PIE 정지)에는 전달되지 않는다. 판단은 전부
     * UUIManagerSubsystem::HandleEscapeKey가 한다.
     */
    class FSideMenuEscapeProcessor : public IInputProcessor
    {
    public:
        explicit FSideMenuEscapeProcessor(UUIManagerSubsystem* InOwner)
            : Owner(InOwner)
        {
        }

        virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
        {
        }

        virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
        {
            if (InKeyEvent.GetKey() != EKeys::Escape || InKeyEvent.IsRepeat())
                return false;

            UUIManagerSubsystem* Manager = Owner.Get();
            return Manager && Manager->HandleEscapeKey();
        }

        virtual const TCHAR* GetDebugName() const override { return TEXT("SideMenuEscape"); }

    private:
        TWeakObjectPtr<UUIManagerSubsystem> Owner;
    };

    /**
     * [2026/09/17] 콘솔 ltts.Cutscene <Intro|Ending> [레벨 경로]
     *
     * 어느 레벨에서든 컷씬을 띄워 본다. 레벨 경로(/Game/Maps/Main/Subway_Title)를 주면 끝난 뒤 그 레벨을 연다.
     * 인자가 없으면 재생 중인 컷씬을 끝낸다.
     */
    static FAutoConsoleCommandWithWorldAndArgs GCutsceneCommand(
        TEXT("ltts.Cutscene"),
        TEXT("ltts.Cutscene <Intro|Ending> [/Game/Maps/Main/Level] : play a picture cutscene, then open the level. No args: end the current cutscene."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UUIManagerSubsystem* UI = UUIManagerSubsystem::Get(World);
            if (!UI)
            {
                UE_LOG(LogTemp, Warning, TEXT("ltts.Cutscene: no UI manager (not a game world?)."));
                return;
            }

            if (Args.IsEmpty())
            {
                UI->EndCutscene();
                return;
            }

            ECutsceneKind Kind;
            if (Args[0].Equals(TEXT("Intro"), ESearchCase::IgnoreCase))
            {
                Kind = ECutsceneKind::Intro;
            }
            else if (Args[0].Equals(TEXT("Ending"), ESearchCase::IgnoreCase))
            {
                Kind = ECutsceneKind::Ending;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ltts.Cutscene: unknown kind '%s' (Intro or Ending)."), *Args[0]);
                return;
            }

            TSoftObjectPtr<UWorld> NextLevel;
            if (Args.Num() > 1)
            {
                NextLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(Args[1]));
            }

            UI->PlayCutscene(Kind, NextLevel);
        }));
}


#pragma region Helper

UUserWidget* UUIManagerSubsystem::GetOrCreateWidget(TObjectPtr<UUserWidget>& Cached, const TSoftClassPtr<UUserWidget>& ClassPtr)
{
    if (IsValid(Cached))
        return Cached;

    ULocalPlayer* LP = GetLocalPlayer();
    if (!LP)
        return nullptr;

    UWorld* World = LP->GetWorld();
    if (!World)
        return nullptr;

    APlayerController* PC = LP->GetPlayerController(World);
    if (!PC)
        return nullptr;


    UClass* WidgetClass = ClassPtr.LoadSynchronous();
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[UIManager] 위젯 클래스가 지정되지 않았습니다. "));
        return nullptr;
    }

    UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
    if (!Widget)
        return nullptr;

    Widget->AddToViewport();
    Widget->SetVisibility(ESlateVisibility::Collapsed);

    Cached = Widget;
    return Widget;
}

void UUIManagerSubsystem::HandleMapLoaded(UWorld* NewWorld)
{
    ClearAllCachedWidgets();

    //위젯이 모두 사라졌으니 열린 UI도 없다. 입력 모드는 새 컨트롤러가 BeginPlay에서 정한다.
    OpenUICount = 0;

    //[2026/09/16] 새 레벨의 스테이지 서브시스템에 다시 붙는다. 월드 서브시스템은 InitWorld에서 만들어지므로
    //이 시점에 이미 있다. 플레이어 컨트롤러는 아직 없을 수 있어 PlayerControllerChanged에서도 한 번 더 붙는다.
    UnbindStage();
    BindStage(UStageSubsystem::Get(NewWorld));
}

void UUIManagerSubsystem::ClearAllCachedWidgets()
{
    if (IsValid(PathUIWidget))
        PathUIWidget->RemoveFromParent();       PathUIWidget = nullptr;

    if (IsValid(SideMenuUIWidget))
        SideMenuUIWidget->RemoveFromParent();   SideMenuUIWidget = nullptr;

    if (IsValid(GuidePopUpUIWidget))
        GuidePopUpUIWidget->RemoveFromParent(); GuidePopUpUIWidget = nullptr;

    if (IsValid(AlertUIWidget))
        AlertUIWidget->RemoveFromParent();      AlertUIWidget = nullptr;

    //[2026/09/17] 컷씬 위젯도 함께 지운다. 빠뜨리면 맵이 바뀐 뒤에도 포인터가 남아 ESC가 영영 막힌다.
    //대기 중인 다음 레벨도 잊는다: 컷씬 도중 다른 이유로 맵이 바뀌었으면 그 흐름은 끝난 것이다.
    if (IsValid(CutsceneWidget))
        CutsceneWidget->RemoveFromParent();     CutsceneWidget = nullptr;
    PendingLevelAfterCutscene.Reset();
}

void UUIManagerSubsystem::ApplyUIInputMode(bool bUIOnly)
{
    ULocalPlayer* LP = GetLocalPlayer();
    UWorld* World = LP ? LP->GetWorld() : nullptr;
    APlayerController* PC = (LP && World) ? LP->GetPlayerController(World) : nullptr;
    if (!PC)
        return;

    if (bUIOnly)
    {
        FInputModeUIOnly InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);

        //누르고 있던 키의 뗌 이벤트는 UI 전용 모드에서 게임으로 오지 않는다. 눌린 채로 남지 않게 비운다.
        PC->FlushPressedKeys();
    }
    else
    {
        //AGridPlayerController::BeginPlay와 같은 설정. 클릭 이동에는 잠기지 않은 커서가 필요하다.
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);
    }

    PC->SetShowMouseCursor(true);

    UE_LOG(LogTemp, Display, TEXT("[UIManager] Input mode: %s."), bUIOnly ? TEXT("UI only") : TEXT("game and UI"));
}

#pragma endregion



#pragma region Lifecycle

void UUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UUIManagerSubsystem::HandleMapLoaded);

    //ESC 토글. 슬레이트가 없는 환경(커맨드렛, 전용 서버)에서는 등록하지 않는다.
    if (FSlateApplication::IsInitialized())
    {
        EscapeProcessor = MakeShared<FSideMenuEscapeProcessor>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(EscapeProcessor);
    }
}

void UUIManagerSubsystem::Deinitialize()
{
    if (EscapeProcessor.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().UnregisterInputPreProcessor(EscapeProcessor);
    }
    EscapeProcessor.Reset();

    UnbindStage();
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    OnUIOpened.RemoveAll(this);
    ClearAllCachedWidgets();

    Super::Deinitialize();
}

void UUIManagerSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
    Super::PlayerControllerChanged(NewPlayerController);

    //컨트롤러가 사라지는 중(레벨 정리)이면 풀기만 한다.
    if (!NewPlayerController)
    {
        UnbindStage();
        return;
    }

    UStageSubsystem* Stage = UStageSubsystem::Get(NewPlayerController);
    if (Stage != BoundStage.Get())
    {
        UnbindStage();
        BindStage(Stage);
    }
    else if (Stage && Stage->IsResolved())
    {
        //같은 레벨에 이미 붙어 있었지만, 판정 방송이 컨트롤러보다 먼저 와서 위젯을 못 만들었을 수 있다.
        HandleStageStateChanged(Stage->GetState());
    }
}

#pragma endregion



#pragma region GameInterface

UUIManagerSubsystem* UUIManagerSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    const ULocalPlayer* LP = GI ? GI->GetFirstGamePlayer() : nullptr;
    return LP ? LP->GetSubsystem<UUIManagerSubsystem>() : nullptr;
}

void UUIManagerSubsystem::ShowOrRefreshPathUI(int32 PathIndex)
{
    //[2026/09/16] 인자를 그림 번호(0~9)로 바꿨다. 위젯은 InitializeInt의 Switch On Int로 그림을 고른다.
    if (PathIndex < 0)
        return;

    const UUISettings* Settings = GetDefault<UUISettings>();

    if (UUserWidget* W = GetOrCreateWidget(PathUIWidget, Settings->PathUIWidget))
    {
        W->SetVisibility(ESlateVisibility::Visible);

        if (W->GetClass()->ImplementsInterface(UUIInitializable::StaticClass()))
        {
            IUIInitializable::Execute_InitializeInt(W, PathIndex);
        }

        //위젯을 실제로 띄웠을 때만 기록한다. 컨트롤러가 아직 없어 실패했다면 다음 방송에서 다시 시도한다.
        LastPathIndex = PathIndex;

        UE_LOG(LogTemp, Display, TEXT("[UIManager] PathUI index %d."), PathIndex);
    }
}

void UUIManagerSubsystem::ShowSideMenuUI()
{
    const UUISettings* Settings = GetDefault<UUISettings>();

    if (UUserWidget* W = GetOrCreateWidget(SideMenuUIWidget, Settings->SideMenuUIWidget))
    {
        W->SetVisibility(ESlateVisibility::Visible);

        if (W->GetClass()->ImplementsInterface(UUIInitializable::StaticClass()))
        {
            IUIInitializable::Execute_Initialize(W);
        }
    }
}

void UUIManagerSubsystem::HideSideMenuUI()
{
    if (!IsSideMenuOpen())
        return;

    //WBP_SideMenuUI의 닫기 버튼과 같은 순서: 접은 뒤 열린 UI 개수를 줄인다(0이 되면 게임 입력 모드로 복귀).
    SideMenuUIWidget->SetVisibility(ESlateVisibility::Collapsed);
    CallUIClosed();
}

void UUIManagerSubsystem::ToggleSideMenuUI()
{
    if (IsSideMenuOpen())
    {
        HideSideMenuUI();
    }
    else
    {
        ShowSideMenuUI();
    }
}

bool UUIManagerSubsystem::IsSideMenuOpen() const
{
    //IsVisible: Visible·HitTestInvisible·SelfHitTestInvisible. Collapsed(닫힘)와 Hidden은 false.
    return IsValid(SideMenuUIWidget) && SideMenuUIWidget->IsVisible();
}

void UUIManagerSubsystem::ShowGuidePopUpUI(EGuideType guide)
{
    const UUISettings* Settings = GetDefault<UUISettings>();

    if (UUserWidget* W = GetOrCreateWidget(GuidePopUpUIWidget, Settings->GuidePopUpUIWidget))
    {
        W->SetVisibility(ESlateVisibility::Visible);

        if (W->GetClass()->ImplementsInterface(UUIInitializable::StaticClass()))
        {
            IUIInitializable::Execute_InitializeGuide(W, guide);
        }
    }
}

void UUIManagerSubsystem::ShowAlertUI(FText message)
{
    const UUISettings* Settings = GetDefault<UUISettings>();

    if (UUserWidget* W = GetOrCreateWidget(AlertUIWidget, Settings->AlertUIWidget))
    {
        W->SetVisibility(ESlateVisibility::Visible);

        if (W->GetClass()->ImplementsInterface(UUIInitializable::StaticClass()))
        {
            IUIInitializable::Execute_Initialize(W);
        }
    }
}

//[2026/09/17] 옛 경로. 위젯의 InitializeInt(정수 번호)에 재생을 맡겼다. ECutsceneKind 버전으로 대체.
//void UUIManagerSubsystem::PlayCutscene(int32 cutscene)
//{
//    ULocalPlayer* LP = GetLocalPlayer();
//    if (!LP)
//        return;
//
//    UWorld* World = LP->GetWorld();
//    if (!World)
//        return;
//
//    APlayerController* PC = LP->GetPlayerController(World);
//    if (!PC)
//        return;
//
//
//    const UUISettings* Settings = GetDefault<UUISettings>();
//    UClass* WidgetClass = Settings->CutsceneWidget.LoadSynchronous();
//    if (!WidgetClass)
//    {
//        UE_LOG(LogTemp, Warning,
//            TEXT("[UIManager] 위젯 클래스가 지정되지 않았습니다. "));
//        return;
//    }
//
//    CutsceneWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
//    if (!CutsceneWidget)
//        return;
//
//    CutsceneWidget->AddToViewport(100);   // Z 순서 높게
//
//    if (CutsceneWidget->GetClass()->ImplementsInterface(UUIInitializable::StaticClass()))
//    {
//        IUIInitializable::Execute_InitializeInt(CutsceneWidget, cutscene);
//    }
//}

const UCutSceneDatabase* UUIManagerSubsystem::FindCutsceneDatabase(ECutsceneKind Kind)
{
    const UUISettings* Settings = GetDefault<UUISettings>();
    if (!Settings)
        return nullptr;

    switch (Kind)
    {
    case ECutsceneKind::Intro:  return Settings->IntroCutscene.LoadSynchronous();
    case ECutsceneKind::Ending: return Settings->EndingCutscene.LoadSynchronous();
    default:                    return nullptr;
    }
}

void UUIManagerSubsystem::PlayCutscene(ECutsceneKind Kind, TSoftObjectPtr<UWorld> NextLevel)
{
    if (IsCutscenePlaying())
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Cutscene %s ignored: another cutscene is playing."),
            *UEnum::GetValueAsString(Kind));
        return;
    }

    ULocalPlayer* LP = GetLocalPlayer();
    UWorld* World = LP ? LP->GetWorld() : nullptr;
    APlayerController* PC = (LP && World) ? LP->GetPlayerController(World) : nullptr;
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Cutscene %s ignored: no player controller."),
            *UEnum::GetValueAsString(Kind));
        return;
    }

    ActiveCutscene = Kind;
    PendingLevelAfterCutscene = NextLevel;

    //새 게임의 시작. 지난 판에 본 가이드를 잊어 튜토리얼 팝업이 다시 뜬다(GameLoop.md 3.9).
    if (Kind == ECutsceneKind::Intro)
    {
        if (UGuideDataSubsystem* Guide = UGuideDataSubsystem::Get(World))
        {
            Guide->ResetShownGuides();
        }
    }

    const UUISettings* Settings = GetDefault<UUISettings>();
    UClass* WidgetClass = Settings ? Settings->CutsceneWidget.LoadSynchronous() : nullptr;
    const UCutSceneDatabase* Database = FindCutsceneDatabase(Kind);

    //위젯이나 애셋이 빠져도 루프는 끊기지 않는다: 컷씬만 건너뛰고 다음 레벨로 간다.
    if (!WidgetClass || !WidgetClass->IsChildOf(UCutSceneWidget::StaticClass()) || !Database)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[UIManager] Cutscene %s skipped: widget %s (must derive from CutSceneWidget), database %s. Check Project Settings > Game > UI Manager."),
            *UEnum::GetValueAsString(Kind), *GetNameSafe(WidgetClass), *GetNameSafe(Database));
        EndCutscene();
        return;
    }

    UCutSceneWidget* Widget = CreateWidget<UCutSceneWidget>(PC, WidgetClass);
    if (!Widget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIManager] Cutscene %s skipped: failed to create %s."),
            *UEnum::GetValueAsString(Kind), *WidgetClass->GetName());
        EndCutscene();
        return;
    }

    CutsceneWidget = Widget;
    CutsceneWidget->AddToViewport(100);   //Z 순서 높게. 다른 UI(PathUI, 사이드 메뉴)를 덮는다.

    UE_LOG(LogTemp, Display, TEXT("[UIManager] Cutscene %s starts; then %s."),
        *UEnum::GetValueAsString(Kind),
        NextLevel.IsNull() ? TEXT("stay in this level") : *NextLevel.ToSoftObjectPath().ToString());

    //Play가 프레임이 비어 있어 곧바로 끝나면 OnFinished → EndCutscene이 여기서 동기로 불린다. 그래도 안전하다.
    Widget->OnFinished.AddUniqueDynamic(this, &UUIManagerSubsystem::EndCutscene);
    Widget->Play(Database);
}

bool UUIManagerSubsystem::IsCutscenePlaying() const
{
    return IsValid(CutsceneWidget);
}

#pragma endregion



#pragma region UIInterface

void UUIManagerSubsystem::CallUIOpened()
{
    //[2026/09/16] 첫 UI가 열릴 때만 입력 모드를 바꾼다. 겹쳐 열린 UI는 개수만 센다.
    // --> 개수 셀 필요가 없게 bp단에서 특수화를 해뒀는데... 왜한거임?
    ++OpenUICount;
    if (OpenUICount == 1)
    {
        ApplyUIInputMode(true);
    }

    OnUIOpened.Broadcast();
}

void UUIManagerSubsystem::CallUIClosed()
{
    //열지 않은 UI를 닫는 호출(닫기 버튼 두 번 등)은 개수를 음수로 만들지 않는다.
    if (OpenUICount > 0)
    {
        --OpenUICount;
    }

    if (OpenUICount == 0)
    {
        ApplyUIInputMode(false);
    }

    OnUIClosed.Broadcast();
}

void UUIManagerSubsystem::EndCutscene()
{
    //[2026/09/17] 위젯이 아직 재생 중이면(ESC·콘솔) 먼저 끝낸다. Finish는 OnFinished로 이 함수를 다시 부르지만
    //그때는 위젯을 먼저 떼어 두었으므로 아래에서 아무것도 하지 않는다.
    if (UCutSceneWidget* Widget = Cast<UCutSceneWidget>(CutsceneWidget))
    {
        CutsceneWidget = nullptr;
        Widget->OnFinished.RemoveDynamic(this, &UUIManagerSubsystem::EndCutscene);
        Widget->Skip();               //이미 끝났으면 아무것도 하지 않는다.
        Widget->RemoveFromParent();   //NativeDestruct가 CallUIClosed를 한 번만 부른다.
    }
    else if (IsValid(CutsceneWidget))
    {
        CutsceneWidget->RemoveFromParent();
        CutsceneWidget = nullptr;
    }

    //다음 레벨은 한 번만 연다. 방송보다 먼저 비워 두어, 방송을 받은 쪽이 다시 EndCutscene을 불러도 두 번 열리지 않는다.
    const TSoftObjectPtr<UWorld> NextLevel = PendingLevelAfterCutscene;
    PendingLevelAfterCutscene.Reset();

    OnCutsceneEnded.Broadcast(ActiveCutscene);

    if (!NextLevel.IsNull())
    {
        UE_LOG(LogTemp, Display, TEXT("[UIManager] Cutscene %s ended; opening %s."),
            *UEnum::GetValueAsString(ActiveCutscene), *NextLevel.ToSoftObjectPath().ToString());
        UGameplayStatics::OpenLevelBySoftObjectPtr(this, NextLevel);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[UIManager] Cutscene %s ended."), *UEnum::GetValueAsString(ActiveCutscene));
    }
}

#pragma endregion



#pragma region Input

bool UUIManagerSubsystem::HandleEscapeKey()
{
    ULocalPlayer* LP = GetLocalPlayer();
    UWorld* World = LP ? LP->GetWorld() : nullptr;
    if (!World || !World->IsGameWorld())
        return false;

    //[2026/09/17] 컷씬 중 ESC는 스킵이다. 타이틀 위의 시작 컷씬도 같으므로 컨트롤러 종류보다 먼저 본다.
    //에디터에서는 아래와 같은 포커스 검사를 거친다.
    if (IsCutscenePlaying())
    {
        if (GIsEditor)
        {
            const UGameViewportClient* Viewport = World->GetGameViewport();
            const TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
            if (!ViewportWidget.IsValid() || !ViewportWidget->HasAnyUserFocusOrFocusedDescendants())
                return false;
        }

        UE_LOG(LogTemp, Display, TEXT("[UIManager] Escape: cutscene skipped."));
        EndCutscene();
        return true;
    }

    //타이틀(ATitleGameMode, 기본 APlayerController)처럼 그리드 플레이어 컨트롤러가 없는 레벨에는
    //재개·설정·가이드로 이뤄진 사이드 메뉴를 띄울 이유가 없다.
    APlayerController* PC = LP->GetPlayerController(World);
    if (!PC || !PC->IsA<AGridPlayerController>())
        return false;

    //컷씬 위젯이 떠 있는 동안은 여닫지 않는다(위에서 이미 스킵으로 처리했으므로 여기 오지 않는다).
    if (IsValid(CutsceneWidget))
        return false;

    //에디터에서는 PIE 뷰포트(또는 그 위에 올린 위젯)에 포커스가 있을 때만 받는다. 다른 패널에서 누른
    //ESC는 원래대로 에디터가 처리한다(이름 바꾸기 취소, PIE 정지 등).
    if (GIsEditor)
    {
        const UGameViewportClient* Viewport = World->GetGameViewport();
        const TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        if (!ViewportWidget.IsValid() || !ViewportWidget->HasAnyUserFocusOrFocusedDescendants())
            return false;
    }

    ToggleSideMenuUI();

    UE_LOG(LogTemp, Display, TEXT("[UIManager] Escape: side menu %s."), IsSideMenuOpen() ? TEXT("opened") : TEXT("closed"));
    return true;
}

#pragma endregion



#pragma region Stage

void UUIManagerSubsystem::BindStage(UStageSubsystem* Stage)
{
    if (!Stage)
        return;

    Stage->OnStageStateChanged.AddUniqueDynamic(this, &UUIManagerSubsystem::HandleStageStateChanged);
    BoundStage = Stage;

    //레벨이 바뀌었으니 이전 레벨에서 그린 번호와 진입 가이드 기록을 잊는다.
    LastPathIndex = INDEX_NONE;
    bStageEntryHandled = false;

    //이미 레벨 시작 판정이 끝난 뒤에 붙었다면 지금 상태로 바로 그린다. 판정 전이면 첫 방송을 기다린다.
    if (Stage->IsResolved())
    {
        HandleStageStateChanged(Stage->GetState());
    }
}

void UUIManagerSubsystem::UnbindStage()
{
    if (UStageSubsystem* Stage = BoundStage.Get())
    {
        Stage->OnStageStateChanged.RemoveDynamic(this, &UUIManagerSubsystem::HandleStageStateChanged);
    }

    BoundStage.Reset();
}

void UUIManagerSubsystem::HandleStageStateChanged(const FStageState& State)
{
    if (!State.bResolved)
        return;

    const UStageSubsystem* Stage = BoundStage.Get();
    const AStageInfo* Info = Stage ? Stage->GetStageInfo() : nullptr;

    //StageInfo가 없는 레벨(타이틀, 튜토리얼 등)은 PathUI를 건드리지 않는다.
    if (!Info)
        return;

    const int32 PathIndex = Info->GetPathUIIndex(State.CurrentZoneIndex);
    if (PathIndex >= 0 && PathIndex != LastPathIndex)
    {
        ShowOrRefreshPathUI(PathIndex);
    }

    if (!bStageEntryHandled)
    {
        bStageEntryHandled = true;

        if (!Info->EntryGuides.IsEmpty())
        {
            //레벨 시작 방송은 액터 BeginPlay 직후라 뷰포트·카메라가 아직 자리 잡기 전이다. 한 틱 미룬다.
            if (UWorld* World = Stage->GetWorld())
            {
                World->GetTimerManager().SetTimerForNextTick(
                    FTimerDelegate::CreateWeakLambda(this, [this]() { RequestEntryGuides(); }));
            }
        }
    }
}

void UUIManagerSubsystem::RequestEntryGuides()
{
    const UStageSubsystem* Stage = BoundStage.Get();
    const AStageInfo* Info = Stage ? Stage->GetStageInfo() : nullptr;
    UGuideDataSubsystem* Guide = UGuideDataSubsystem::Get(Stage);

    if (!Info || !Guide)
        return;

    for (const EGuideType Type : Info->EntryGuides)
    {
        Guide->RequestGuide(Type);
    }
}

#pragma endregion



#pragma region Settings

namespace
{
    UGameSoundSubsystem* GetSoundSubsystem(const ULocalPlayer* LP)
    {
        const UGameInstance* GI = LP ? LP->GetGameInstance() : nullptr;
        return GI ? GI->GetSubsystem<UGameSoundSubsystem>() : nullptr;
    }
}

float UUIManagerSubsystem::GetMasterVolume() const
{
    const UGameSoundSubsystem* Sound = GetSoundSubsystem(GetLocalPlayer());
    return Sound ? Sound->GetMasterVolume() : 1.0f;
}

void UUIManagerSubsystem::ApplyMasterVolume(float Volume01)
{
    //슬라이더 값 적용.
    if (UGameSoundSubsystem* Sound = GetSoundSubsystem(GetLocalPlayer()))
    {
        Sound->SetMasterVolume(Volume01);
    }
}

void UUIManagerSubsystem::CommitSettings()
{
    if (UGameSoundSubsystem* Sound = GetSoundSubsystem(GetLocalPlayer()))
    {
        Sound->SaveSettings();
    }
}

#pragma endregion
