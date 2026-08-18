#include "Night/Course/NightBridgeSegmentActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

#pragma region K2 moonyfli
ANightBridgeSegmentActor::ANightBridgeSegmentActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ArtRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArtRoot"));
	SetRootComponent(ArtRoot);

	BridgeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BridgeMesh"));
	BridgeMesh->SetupAttachment(ArtRoot);
	BridgeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BridgeMesh->SetRelativeLocation(FVector(0.f, 0.f, 8.f));
	// The imported bridge's long axis is local Y; rotate it onto course forward.
	BridgeMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	BridgeMesh->SetRelativeScale3D(FVector::OneVector);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		BridgeMesh->SetStaticMesh(CubeMesh.Object);
	}

	// Prefer imported muban when present; falls back to cube above.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MubanA(
		TEXT("/Game/Night/Course/Art/Bridge/muban1.muban1"));
	if (MubanA.Succeeded())
	{
		BridgeMesh->SetStaticMesh(MubanA.Object);
		BridgeMesh->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));
	}
}

void ANightBridgeSegmentActor::ApplyMesh(UStaticMesh* Mesh)
{
	if (BridgeMesh && Mesh)
	{
		BridgeMesh->SetStaticMesh(Mesh);
	}
}

void ANightBridgeSegmentActor::SetupBridge(const FNightBridgeSpec& InSpec, UStaticMesh* MeshOverride)
{
	Spec = InSpec;
	if (MeshOverride)
	{
		ApplyMesh(MeshOverride);
	}
	SetActorLocationAndRotation(Spec.WorldLocation, FRotator(0.f, Spec.YawDeg, 0.f));
	if (BridgeMesh)
	{
		const FVector Base = FVector::OneVector;
		BridgeMesh->SetRelativeScale3D(FVector(
			Base.X,
			FMath::Max(0.05f, Spec.LengthScale) * Base.Y,
			Base.Z));
	}
}
#pragma endregion K2 moonyfli
