// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/GridDebugDrawComponent.h"

#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"

#include "DynamicMeshBuilder.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "SceneView.h"
#include "ShowFlags.h"

void FGridLabelDrawHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController*)
{
	const FSceneView* View = Canvas ? Canvas->SceneView : nullptr;
	if (!View || GetTexts().IsEmpty())
	{
		return;
	}

	const FColor OldDrawColor = Canvas->DrawColor;
	UFont* Font = GEngine->GetSmallFont();
	const FFontRenderInfo FontInfo = Canvas->CreateFontRenderInfo(true, true);

	// 엔진 헬퍼가 쓰는 것과 같은 정규화 좌표 -> 캔버스 매핑이다. 그래야 DPI 스케일링과
	// 제한된 종횡비에서도 라벨이 제 셀 위에 놓인다.
	const float InvDPIScale = 1.0f / Canvas->GetDPIScale();
	const FIntRect& ViewRect = View->UnscaledViewRect;
	const float HalfWidth = ViewRect.Width() * 0.5f;
	const float HalfHeight = ViewRect.Height() * 0.5f;
	const FIntRect HalfDelta = (View->UnconstrainedViewRect - ViewRect) / 2;

	for (const FDebugRenderSceneProxy::FText3d& Label : GetTexts())
	{
		if (LabelMaxDistance > 0.0 && !FDebugRenderSceneProxy::PointInRange(Label.Location, View, LabelMaxDistance))
		{
			continue;
		}
		if (!FDebugRenderSceneProxy::PointInView(Label.Location, View))
		{
			continue;
		}

		const FVector4 Projected = View->Project(Label.Location);
		const float ScreenX = (HalfDelta.Width() + (1.0f + static_cast<float>(Projected.X)) * HalfWidth) * InvDPIScale;
		const float ScreenY = (HalfDelta.Height() + (1.0f - static_cast<float>(Projected.Y)) * HalfHeight) * InvDPIScale;

		Canvas->SetDrawColor(Label.Color);
		Canvas->DrawText(Font, Label.Text, ScreenX, ScreenY, 1.0f, 1.0f, FontInfo);
	}

	Canvas->SetDrawColor(OldDrawColor);
}

namespace
{
	FColor ColorForCellType(EGridCellType Type)
	{
		switch (Type)
		{
		case EGridCellType::Walkable:		return FColor(60, 200, 90);
		case EGridCellType::Blocked:		return FColor(220, 50, 50);
		case EGridCellType::StageClear:		return FColor(40, 200, 220);
		case EGridCellType::Conditional:	return FColor(235, 200, 40);
		case EGridCellType::NoFloor:		return FColor(120, 120, 120);
		default:							return FColor::White;
		}
	}

#if WITH_EDITOR
	/** 기본 디버그 프록시의 에디터 뷰포트 전용 변형. */
	class FGridDebugSceneProxy final : public FDebugRenderSceneProxy
	{
	public:
		explicit FGridDebugSceneProxy(const UPrimitiveComponent* InComponent)
			: FDebugRenderSceneProxy(InComponent)
		{
			DrawType = SolidAndWireMeshes;
			DrawAlpha = 90;
			ViewFlagName = TEXT("Editor");
			ViewFlagIndex = static_cast<uint32>(FEngineShowFlags::FindIndexByName(TEXT("Editor")));
			TextWithoutShadowDistance = 1500.0f;
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			// FDebugRenderSceneProxy는 이것을 구현하지 않고, 기본 relevance에는 다이나믹 패스가
			// 없으므로 오버라이드하지 않으면 아무것도 그려지지 않는다.
			const bool bVisible = View->Family->EngineShowFlags.GetSingleFlag(ViewFlagIndex) && IsShown(View);

			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = bVisible;
			Result.bDynamicRelevance = true;
			Result.bSeparateTranslucency = bVisible;
			Result.bNormalTranslucency = bVisible;
			return Result;
		}

		virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
	};

	/** 축 정렬 쿼드 하나를 메시 배치에 추가한다. */
	void AddCellQuad(FDebugRenderSceneProxy::FMesh& Mesh, const FVector& Centre, double HalfSize, double Z)
	{
		const uint32 BaseIndex = static_cast<uint32>(Mesh.Vertices.Num());

		const FVector Corners[4] = {
			FVector(Centre.X - HalfSize, Centre.Y - HalfSize, Z),
			FVector(Centre.X + HalfSize, Centre.Y - HalfSize, Z),
			FVector(Centre.X + HalfSize, Centre.Y + HalfSize, Z),
			FVector(Centre.X - HalfSize, Centre.Y + HalfSize, Z)
		};

		for (const FVector& Corner : Corners)
		{
			Mesh.Vertices.Emplace(FDynamicMeshVertex(FVector3f(Corner)));
			Mesh.Box += Corner;
		}

		Mesh.Indices.Append({ BaseIndex, BaseIndex + 1, BaseIndex + 2, BaseIndex, BaseIndex + 2, BaseIndex + 3 });
	}
#endif	// WITH_EDITOR
}

UGridDebugDrawComponent::UGridDebugDrawComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	bIsEditorOnly = true;
	bHiddenInGame = true;

	// 엔진은 이 세터를 WITH_EDITORONLY_DATA 안에 선언하므로, 가드 없이 호출하면 에디터
	// 타깃에서는 컴파일되지만 게임 타깃은 깨진다. 거기서 건너뛰어도 잃는 것은 없다: 이 세터가
	// 하는 일은 에디터 전용 데이터인 bIsVisualizationComponent를 켜는 것과, 바로 윗줄에서
	// 이미 설정한 bIsEditorOnly를 켜는 것뿐이다.
#if WITH_EDITORONLY_DATA
	SetIsVisualizationComponent(true);
#endif

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bSelectable = false;
	SetCastShadow(false);
}

FBoxSphereBounds UGridDebugDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const AGridActor* Grid = Cast<AGridActor>(GetOwner());
	if (!Grid)
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0f);
	}

	// 바운드가 영역 전체를 덮어야 한다. 그렇지 않으면 액터 원점이 화면 밖에 있을 때
	// 프록시가 컬링된다.
	const FVector Origin = Grid->GetGridOrigin();
	const FBox Region(
		Origin - FVector(0.0, 0.0, Grid->CellSize),
		Origin + FVector(Grid->SizeInCells.X * Grid->CellSize, Grid->SizeInCells.Y * Grid->CellSize, Grid->RegionHeight));

	return FBoxSphereBounds(Region);
}

FDebugRenderSceneProxy* UGridDebugDrawComponent::CreateDebugSceneProxy()
{
#if WITH_EDITOR
	// 아래에서 null을 반환하면 라벨 델리게이트가 이전 텍스트 목록을 든 채로 남으므로 항상
	// 빈 목록에서 시작한다. 프록시가 만들어지면 InitDelegateHelper가 다시 채운다.
	LabelHelper.ClearLabels();

	AGridActor* Grid = Cast<AGridActor>(GetOwner());
	if (!Grid || !Grid->bDrawGridInEditor || Grid->Cells.Num() == 0)
	{
		return nullptr;
	}

	const int32 Width = Grid->SizeInCells.X;
	const int32 Height = Grid->SizeInCells.Y;
	if (Grid->Cells.Num() != Width * Height)
	{
		return nullptr;
	}

	FGridDebugSceneProxy* Proxy = new FGridDebugSceneProxy(this);

	// Proxy->FarClippingDistance는 절대 설정하지 말 것: 셀 쿼드까지 잘린다. 라벨 거리는
	// 대신 헬퍼에 둔다.
	LabelHelper.LabelMaxDistance = Grid->bDrawCellCoords ? Grid->CoordLabelMaxDistance : 0.0;

	const double CellSize = Grid->CellSize;
	const double HalfSize = CellSize * 0.5 - 4.0;	// 셀 경계가 잘 보이도록 안쪽으로 줄인다

	// 셀 타입당 메시 배치 하나: FMesh는 색을 하나만 가지므로, 타입별로 묶으면 그리드 전체가
	// 셀마다 하나씩이 아니라 드로우 몇 개로 끝난다.
	TMap<EGridCellType, FDebugRenderSceneProxy::FMesh> MeshesByType;
	int32 LabelCount = 0;

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const FIntPoint Cell(X, Y);
			const FGridCellData& Data = Grid->Cells[Y * Width + X];

			if (Data.Type == EGridCellType::NoFloor && !Grid->bDrawNoFloorCells)
			{
				continue;
			}

			const FVector Centre = Grid->CellToWorld(Cell);
			const double DrawZ = Centre.Z + 2.0;

			FDebugRenderSceneProxy::FMesh& Mesh = MeshesByType.FindOrAdd(Data.Type);
			Mesh.Color = ColorForCellType(Data.Type);
			AddCellQuad(Mesh, Centre, HalfSize, DrawZ);

			if (Grid->bDrawCellCoords && LabelCount < Grid->CoordLabelMaxCount)
			{
				Proxy->Texts.Emplace(
					FString::Printf(TEXT("%d,%d"), X, Y),
					FVector(Centre.X, Centre.Y, DrawZ + 8.0),
					FLinearColor::White);
				++LabelCount;
			}

			// 단차 높이 규칙이 연결을 끊은 경계를 표시해, 너무 가파른 계단을 한눈에 볼 수 있게
			// 한다.
			if (Grid->bDrawStepBreaks && Data.Type != EGridCellType::NoFloor)
			{
				if (X + 1 < Width)
				{
					const FGridCellData& East = Grid->Cells[Y * Width + X + 1];
					if (East.Type != EGridCellType::NoFloor && (Data.NeighborMask & EGridDir::East) == 0)
					{
						const double EdgeX = Centre.X + CellSize * 0.5;
						Proxy->Lines.Emplace(
							FVector(EdgeX, Centre.Y - CellSize * 0.5, DrawZ + 4.0),
							FVector(EdgeX, Centre.Y + CellSize * 0.5, DrawZ + 4.0),
							FColor(255, 140, 0), 3.0f);
					}
				}

				if (Y + 1 < Height)
				{
					const FGridCellData& North = Grid->Cells[(Y + 1) * Width + X];
					if (North.Type != EGridCellType::NoFloor && (Data.NeighborMask & EGridDir::North) == 0)
					{
						const double EdgeY = Centre.Y + CellSize * 0.5;
						Proxy->Lines.Emplace(
							FVector(Centre.X - CellSize * 0.5, EdgeY, DrawZ + 4.0),
							FVector(Centre.X + CellSize * 0.5, EdgeY, DrawZ + 4.0),
							FColor(255, 140, 0), 3.0f);
					}
				}
			}
		}
	}

	for (TPair<EGridCellType, FDebugRenderSceneProxy::FMesh>& Pair : MeshesByType)
	{
		Proxy->Meshes.Add(MoveTemp(Pair.Value));
	}

	// 영역 외곽선.
	const FVector Origin = Grid->GetGridOrigin();
	const FBox RegionBox(
		Origin,
		Origin + FVector(Width * CellSize, Height * CellSize, Grid->RegionHeight));
	Proxy->Boxes.Emplace(RegionBox, FColor(200, 200, 255), FDebugRenderSceneProxy::WireMesh, 2.0f);

	return Proxy;
#else
	return nullptr;
#endif
}
