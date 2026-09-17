// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CutScene/CutSceneWidget.h"

#include "UI/CutScene/CutSceneDatabase.h"
#include "UI/UIManagerSubsystem.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"

void UCutSceneWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (skipBtn)
	{
		skipBtn->OnClicked.AddUniqueDynamic(this, &UCutSceneWidget::HandleSkipClicked);
	}

	CallUIOpenedOnce();
}

void UCutSceneWidget::NativeDestruct()
{
	if (skipBtn)
	{
		skipBtn->OnClicked.RemoveDynamic(this, &UCutSceneWidget::HandleSkipClicked);
	}

	if (PreloadHandle.IsValid())
	{
		PreloadHandle->ReleaseHandle();
		PreloadHandle.Reset();
	}

	// 맵 전환 등으로 Finish 없이 지워져도 입력 차단은 푼다.
	CallUIClosedOnce();

	Super::NativeDestruct();
}

void UCutSceneWidget::Play(const UCutSceneDatabase* Data)
{
	if (bFinished)
	{
		return;
	}

	Database = Data;

	if (!Database || Database->Frames.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CutScene] %s: no frames in %s; finishing immediately."),
			*GetName(), *GetNameSafe(Data));
		Finish();
		return;
	}

	if (!pictureImage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CutScene] %s: pictureImage is not bound; finishing immediately."), *GetName());
		Finish();
		return;
	}

	// 나머지 프레임을 미리 요청해 둔다. 첫 장은 ShowFrame이 동기 로드한다.
	TArray<FSoftObjectPath> Paths;
	for (const FCutsceneFrame& Frame : Database->Frames)
	{
		if (!Frame.Image.IsNull())
		{
			Paths.AddUnique(Frame.Image.ToSoftObjectPath());
		}
	}
	if (!Paths.IsEmpty())
	{
		PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
	}

	UE_LOG(LogTemp, Display, TEXT("[CutScene] %s: playing %s (%d frames, fade %.2f s)."),
		*GetName(), *Database->GetName(), Database->Frames.Num(), FrameFadeSeconds);

	ShowFrame(0);
}

void UCutSceneWidget::Skip()
{
	if (bFinished)
	{
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("[CutScene] %s: skipped at frame %d."), *GetName(), FrameIndex);
	Finish();
}

void UCutSceneWidget::HandleSkipClicked()
{
	Skip();
}

void UCutSceneWidget::ShowFrame(int32 Index)
{
	FrameIndex = Index;
	const FCutsceneFrame& Frame = Database->Frames[Index];

	if (Frame.Image.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CutScene] %s: frame %d has no image."), *GetName(), Index);
	}
	else
	{
		// 미리 요청해 두었으므로 대개 이미 메모리에 있다. 아니면 여기서 잠깐 기다린다.
		// bMatchSize = true: 브러시 크기를 텍스처 크기(2560x1440)로 맞춘다. 디자이너의 기본 브러시(32x32)를 그대로 두면
		// 바깥 ScaleBox가 정사각형으로 늘려 그림이 찌그러진다(2026-09-17 PIE에서 확인).
		if (UTexture2D* Texture = Frame.Image.LoadSynchronous())
		{
			pictureImage->SetBrushFromTexture(Texture, true);
		}
	}

	Phase = FrameFadeSeconds > 0.0f ? EPhase::FadeIn : EPhase::Hold;
	PhaseElapsed = 0.0f;
	ApplyBrightness(Phase == EPhase::FadeIn ? 0.0f : 1.0f);
}

void UCutSceneWidget::AdvanceFrame()
{
	const int32 Next = FrameIndex + 1;
	if (Database && Database->Frames.IsValidIndex(Next))
	{
		ShowFrame(Next);
	}
	else
	{
		Finish();
	}
}

void UCutSceneWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bFinished || Phase == EPhase::Idle || !Database)
	{
		return;
	}

	PhaseElapsed += InDeltaTime;

	switch (Phase)
	{
	case EPhase::FadeIn:
	{
		const float T = FMath::Clamp(PhaseElapsed / FrameFadeSeconds, 0.0f, 1.0f);
		ApplyBrightness(T);
		if (T >= 1.0f)
		{
			Phase = EPhase::Hold;
			PhaseElapsed = 0.0f;
		}
		break;
	}

	case EPhase::Hold:
		if (PhaseElapsed >= GetCurrentHoldSeconds())
		{
			PhaseElapsed = 0.0f;
			if (FrameFadeSeconds > 0.0f)
			{
				Phase = EPhase::FadeOut;
			}
			else
			{
				AdvanceFrame();
			}
		}
		break;

	case EPhase::FadeOut:
	{
		const float T = FMath::Clamp(PhaseElapsed / FrameFadeSeconds, 0.0f, 1.0f);
		ApplyBrightness(1.0f - T);
		if (T >= 1.0f)
		{
			AdvanceFrame();
		}
		break;
	}

	default:
		break;
	}
}

void UCutSceneWidget::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	Phase = EPhase::Idle;

	CallUIClosedOnce();
	OnFinished.Broadcast();
}

void UCutSceneWidget::ApplyBrightness(float Brightness)
{
	if (pictureImage)
	{
		pictureImage->SetColorAndOpacity(FLinearColor(Brightness, Brightness, Brightness, 1.0f));
	}
}

float UCutSceneWidget::GetCurrentHoldSeconds() const
{
	if (!Database || !Database->Frames.IsValidIndex(FrameIndex))
	{
		return 0.0f;
	}
	return FMath::Max(Database->Frames[FrameIndex].Duration, 0.0f);
}

void UCutSceneWidget::CallUIOpenedOnce()
{
	if (bUIOpened)
	{
		return;
	}
	bUIOpened = true;

	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (UUIManagerSubsystem* UI = LP ? LP->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		UI->CallUIOpened();
	}
}

void UCutSceneWidget::CallUIClosedOnce()
{
	if (!bUIOpened || bUIClosed)
	{
		return;
	}
	bUIClosed = true;

	ULocalPlayer* LP = GetOwningLocalPlayer();
	if (UUIManagerSubsystem* UI = LP ? LP->GetSubsystem<UUIManagerSubsystem>() : nullptr)
	{
		UI->CallUIClosed();
	}
}
