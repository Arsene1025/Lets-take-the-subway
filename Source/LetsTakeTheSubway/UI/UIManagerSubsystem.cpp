// Fill out your copyright notice in the Description page of Project Settings.


#include "UIManagerSubsystem.h"
#include "UISettings.h"
#include "UIInitializable.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Sound/GameSoundSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Guide/GuideDataSubsystem.h"
#include "Stage/StageInfo.h"
#include "Stage/StageSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"


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
}

void UUIManagerSubsystem::Deinitialize()
{
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

void UUIManagerSubsystem::PlayCutscene(int32 cutscene)
{
    ULocalPlayer* LP = GetLocalPlayer();
    if (!LP)
        return;

    UWorld* World = LP->GetWorld();
    if (!World)
        return;

    APlayerController* PC = LP->GetPlayerController(World);
    if (!PC)
        return;


    const UUISettings* Settings = GetDefault<UUISettings>();
    UClass* WidgetClass = Settings->CutsceneWidget.LoadSynchronous();
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[UIManager] 위젯 클래스가 지정되지 않았습니다. "));
        return;
    }

    CutsceneWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
    if (!CutsceneWidget)
        return;

    CutsceneWidget->AddToViewport(100);   // Z 순서 높게

    if (CutsceneWidget->GetClass()->ImplementsInterface(UUIInitializable::StaticClass()))
    {
        IUIInitializable::Execute_InitializeInt(CutsceneWidget, cutscene);
    }
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
    if (IsValid(CutsceneWidget))
    {
        CutsceneWidget->RemoveFromParent();
    }
    CutsceneWidget = nullptr;
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
