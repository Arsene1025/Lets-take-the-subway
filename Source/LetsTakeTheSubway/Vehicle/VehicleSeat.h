// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 탈것에 올라탈 때 폰이 들어설 자리를 구한다.
 *
 * 좌석을 차체 중심으로 잡으면 문 앞에 선 폰이 차체 중심까지 대각선으로 미끄러져 들어간다.
 * 문으로 들어가는 것처럼 보이지 않고, 문이 여럿인 열차에서는 어느 문으로 탔는지도 사라진다.
 * 그래서 좌석은 **폰이 서 있는 자리를 차체의 문 면을 따라 그대로 옮긴 점**이다: 문 면과
 * 나란한 성분은 폰의 것을 쓰고, 문 면에 수직인 성분만 0(중심선)으로 만든다. 그러면 폰은
 * 자기가 선 자리에서 차체를 향해 **똑바로** 걸어 들어간다.
 */
namespace LTTSVehicle
{
	/**
	 * 문 면을 따라 폰을 마주 보는 좌석.
	 *
	 * @param RiderWorld       폰이 지금 서 있는 자리.
	 * @param BodyCentreWorld  차체 중심. XY만 쓴다.
	 * @param EdgeAxisWorld    문이 늘어선 축(월드). 열차는 진행축, 엘리베이터는 문 앞 셀이
	 *                         늘어선 축이다. 수평 성분만 쓰며 정규화는 여기서 한다.
	 * @param HalfExtent       그 축으로 좌석이 벗어날 수 있는 최대 거리(cm). 차체 밖에
	 *                         좌석이 생기지 않게 막는다.
	 * @param SeatZ            좌석 높이. 객차 바닥 + 폰의 HeightAboveFloor다.
	 */
	inline FVector SeatFacingRider(
		const FVector& RiderWorld,
		const FVector& BodyCentreWorld,
		const FVector& EdgeAxisWorld,
		double HalfExtent,
		double SeatZ)
	{
		const FVector Axis = FVector(EdgeAxisWorld.X, EdgeAxisWorld.Y, 0.0).GetSafeNormal();

		// 축을 못 읽으면 예전처럼 차체 중심에 앉힌다. 대각선으로 들어가긴 해도 폰이 허공에
		// 남는 것보다는 낫다.
		if (Axis.IsNearlyZero())
		{
			return FVector(BodyCentreWorld.X, BodyCentreWorld.Y, SeatZ);
		}

		const FVector Delta(RiderWorld.X - BodyCentreWorld.X, RiderWorld.Y - BodyCentreWorld.Y, 0.0);
		const double Along = FMath::Clamp(FVector::DotProduct(Delta, Axis), -HalfExtent, HalfExtent);

		const FVector Seat = BodyCentreWorld + Axis * Along;
		return FVector(Seat.X, Seat.Y, SeatZ);
	}
}
