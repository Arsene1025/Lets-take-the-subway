// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

/**
 * 아트 메시가 머티리얼 없이 들어왔을 때 대신 씌울 것을 정하는 자리.
 *
 * 임포트된 메시는 슬롯이 비어 있으면 엔진의 기본 머티리얼(WorldGridMaterial, 회색 격자)로
 * 그려진다. 원본 스태틱 메시 액터는 컴포넌트 오버라이드로 프로젝트 툰 머티리얼을 덮어 쓰고
 * 있었지만, 그 메시를 코드가 만든 컴포넌트로 옮기면 오버라이드는 따라오지 않는다.
 */
namespace LTTSArt
{
	/**
	 * 엔진 기본 머티리얼이 들어 있는 슬롯만 Replacement로 바꾼다.
	 *
	 * 이미 무엇이든 지정된 슬롯은 건드리지 않는다. 아트가 일부 슬롯만 칠해 둔 메시에서
	 * 그 작업을 지워 버리지 않기 위해서다. 되돌릴 필요가 없도록 판정은 언제나 "지금 이
	 * 슬롯이 기본 머티리얼인가"이며, 이 함수를 여러 번 불러도 결과가 같다.
	 */
	inline void ReplaceDefaultMaterials(UMeshComponent& Component, UMaterialInterface* Replacement)
	{
		if (!Replacement)
		{
			return;
		}

		const UMaterialInterface* EngineDefault = UMaterial::GetDefaultMaterial(MD_Surface);

		const int32 NumSlots = Component.GetNumMaterials();
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			const UMaterialInterface* Current = Component.GetMaterial(Slot);
			if (!Current || Current == EngineDefault)
			{
				Component.SetMaterial(Slot, Replacement);
			}
		}
	}
}
