// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 코드가 부르는 사운드 키의 정본.
 *
 * 키는 enum이 아니라 FName이다. 기획이 DA_SoundLibrary에 코드에 없는 키를 더하고 BP나
 * ASoundCellTrigger에서 그 키를 불러도 동작해야 하기 때문이다. 이 파일은 오타를 막는 상수일 뿐,
 * 쓸 수 있는 키의 목록을 제한하지 않는다.
 *
 * 키를 바꾸면 DA_SoundLibrary의 항목 이름과 액터에 저장된 키 프로퍼티도 함께 바꿔야 한다.
 * 설계와 파일 대응표는 Docs/Plans/SoundSystem.md 3.3절.
 */
namespace LTTSSoundKeys
{
	// ---------------------------------------------------------------- 인게임 세상 속 소리

	/** WAV_03. 열차가 화면에 들어오기 직전의 승강장 알림음. */
	inline const FName TrainNotification(TEXT("Train.Notification"));

	/** WAV_01. 열차 진입. 열차에 붙어 따라다닌다. */
	inline const FName TrainApproach(TEXT("Train.Approach"));

	/** WAV_02. 열차 서는 소리("고오오오 착"). */
	inline const FName TrainStop(TEXT("Train.Stop"));

	/** WAV_06 / WAV_07. 문 열림·닫힘. */
	inline const FName TrainDoorOpen(TEXT("Train.DoorOpen"));
	inline const FName TrainDoorClose(TEXT("Train.DoorClose"));

	/** WAV_05. 열차를 타고 이동하는 동안의 루프. */
	inline const FName TrainInside(TEXT("Train.Inside"));

	/** WAV_04(파일 없음). 하차 안내 방송. 파일이 오면 라이브러리만 채운다. */
	inline const FName TrainArrivalAnnouncement(TEXT("Train.ArrivalAnnouncement"));

	/** WAV_08. 엘리베이터 이동 루프. */
	inline const FName ElevatorMoving(TEXT("Elevator.Moving"));

	/** WAV_09. 엘리베이터 도착 알림. */
	inline const FName ElevatorArrived(TEXT("Elevator.Arrived"));

	/** WAV_10. 개찰구 카드 찍는 소리. ASoundCellTrigger의 기본 키. */
	inline const FName WorldTurnstile(TEXT("World.Turnstile"));

	// ---------------------------------------------------------------- 게임적 연출 소리

	/** WAV_11. 클릭. */
	inline const FName UIClick(TEXT("UI.Click"));

	/** WAV_12. 블록이 한 칸 밀릴 때. */
	inline const FName PuzzleSlide(TEXT("Puzzle.Slide"));

	/** WAV_13. 막힌 쪽으로 블록을 끌 때. */
	inline const FName PuzzleJam(TEXT("Puzzle.Jam"));

	/** WAV_14. 회전판이 공간을 돌릴 때. */
	inline const FName PuzzleTileRotate(TEXT("Puzzle.TileRotate"));

	/** WAV_15. 기둥·회전 장애물이 돌 때. */
	inline const FName PuzzlePillarRotate(TEXT("Puzzle.PillarRotate"));

	/** WAV_16. 기둥·회전 장애물이 걸려서 돌지 못할 때. */
	inline const FName PuzzlePillarJam(TEXT("Puzzle.PillarJam"));

	/** 파일 없음. 스테이지 클리어(목적지 도착). */
	inline const FName StageClear(TEXT("Stage.Clear"));
}
