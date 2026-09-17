// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CutSceneWidget.generated.h"

class UButton;
class UCutSceneDatabase;
class UImage;
struct FStreamableHandle;

/** 컷씬이 끝나거나 스킵됐다. 한 번만 온다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCutSceneFinished);

/**
 * 그림 컷씬 위젯의 C++ 베이스(2026-09-17). WBP_CutScene이 이 클래스를 부모로 쓴다.
 *
 * 재생 논리(프레임 순서, 머무는 시간, 스킵, 프레임 사이 페이드)는 전부 여기 있고, 블루프린트는
 * 디자이너(배경 이미지 pictureImage, 스킵 버튼 skipBtn)만 갖는다. 블루프린트 Delay로 루프를 짜면
 * 스킵한 뒤에도 잠재 노드가 남아 EndCutscene이 두 번 불리는 문제가 있어 C++로 옮겼다
 * (Docs/Plans/GameLoop.md 1절).
 *
 * 흐름: UUIManagerSubsystem::PlayCutscene이 위젯을 만들고 Play(데이터 애셋)를 부른다.
 *   프레임마다  밝아짐(FrameFadeSeconds) → Duration 동안 머묾 → 어두워짐(FrameFadeSeconds)
 *   마지막 프레임이 어두워지면 Finish → OnFinished → 매니저가 위젯을 지우고 다음 레벨을 연다.
 * 스킵 버튼은 곧바로 Finish다.
 *
 * 페이드는 pictureImage의 ColorAndOpacity RGB를 검정 쪽으로 곱해서 낸다. 알파를 내리면 뒤의 게임
 * 화면이 비치므로 알파는 1로 둔다.
 *
 * 입력 차단: 생성될 때 CallUIOpened, 끝나거나 지워질 때 CallUIClosed(한 번만). 이전 블루프린트와
 * 같은 순서다. 컷씬 중에는 AGridPlayerController가 이동·드래그를 놓고, ESC는 사이드 메뉴 대신
 * 스킵이 된다(UUIManagerSubsystem::HandleEscapeKey).
 */
UCLASS(Abstract)
class LETSTAKETHESUBWAY_API UCutSceneWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 컷씬 그림. 디자이너의 ScaleBox 안 Image 위젯 이름이 이것과 같아야 한다(배경 bgImage는 검정 바탕이라 건드리지 않는다). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> pictureImage;

	/** 스킵 버튼. 디자이너의 Button 이름이 이것과 같아야 한다. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> skipBtn;

	/** 프레임이 밝아지고 어두워지는 시간(초). 0이면 곧바로 바뀐다. */
	UPROPERTY(EditDefaultsOnly, Category = "CutScene", meta = (ClampMin = 0.0, Units = "s"))
	float FrameFadeSeconds = 0.3f;

	/**
	 * 첫 프레임부터 재생한다. 프레임이 없으면 경고를 남기고 곧바로 끝낸다.
	 *
	 * 텍스처는 전부 비동기 로드를 걸어 두고 첫 장만 동기 로드한다. 프레임이 바뀌는 순간의 히치를 피한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "CutScene")
	void Play(const UCutSceneDatabase* Data);

	/** 남은 프레임을 건너뛰고 끝낸다. 스킵 버튼과 ESC가 부른다. */
	UFUNCTION(BlueprintCallable, Category = "CutScene")
	void Skip();

	UFUNCTION(BlueprintPure, Category = "CutScene")
	bool IsFinished() const { return bFinished; }

	UPROPERTY(BlueprintAssignable, Category = "CutScene")
	FOnCutSceneFinished OnFinished;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class EPhase : uint8
	{
		/** Play 전이거나 끝난 뒤. 틱이 아무것도 하지 않는다. */
		Idle,
		FadeIn,
		Hold,
		FadeOut
	};

	UFUNCTION()
	void HandleSkipClicked();

	/** 그 번호의 그림을 걸고 밝아지기 시작한다. */
	void ShowFrame(int32 Index);

	/** 어두워진 뒤: 다음 프레임이 있으면 걸고, 없으면 끝낸다. */
	void AdvanceFrame();

	/** 한 번만. 입력 차단을 풀고 OnFinished를 보낸다. 위젯을 지우는 것은 매니저다. */
	void Finish();

	/** 0 = 검정, 1 = 원래 색. */
	void ApplyBrightness(float Brightness);

	/** 지금 프레임이 머무는 시간(초). 음수는 0으로 본다. */
	float GetCurrentHoldSeconds() const;

	void CallUIOpenedOnce();
	void CallUIClosedOnce();

	UPROPERTY(Transient)
	TObjectPtr<const UCutSceneDatabase> Database;

	int32 FrameIndex = INDEX_NONE;

	EPhase Phase = EPhase::Idle;

	/** 현재 단계에 들어선 뒤 흐른 시간(초). */
	float PhaseElapsed = 0.0f;

	bool bFinished = false;
	bool bUIOpened = false;
	bool bUIClosed = false;

	/** 모든 프레임 텍스처의 비동기 로드 핸들. 위젯이 사라지면 함께 놓는다. */
	TSharedPtr<FStreamableHandle> PreloadHandle;
};
