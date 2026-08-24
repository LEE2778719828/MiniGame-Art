#include "Night/Course/NightCourseRoadsideActor.h"

#include "Components/ArrowComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

ANightRoadsideSegmentActor::ANightRoadsideSegmentActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RoadsideRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RoadsideRoot"));
	SetRootComponent(RoadsideRoot);

	StartMarker = CreateDefaultSubobject<UArrowComponent>(TEXT("StartMarker"));
	StartMarker->SetupAttachment(RoadsideRoot);
	StartMarker->ArrowColor = FColor::Green;
	StartMarker->ArrowSize = 1.5f;
	StartMarker->SetHiddenInGame(true);

	EndMarker = CreateDefaultSubobject<UArrowComponent>(TEXT("EndMarker"));
	EndMarker->SetupAttachment(RoadsideRoot);
	EndMarker->SetRelativeLocation(FVector(100.f, 0.f, 0.f));
	EndMarker->ArrowColor = FColor::Red;
	EndMarker->ArrowSize = 1.5f;
	EndMarker->SetHiddenInGame(true);
}

bool ANightRoadsideSegmentActor::GetRoadsideMarkerLocations(
	FVector& OutStart,
	FVector& OutEnd) const
{
	OutStart = StartMarker ? StartMarker->GetRelativeLocation() : FVector::ZeroVector;
	OutEnd = EndMarker ? EndMarker->GetRelativeLocation() : FVector::ZeroVector;
	return StartMarker != nullptr && EndMarker != nullptr;
}

float ANightRoadsideSegmentActor::GetRoadsideSpanCm() const
{
	FVector Start;
	FVector End;
	if (!GetRoadsideMarkerLocations(Start, End))
	{
		return 0.f;
	}
	return FVector::Distance(Start, End);
}

void ANightRoadsideSegmentActor::ApplyRoadsideCollisionPolicy()
{
	SetActorEnableCollision(false);

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents(PrimitiveComponents);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!Primitive)
		{
			continue;
		}
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Primitive->SetGenerateOverlapEvents(false);
	}
}

void ANightRoadsideSegmentActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyRoadsideCollisionPolicy();
}

void ANightRoadsideSegmentActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyRoadsideCollisionPolicy();
}

void ANightRoadsideSegmentActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyRoadsideCollisionPolicy();
}
