// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/GridDebugDrawComponent.h"

#include "Grid/GridActor.h"
#include "Grid/GridTypes.h"

#include "DynamicMeshBuilder.h"
#include "SceneView.h"
#include "ShowFlags.h"

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
	/** Editor-viewport-only variant of the stock debug proxy. */
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
			// FDebugRenderSceneProxy does not implement this, and the default relevance has
			// no dynamic pass, so without the override nothing would draw at all.
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

	/** Appends one axis-aligned quad to a mesh batch. */
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
	SetIsVisualizationComponent(true);
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

	// Bounds must cover the whole region or the proxy gets culled while the actor's own
	// origin is off screen.
	const FVector Origin = Grid->GetGridOrigin();
	const FBox Region(
		Origin - FVector(0.0, 0.0, Grid->CellSize),
		Origin + FVector(Grid->SizeInCells.X * Grid->CellSize, Grid->SizeInCells.Y * Grid->CellSize, Grid->RegionHeight));

	return FBoxSphereBounds(Region);
}

FDebugRenderSceneProxy* UGridDebugDrawComponent::CreateDebugSceneProxy()
{
#if WITH_EDITOR
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
	Proxy->FarClippingDistance = Grid->bDrawCellCoords ? Grid->CoordLabelMaxDistance : 0.0;

	const double CellSize = Grid->CellSize;
	const double HalfSize = CellSize * 0.5 - 4.0;	// inset so cell borders stay readable

	// One mesh batch per cell type: FMesh carries a single colour, so grouping by type keeps
	// the whole grid down to a handful of draws instead of one per cell.
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

			// Mark the borders where the step-height rule severed a connection, so a stair
			// that is too steep is visible at a glance.
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

	// Region outline.
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
