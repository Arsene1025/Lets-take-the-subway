// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/TitleGameMode.h"

#include "LetsTakeTheSubway.h"
#include "UI/UISettings.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ATitleGameMode::ATitleGameMode()
{
	// 폰 없음. 시점은 레벨의 고정 카메라가, 조작은 MainUI 위젯이 맡는다.
	//
	// bStartPlayersAsSpectators가 켜져 있으면 HandleStartingNewPlayer가 RestartPlayer를 부르지 않는다. 끄고
	// DefaultPawnClass만 비우면 엔진이 1초마다 폰 스폰을 다시 시도한다(FailedToSpawnPawn -> UnFreeze 루프).
	// SpectatorClass도 비워 두어야 관전 폰(날아다니는 기본 폰)이 스폰되지 않는다.
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;
	bStartPlayersAsSpectators = true;
	PlayerControllerClass = APlayerController::StaticClass();
}

void ATitleGameMode::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: no player controller; title camera and UI skipped."), *GetName());
		return;
	}

	LockViewToLevelCamera(PC);
	ShowMainUI(PC);
}

void ATitleGameMode::LockViewToLevelCamera(APlayerController* PC) const
{
	// 엔진도 ACameraActor::BeginPlay와 AutoManageActiveCameraTarget에서 같은 카메라를 고르지만, 액터 BeginPlay
	// 순서에 기대지 않고 여기서 한 번 더 고정한다. 타이틀 첫 화면에서 다른 시점이 한 프레임이라도 보이면 안 된다.
	ACameraActor* Camera = nullptr;
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		if (It->GetAutoActivatePlayerIndex() == 0)
		{
			Camera = *It;
			break;
		}
	}

	if (!Camera)
	{
		UE_LOG(LogLTTSGrid, Warning,
			TEXT("%s: this level has no CameraActor with AutoActivateForPlayer = Player0; the view stays at the player start."),
			*GetName());
		return;
	}

	PC->SetViewTarget(Camera);
	UE_LOG(LogLTTSGrid, Display, TEXT("%s: view locked to %s."), *GetName(), *Camera->GetActorNameOrLabel());
}

void ATitleGameMode::ShowMainUI(APlayerController* PC)
{
	const UUISettings* Settings = GetDefault<UUISettings>();
	UClass* WidgetClass = Settings ? Settings->MainUIWidget.LoadSynchronous() : nullptr;
	if (!WidgetClass)
	{
		UE_LOG(LogLTTSGrid, Error,
			TEXT("%s: UUISettings::MainUIWidget is not set (Project Settings > Game > UI Manager); title UI skipped."),
			*GetName());
		return;
	}

	MainUI = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (!MainUI)
	{
		UE_LOG(LogLTTSGrid, Error, TEXT("%s: failed to create %s."), *GetName(), *WidgetClass->GetName());
		return;
	}

	MainUI->AddToViewport();

	// 타이틀에는 게임 입력이 없다. 커서를 보이고 UI만 클릭되게 한다. 다음 레벨의 AGridPlayerController::BeginPlay가
	// GameAndUI로 되돌린다. 포커스 위젯은 주지 않는다: WBP_MainUI는 Is Focusable이 꺼져 있어 주면 엔진이
	// "Attempting to focus Non-Focusable widget" 오류를 남기고, 버튼 클릭에는 포커스가 필요 없다.
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);

	UE_LOG(LogLTTSGrid, Display, TEXT("%s: %s shown."), *GetName(), *WidgetClass->GetName());
}
