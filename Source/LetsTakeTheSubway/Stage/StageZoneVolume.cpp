// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stage/StageZoneVolume.h"

#include "Grid/GridTypes.h"
#include "Stage/StageSubsystem.h"
#include "Stage/StageTypes.h"

#include "Components/BoxComponent.h"
#include "Engine/HitResult.h"

AStageZoneVolume::AStageZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);

	// 뷰포트에서 끌리지 않게 잠근다(Lock Actor Movement). 아트가 역 구조물을 박스 선택으로
	// 옮길 때 구역 볼륨이 같이 끌려가 좌표가 어긋난 사고가 있었다(2026-09-14 노선 연장).
	// 옮겨야 하면 액터 우클릭 > Transform > Lock Actor Movement를 끄거나 디테일 패널에 값을 넣는다.
	// 2026-09-15: 구역 조정 단계라 잠금을 잠시 끈다. 구역 배치가 확정되면 다시 켠다.
// #if WITH_EDITORONLY_DATA
// 	bLockLocation = true;
// #endif

	// 2 x 2 셀, 높이 3 m. 배치한 뒤 Box Extent로 구역에 맞춘다.
	Box->InitBoxExtent(FVector(100.0, 100.0, 150.0));

	// 순서가 중요하다: 오브젝트 타입과 전체 응답을 먼저 정하고, 예외인 폰 응답을 마지막에 연다.
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(LTTSStage::ZoneObjectChannel);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetGenerateOverlapEvents(true);
	Box->CanCharacterStepUpOn = ECB_No;
	Box->SetCanEverAffectNavigation(false);

	// 에디터에서만 보이는 청록 테두리. 퍼즐 구간(회색 막대)과 구분된다.
	Box->ShapeColor = FColor(0, 200, 255);
	Box->SetLineThickness(3.0f);
	Box->SetHiddenInGame(true);

	// Visibility를 무시하므로 그리드 생성 트레이스에 원래 안 걸리지만, 다른 채널로 굽는 그리드를
	// 대비해 퍼즐 구간과 같은 태그를 단다.
	Tags.Add(LTTSGrid::GenerationIgnoreTag());

	// 볼륨이 스트리밍으로 빠지면 플레이어가 그 구역에 서 있어도 구역을 잃는다.
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

bool AStageZoneVolume::ContainsPoint(const FVector& WorldLocation) const
{
	if (!Box)
	{
		return false;
	}

	// 역변환이 액터 스케일까지 되돌리므로, 스케일 전 크기와 비교하면 된다.
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Box->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y
		&& FMath::Abs(Local.Z) <= Extent.Z;
}

FString AStageZoneVolume::GetDisplayName() const
{
	return ZoneName.IsEmpty() ? GetName() : ZoneName.ToString();
}

void AStageZoneVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 생성자에서 바인딩하면 블루프린트 파생 클래스에 바인딩이 직렬화돼 두 번 불린다. BeginPlay에서
	// 바인딩하면 폰이 스폰되는 순간의 오버랩을 놓친다. 그 사이인 여기가 맞다.
	if (Box)
	{
		Box->OnComponentBeginOverlap.AddUniqueDynamic(this, &AStageZoneVolume::HandleBeginOverlap);
		Box->OnComponentEndOverlap.AddUniqueDynamic(this, &AStageZoneVolume::HandleEndOverlap);
	}
}

void AStageZoneVolume::BeginPlay()
{
	Super::BeginPlay();

	if (UStageSubsystem* Subsystem = UStageSubsystem::Get(this))
	{
		Subsystem->RegisterZone(this);
	}
}

void AStageZoneVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UStageSubsystem* Subsystem = UStageSubsystem::Get(this))
	{
		Subsystem->UnregisterZone(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AStageZoneVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 스폰 순간의 오버랩은 BeginPlay 전에 와서 서브시스템이 이 볼륨을 아직 모른다. 그래도 그대로
	// 넘긴다. 서브시스템은 레벨 시작 판정에서 점 판정으로 목록을 다시 만든다.
	if (UStageSubsystem* Subsystem = UStageSubsystem::Get(this))
	{
		Subsystem->NotifyZoneEntered(this, OtherActor);
	}
}

void AStageZoneVolume::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (UStageSubsystem* Subsystem = UStageSubsystem::Get(this))
	{
		Subsystem->NotifyZoneLeft(this, OtherActor);
	}
}
